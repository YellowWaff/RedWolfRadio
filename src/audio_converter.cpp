// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "audio_converter.h"
#include "../third_party/miniaudio/miniaudio.h"

namespace {
constexpr uint32_t kRate = 48000;
constexpr uint16_t kChannels = 2;
constexpr uint16_t kBits = 16;
constexpr char kProfile[] = "red-wolf-radio-cache-v1:s16:2:48000:miniaudio-0.11.25";

uint16_t le16(const unsigned char* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
uint32_t le32(const unsigned char* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

bool hashSource(const std::wstring& path, std::string& hex, const char* profile = kProfile) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    unsigned char buffer[64 * 1024];
    while (ok) {
        DWORD read = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) { ok = false; break; }
        if (!read) break;
        ok = BCryptHashData(hash, buffer, read, 0) >= 0;
    }
    if (ok) ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(profile)),
        static_cast<ULONG>(strlen(profile)), 0) >= 0;
    unsigned char digest[32]{};
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    if (!ok) return false;
    static const char digits[] = "0123456789abcdef";
    hex.resize(64);
    for (size_t i = 0; i < sizeof(digest); ++i) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 15];
    }
    return true;
}

} // namespace

// The pinned game's file resolver copies into a 256-byte narrow buffer. Use
// ASCII routes only; Unicode/long sources get an unchanged compressed copy.
bool enginePathSafe(const std::wstring& path) {
    if (path.empty() || path.size() >= 256) return false;
    for (wchar_t c : path) if (c < 32 || c > 126) return false;
    return true;
}

bool resolveEnginePath(const std::wstring& path, std::wstring& routed) {
    routed.clear();
    if (enginePathSafe(path)) { routed = path; return true; }
    // Preserve the actual owned path for cache accounting; use a DOS alias only
    // at the narrow engine boundary when the filesystem provides one.
    std::vector<wchar_t> shortPath(32768, 0);
    const DWORD size = GetShortPathNameW(path.c_str(), shortPath.data(), static_cast<DWORD>(shortPath.size()));
    if (!size || size >= shortPath.size()) return false;
    routed.assign(shortPath.data(), size);
    if (!enginePathSafe(routed)) { routed.clear(); return false; }
    return true;
}

bool validateXwma(const std::wstring& path, XwmaInfo* info) {
    if (info) *info = {};
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER length{};
    auto readAt = [&](uint64_t offset, void* buffer, DWORD size) {
        LARGE_INTEGER position{}; position.QuadPart = static_cast<LONGLONG>(offset);
        DWORD read = 0;
        return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) &&
            ReadFile(file, buffer, size, &read, nullptr) && read == size;
    };
    unsigned char header[12]{};
    bool ok = GetFileSizeEx(file, &length) && length.QuadPart >= 12 &&
        readAt(0, header, sizeof(header)) && !memcmp(header, "RIFF", 4) &&
        !memcmp(header + 8, "XWMA", 4) &&
        static_cast<uint64_t>(le32(header + 4)) + 8 == static_cast<uint64_t>(length.QuadPart);
    bool formatSeen = false, dataSeen = false, packetsSeen = false;
    uint32_t block = 0, dataBytes = 0;
    XwmaInfo parsed{};
    uint64_t packetOffset = 0;
    for (uint64_t offset = 12; ok && offset < static_cast<uint64_t>(length.QuadPart);) {
        unsigned char chunk[8]{};
        if (static_cast<uint64_t>(length.QuadPart) - offset < 8 || !readAt(offset, chunk, 8)) { ok = false; break; }
        const uint32_t size = le32(chunk + 4);
        const uint64_t end = offset + 8 + size + (size & 1u);
        if (end > static_cast<uint64_t>(length.QuadPart)) { ok = false; break; }
        if (!memcmp(chunk, "fmt ", 4)) {
            unsigned char fmt[18]{};
            if (formatSeen || size != sizeof(fmt) || !readAt(offset + 8, fmt, sizeof(fmt))) { ok = false; break; }
            formatSeen = true;
            parsed.channels = le16(fmt + 2); parsed.sampleRate = le32(fmt + 4);
            block = le16(fmt + 12);
            // WMAudio2 is the profile present in the verified game's music.
            // Other WMA profiles need their own engine acceptance test.
            ok = le16(fmt) == 0x161 && (parsed.channels == 1 || parsed.channels == 2) &&
                parsed.sampleRate >= 8000 && parsed.sampleRate <= 48000 &&
                le32(fmt + 8) != 0 && block != 0 && le16(fmt + 14) == 16 && le16(fmt + 16) == 0;
        } else if (!memcmp(chunk, "dpds", 4)) {
            if (packetsSeen || !size || size % 4) { ok = false; break; }
            packetsSeen = true; parsed.packetCount = size / 4; packetOffset = offset + 8;
        } else if (!memcmp(chunk, "data", 4)) {
            if (dataSeen || !size) { ok = false; break; }
            dataSeen = true; dataBytes = size;
        }
        offset = end;
    }
    ok = ok && formatSeen && dataSeen && packetsSeen && block &&
        dataBytes % block == 0 && dataBytes / block == parsed.packetCount;
    // Stream the packet table rather than allocating according to untrusted sizes.
    unsigned char table[4096];
    for (uint32_t index = 0; ok && index < parsed.packetCount;) {
        const uint32_t count = (parsed.packetCount - index < 1024) ? parsed.packetCount - index : 1024;
        if (!readAt(packetOffset + static_cast<uint64_t>(index) * 4, table, count * 4)) { ok = false; break; }
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t decoded = le32(table + i * 4);
            // A padding/final compressed packet can produce no additional PCM.
            // Two shipped game tracks repeat the final cumulative byte count.
            if (decoded < parsed.decodedBytes || decoded % (parsed.channels * 2)) { ok = false; break; }
            parsed.decodedBytes = decoded;
        }
        index += count;
    }
    ok = ok && parsed.decodedBytes != 0;
    CloseHandle(file);
    if (ok && info) *info = parsed;
    return ok;
}

