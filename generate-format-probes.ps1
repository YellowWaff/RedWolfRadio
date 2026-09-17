param([string]$Python = '')
$ErrorActionPreference = 'Stop'
if (!$Python) {
    $candidate = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $candidate) { $Python = $candidate }
}
if (!$Python) {
    $command = Get-Command python -ErrorAction SilentlyContinue
    if ($command -and $command.Source -and $command.Source -notlike '*\WindowsApps\python.exe') { $Python = $command.Source }
}
if (!$Python -or !(Test-Path -LiteralPath $Python)) {
    throw 'Python 3.10+ is required to generate the controlled FLAC and MP3 fixtures.'
}
$packages = Join-Path $PSScriptRoot 'tools\audio-python'
$env:PYTHONPATH = $packages
& $Python -c 'import soundfile; assert soundfile.__version__ == "0.14.0"'
if ($LASTEXITCODE) {
    throw "Pinned test dependency missing. Install soundfile 0.14.0 into $packages before generating fixtures."
}
& $Python (Join-Path $PSScriptRoot 'utilities\generate_format_probes.py') (Join-Path $PSScriptRoot 'build')
if ($LASTEXITCODE) { throw 'Format fixture generation failed.' }
$expected = @{
    'MusicTrace-descending.flac' = '114F76A17ECBFCDD55F018EFC73B08D59B1914EEDA61A38B0F9343511136AAA6'
    'MusicTrace-alternating.mp3' = '2D264C0845A28872D180A06C50B1FB592F067096FC5E36F9C85ABC817D2F5695'
}
foreach ($entry in $expected.GetEnumerator()) {
    $path = Join-Path $PSScriptRoot "build\$($entry.Key)"
    if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.Value) {
        throw "Generated fixture hash mismatch: $($entry.Key)"
    }
}
Get-FileHash -LiteralPath ($expected.Keys | ForEach-Object { Join-Path $PSScriptRoot "build\$_" }) -Algorithm SHA256
