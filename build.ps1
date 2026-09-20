param([string]$Toolchain = "$PSScriptRoot\tools\llvm-mingw-20260908-ucrt-x86_64")
$ErrorActionPreference = 'Stop'
$compiler = Join-Path $Toolchain 'bin\x86_64-w64-mingw32-clang++.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw "Missing portable compiler: $compiler" }
$out = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Path $out -Force | Out-Null
& $compiler '-std=c++17' '-O2' '-Wall' '-Wextra' '-Werror' '-static' `
    (Join-Path $PSScriptRoot 'utilities\generate_probe_wav.cpp') '-o' (Join-Path $out 'generate-probe-wav.exe')
if ($LASTEXITCODE) { throw 'Probe WAV generator build failed' }
& (Join-Path $out 'generate-probe-wav.exe') (Join-Path $out 'MusicTrace-probe.wav')
if ($LASTEXITCODE) { throw 'Probe WAV generation failed' }
& (Join-Path $PSScriptRoot 'generate-format-probes.ps1')
if ($LASTEXITCODE) { throw 'FLAC/MP3 probe generation failed' }
$miniaudioObject = Join-Path $out 'miniaudio_impl.o'
$converterObject = Join-Path $out 'audio_converter.o'
& $compiler '-std=c++17' '-O2' '-w' '-c' `
    (Join-Path $PSScriptRoot 'src\miniaudio_impl.cpp') '-o' $miniaudioObject
if ($LASTEXITCODE) { throw 'Miniaudio implementation build failed' }
& $compiler '-std=c++17' '-O2' '-Wall' '-Wextra' '-Werror' '-c' `
    (Join-Path $PSScriptRoot 'src\audio_converter.cpp') '-o' $converterObject
if ($LASTEXITCODE) { throw 'Audio converter build failed' }
& $compiler '-std=c++17' '-O2' '-Wall' '-Wextra' '-Werror' '-static' '-municode' `
    (Join-Path $PSScriptRoot 'tests\converter.cpp') $converterObject $miniaudioObject `
    '-lbcrypt' '-o' (Join-Path $out 'converter-tests.exe')
if ($LASTEXITCODE) { throw 'Audio converter test build failed' }
& (Join-Path $out 'converter-tests.exe') `
    (Join-Path $out 'MusicTrace-probe.wav') `
    (Join-Path $out 'MusicTrace-descending.flac') `
    (Join-Path $out 'MusicTrace-alternating.mp3')
if ($LASTEXITCODE) { throw 'Audio converter tests failed' }
& $compiler '-std=c++17' '-O2' '-Wall' '-Wextra' '-Werror' '-shared' '-static' '-Wl,--no-insert-timestamp' `
    (Join-Path $PSScriptRoot 'src\music_trace.cpp') $converterObject $miniaudioObject `
    '-lbcrypt' '-lole32' '-lshell32' '-luser32' '-o' (Join-Path $out 'RedWolfRadio.dll')
if ($LASTEXITCODE) { throw 'Red Wolf Radio build failed' }
& $compiler '-std=c++17' '-O2' '-Wall' '-Wextra' '-Werror' '-static' `
    (Join-Path $PSScriptRoot 'tests\forwarding.cpp') $converterObject $miniaudioObject `
    '-lbcrypt' '-luser32' '-o' (Join-Path $out 'forwarding-tests.exe')
if ($LASTEXITCODE) { throw 'Forwarding test build failed' }
& (Join-Path $out 'forwarding-tests.exe')
if ($LASTEXITCODE) { throw 'Forwarding tests failed' }
foreach ($test in @('hotkeys', 'music_voice_control', 'music_controls')) {
    & $compiler '-std=c++17' '-O2' '-Wall' '-Wextra' '-Werror' '-static' `
        (Join-Path $PSScriptRoot "tests\$test.cpp") $converterObject $miniaudioObject `
        '-lbcrypt' '-luser32' '-o' (Join-Path $out "$test-tests.exe")
    if ($LASTEXITCODE) { throw "$test test build failed" }
    & (Join-Path $out "$test-tests.exe")
    if ($LASTEXITCODE) { throw "$test tests failed" }
}
& (Join-Path $Toolchain 'bin\llvm-readobj.exe') '--file-headers' '--coff-exports' '--coff-imports' (Join-Path $out 'RedWolfRadio.dll') |
    Out-File -LiteralPath (Join-Path $out 'RedWolfRadio-pe.txt') -Encoding utf8
Get-FileHash -LiteralPath (Join-Path $out 'RedWolfRadio.dll') -Algorithm SHA256
Get-FileHash -LiteralPath (Join-Path $out 'MusicTrace-probe.wav') -Algorithm SHA256