bool prepareNativeXwma(const std::wstring& source, const std::wstring& cacheDirectory,
                       NativeAudioResult& result, std::string& error) {
    result = {}; error.clear();
    if (!validateXwma(source)) { error = "invalid or unsupported XWMA (requires RIFF XWMA/WMAudio2)"; return false; }
    if (enginePathSafe(source)) { result.path = source; return true; }
    if (cacheDirectory.empty()) { error = "no cache directory for safe XWMA route"; return false; }
    std::string digest;
    if (!hashSource(source, digest, "red-wolf-radio-native-xwma-v1")) { error = "could not hash XWMA"; return false; }
    std::wstring destination = cacheDirectory;
    if (destination.back() != L'\\' && destination.back() != L'/') destination += L'\\';
    destination.append(digest.begin(), digest.end()); destination += L".xwma";
    std::wstring routedDirectory;
    if (!resolveEnginePath(cacheDirectory, routedDirectory) ||
        !enginePathSafe(routedDirectory + L"\\" + std::wstring(digest.begin(), digest.end()) + L".xwma")) {
        error = "cache path has no safe narrow engine route (use an ASCII Windows profile/cache path)"; return false;
    }
    std::string existingDigest;
    if (validateXwma(destination) && hashSource(destination, existingDigest, "red-wolf-radio-native-xwma-v1") && existingDigest == digest) {
        result = {destination, true, true}; return true;
    }
    wchar_t suffix[80]{};
    swprintf(suffix, std::size(suffix), L".tmp-%lu-%lu", GetCurrentProcessId(), GetCurrentThreadId());
    const std::wstring temporary = destination + suffix;
    DeleteFileW(temporary.c_str());
    bool ok = CopyFileW(source.c_str(), temporary.c_str(), TRUE) && validateXwma(temporary) &&
        hashSource(temporary, existingDigest, "red-wolf-radio-native-xwma-v1") && existingDigest == digest &&
        MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) { DeleteFileW(temporary.c_str()); error = "could not publish unchanged XWMA cache copy"; return false; }
    result = {destination, true, false}; return true;
}

namespace {
std::string resultText(ma_result value) {
    char text[64]{};
    snprintf(text, sizeof(text), "miniaudio error %d", static_cast<int>(value));
    return text;
}
}

