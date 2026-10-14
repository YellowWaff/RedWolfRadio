param(
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]*$')]
    [string]$Version = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'VERSION') -Raw).Trim(),
    [string]$DllPath = (Join-Path $PSScriptRoot 'build\RedWolfRadio.dll')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($Version)) {
    throw 'Version cannot be blank.'
}
if (!(Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "Missing plugin DLL: $DllPath. Run build.ps1 first or pass -DllPath."
}
$sourceVersion = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'VERSION') -Raw).Trim()
if ($Version -cne $sourceVersion) {
    throw "Package version $Version does not match VERSION ($sourceVersion). Update and build the source before packaging."
}
$dllText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($DllPath))
if ($dllText -cnotmatch ('(?:^|\x00)' + [regex]::Escape($Version) + '\x00')) {
    throw "The DLL does not contain the exact version $Version. Run build.ps1 before packaging."
}

$playerScript = Join-Path $PSScriptRoot 'distribution\rwr_now_playing.txt'
$playerRegistration = Join-Path $PSScriptRoot 'distribution\workshop-script-entry.txt'
$playerIcon = Join-Path $PSScriptRoot 'previewimage.png'
$expectedRegistration = '$OBJECT_SCRIPT rwr_now_playing "Red Wolf Radio: Now playing"'
foreach ($required in @($playerScript, $playerRegistration, $playerIcon)) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing player package input: $required" }
}
if ((Get-Content -LiteralPath $playerRegistration -Raw).Trim() -cne $expectedRegistration) {
    throw 'The player registration fragment must contain only the expected OBJECT_SCRIPT entry, without item IDs or END.'
}
if ((Get-Content -LiteralPath $playerScript -Raw) -notmatch '\[RWR_NOW_PLAYING_V1\]') {
    throw 'The player script is missing its native-window bridge marker.'
}

function Copy-PlayerCompanion([string]$PackageRoot) {
    Copy-Item -LiteralPath $playerScript -Destination (Join-Path $PackageRoot 'rwr_now_playing.txt')
    Copy-Item -LiteralPath $playerIcon -Destination (Join-Path $PackageRoot 'rwr_now_playing.png')
    Copy-Item -LiteralPath $playerRegistration -Destination (Join-Path $PackageRoot 'workshop-script-entry.txt')
}

function Assert-PackageAndWriteHashes([string]$PackageRoot, [string]$DllRelativePath, [string]$IniRelativePath) {
    $runtimeFiles = @($DllRelativePath, $IniRelativePath, 'rwr_now_playing.txt', 'rwr_now_playing.png', 'workshop-script-entry.txt', 'previewimage.png')
    foreach ($relative in $runtimeFiles) {
        if (!(Test-Path -LiteralPath (Join-Path $PackageRoot $relative) -PathType Leaf)) {
            throw "Package is missing required runtime file: $relative"
        }
    }
    if (Test-Path -LiteralPath (Join-Path $PackageRoot 'workshopconfig.ini')) {
        throw 'Packages must not replace registered Workshop metadata. Merge workshop-script-entry.txt into the existing metadata.'
    }
    $expectedFiles = @{
        $DllRelativePath = $DllPath
        $IniRelativePath = (Join-Path $PSScriptRoot 'RedWolfRadio.ini')
        'rwr_now_playing.txt' = $playerScript
        'rwr_now_playing.png' = $playerIcon
        'workshop-script-entry.txt' = $playerRegistration
        'previewimage.png' = $playerIcon
    }
    $manifest = foreach ($relative in $runtimeFiles) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $PackageRoot $relative) -Algorithm SHA256).Hash
        $sourceHash = (Get-FileHash -LiteralPath $expectedFiles[$relative] -Algorithm SHA256).Hash
        if ($hash -cne $sourceHash) { throw "Package copy hash mismatch: $relative" }
        "$hash  $($relative.Replace('\', '/'))"
    }
    $manifest | Set-Content -LiteralPath (Join-Path $PackageRoot 'SHA256SUMS.txt') -Encoding ascii
}

