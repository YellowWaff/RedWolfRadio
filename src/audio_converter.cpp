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

bool hashSource(const std::wstring& path, std::string& hex) {
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
    if (ok) ok = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(kProfile)),
        static_cast<ULONG>(strlen(kProfile)), 0) >= 0;
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