bool validateCanonicalWav(const std::wstring& path, uint64_t* frames) {
    if (frames) *frames = 0;
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER total{};
    unsigned char header[12]{};
    DWORD read = 0;
    bool ok = GetFileSizeEx(file, &total) && total.QuadPart >= 44 &&
        ReadFile(file, header, sizeof(header), &read, nullptr) && read == sizeof(header) &&
        memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0;
    bool validFormat = false;
    uint64_t dataBytes = 0;
    while (ok) {
        unsigned char chunk[8]{};
        if (!ReadFile(file, chunk, sizeof(chunk), &read, nullptr) || read == 0) break;
        if (read != sizeof(chunk)) { ok = false; break; }
        const uint32_t size = le32(chunk + 4);
        LARGE_INTEGER remaining{};
        LARGE_INTEGER current{};
        if (!SetFilePointerEx(file, {}, &current, FILE_CURRENT)) { ok = false; break; }
        remaining.QuadPart = total.QuadPart - current.QuadPart;
        if (remaining.QuadPart < size) { ok = false; break; }
        if (memcmp(chunk, "fmt ", 4) == 0) {
            if (size < 16) { ok = false; break; }
            unsigned char format[16]{};
            if (!ReadFile(file, format, sizeof(format), &read, nullptr) || read != sizeof(format)) { ok = false; break; }
            validFormat = le16(format) == 1 && le16(format + 2) == kChannels &&
                le32(format + 4) == kRate && le16(format + 14) == kBits;
            LARGE_INTEGER skip{}; skip.QuadPart = static_cast<LONGLONG>(size - 16) + (size & 1u);
            if (!SetFilePointerEx(file, skip, nullptr, FILE_CURRENT)) { ok = false; break; }
        } else {
            if (memcmp(chunk, "data", 4) == 0) dataBytes = size;
            LARGE_INTEGER skip{}; skip.QuadPart = static_cast<LONGLONG>(size) + (size & 1u);
            if (!SetFilePointerEx(file, skip, nullptr, FILE_CURRENT)) { ok = false; break; }
        }
    }
    CloseHandle(file);
    if (!ok || !validFormat || !dataBytes || (dataBytes % (kChannels * (kBits / 8))) != 0) return false;
    if (frames) *frames = dataBytes / (kChannels * (kBits / 8));
    return true;
}

bool convertToCanonicalCache(const std::wstring& source,
                             const std::wstring& cacheDirectory,
                             CanonicalAudioResult& result,
                             std::string& error) {
    result = {};
    error.clear();
    std::string digest;
    if (!hashSource(source, digest)) { error = "could not hash input"; return false; }
    std::wstring destination = cacheDirectory;
    if (!destination.empty() && destination.back() != L'\\' && destination.back() != L'/') destination.push_back(L'\\');
    for (char c : digest) destination.push_back(static_cast<wchar_t>(c));
    destination += L".wav";
    uint64_t cachedFrames = 0;
    if (validateCanonicalWav(destination, &cachedFrames)) {
        result.path = destination; result.frames = cachedFrames; result.cacheHit = true; return true;
    }

    wchar_t suffix[80]{};
    swprintf(suffix, std::size(suffix), L".tmp-%lu-%lu", GetCurrentProcessId(), GetCurrentThreadId());
    const std::wstring temporary = destination + suffix;
    DeleteFileW(temporary.c_str());

    ma_decoder decoder{};
    const ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_s16, kChannels, kRate);
    ma_result status = ma_decoder_init_file_w(source.c_str(), &decoderConfig, &decoder);
    if (status != MA_SUCCESS) { error = resultText(status); return false; }
    ma_encoder encoder{};
    const ma_encoder_config encoderConfig = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, kChannels, kRate);
    status = ma_encoder_init_file_w(temporary.c_str(), &encoderConfig, &encoder);
    if (status != MA_SUCCESS) { ma_decoder_uninit(&decoder); error = resultText(status); return false; }

    constexpr ma_uint64 batchFrames = 4096;
    std::vector<int16_t> samples(static_cast<size_t>(batchFrames) * kChannels);
    uint64_t totalFrames = 0;
    bool ok = true;
    while (ok) {
        ma_uint64 decoded = 0;
        status = ma_decoder_read_pcm_frames(&decoder, samples.data(), batchFrames, &decoded);
        if (decoded) {
            ma_uint64 encoded = 0;
            const ma_result writeStatus = ma_encoder_write_pcm_frames(&encoder, samples.data(), decoded, &encoded);
            if (writeStatus != MA_SUCCESS || encoded != decoded) { error = resultText(writeStatus); ok = false; break; }
            totalFrames += encoded;
            if (totalFrames > (UINT32_MAX - 44u) / (kChannels * (kBits / 8))) {
                error = "decoded audio exceeds RIFF WAV size limit"; ok = false; break;
            }
        }
        if (status == MA_AT_END || decoded == 0) break;
        if (status != MA_SUCCESS) { error = resultText(status); ok = false; break; }
    }
    ma_encoder_uninit(&encoder);
    ma_decoder_uninit(&decoder);
    uint64_t verifiedFrames = 0;
    if (ok && totalFrames && validateCanonicalWav(temporary, &verifiedFrames) && verifiedFrames == totalFrames) {
        ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!ok) error = "could not publish cache file";
    } else if (ok) {
        error = "converted WAV validation failed"; ok = false;
    }
    if (!ok) { DeleteFileW(temporary.c_str()); return false; }
    result.path = destination;
    result.frames = totalFrames;
    result.cacheHit = false;
    return true;
}