$distRoot = Join-Path $PSScriptRoot 'dist'
$releaseName = "RedWolfRadio-$Version"
$workshopName = "RedWolfRadio-Workshop-$Version"
$releaseRoot = Join-Path $distRoot $releaseName
$workshopRoot = Join-Path $distRoot $workshopName
$releaseZip = Join-Path $distRoot "$releaseName.zip"
$workshopZip = Join-Path $distRoot "$workshopName.zip"

New-Item -ItemType Directory -Path $distRoot -Force | Out-Null
$distBoundary = [IO.Path]::GetFullPath($distRoot).TrimEnd('\') + '\'
foreach ($target in @($releaseRoot, $workshopRoot, $releaseZip, $workshopZip)) {
    $resolvedTarget = [IO.Path]::GetFullPath($target)
    if (!$resolvedTarget.StartsWith($distBoundary, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Package target is outside dist: $resolvedTarget"
    }
    if (Test-Path -LiteralPath $target) {
        Remove-Item -LiteralPath $target -Recurse -Force
    }
}

$commonFiles = @(
    'README.md',
    'README.ru.md',
    'CHANGELOG.md',
    'LICENSE',
    'THIRD-PARTY-NOTICES.md',
    'DISTRIBUTION.md',
    'VALIDATION.md',
    'workshop-description.txt',
    'workshop-description.ru.txt',
    'previewimage.png'
)

New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
Copy-Item -LiteralPath $DllPath -Destination (Join-Path $releaseRoot 'RedWolfRadio.dll')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RedWolfRadio.ini') -Destination $releaseRoot
Copy-PlayerCompanion $releaseRoot
foreach ($name in $commonFiles) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $releaseRoot
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md') -Destination (Join-Path $releaseRoot 'ARTWORK-TERMS.md')
$releaseAssets = Join-Path $releaseRoot 'assets'
New-Item -ItemType Directory -Path $releaseAssets -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-primary-logo-transparent.png') -Destination $releaseAssets
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\red-wolf-radio-how-it-works-1.0-player-en.jpg'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-how-it-works-1.0-player-ru.jpg'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-how-it-works-1.0-player-prompts.md') -Destination $releaseAssets
$releaseNotices = Join-Path $releaseRoot 'third_party\miniaudio'
New-Item -ItemType Directory -Path $releaseNotices -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\miniaudio\LICENSE') -Destination $releaseNotices

Assert-PackageAndWriteHashes $releaseRoot 'RedWolfRadio.dll' 'RedWolfRadio.ini'
Compress-Archive -Path (Join-Path $releaseRoot '*') -DestinationPath $releaseZip -CompressionLevel Optimal

$workshopPlugin = Join-Path $workshopRoot 'plugin'
New-Item -ItemType Directory -Path $workshopPlugin -Force | Out-Null
Copy-Item -LiteralPath $DllPath -Destination (Join-Path $workshopPlugin 'RedWolfRadio.dll')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RedWolfRadio.ini') -Destination $workshopPlugin
Copy-PlayerCompanion $workshopRoot
foreach ($name in $commonFiles) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $workshopRoot
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md') -Destination (Join-Path $workshopRoot 'ARTWORK-TERMS.md')
$workshopAssets = Join-Path $workshopRoot 'assets'
New-Item -ItemType Directory -Path $workshopAssets -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-primary-logo-transparent.png') -Destination $workshopAssets
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\red-wolf-radio-how-it-works-1.0-player-en.jpg'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-how-it-works-1.0-player-ru.jpg'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-how-it-works-1.0-player-prompts.md') -Destination $workshopAssets
$workshopNotices = Join-Path $workshopRoot 'third_party\miniaudio'
New-Item -ItemType Directory -Path $workshopNotices -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\miniaudio\LICENSE') -Destination $workshopNotices

Assert-PackageAndWriteHashes $workshopRoot 'plugin\RedWolfRadio.dll' 'plugin\RedWolfRadio.ini'
Compress-Archive -Path (Join-Path $workshopRoot '*') -DestinationPath $workshopZip -CompressionLevel Optimal

Get-Item -LiteralPath $releaseZip, $workshopZip | Select-Object FullName, Length
