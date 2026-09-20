param(
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]*$')]
    [string]$Version = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'VERSION') -Raw).Trim(),
    [string]$DllPath = (Join-Path $PSScriptRoot 'build\RedWolfRadio.dll')
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Version)) {
    throw 'Version cannot be blank.'
}
if (!(Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "Missing plugin DLL: $DllPath. Run build.ps1 first or pass -DllPath."
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
    'CHANGELOG.md',
    'LICENSE',
    'THIRD-PARTY-NOTICES.md',
    'DISTRIBUTION.md',
    'previewimage.png'
)

New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
Copy-Item -LiteralPath $DllPath -Destination (Join-Path $releaseRoot 'RedWolfRadio.dll')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RedWolfRadio.ini') -Destination $releaseRoot
foreach ($name in $commonFiles) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $releaseRoot
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md') -Destination (Join-Path $releaseRoot 'ARTWORK-TERMS.md')
$releaseAssets = Join-Path $releaseRoot 'assets'
New-Item -ItemType Directory -Path $releaseAssets -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-primary-logo-transparent.png') -Destination $releaseAssets
$releaseNotices = Join-Path $releaseRoot 'third_party\miniaudio'
New-Item -ItemType Directory -Path $releaseNotices -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\miniaudio\LICENSE') -Destination $releaseNotices

$releaseDll = Join-Path $releaseRoot 'RedWolfRadio.dll'
$releaseHash = (Get-FileHash -LiteralPath $releaseDll -Algorithm SHA256).Hash
"$releaseHash  RedWolfRadio.dll" | Set-Content -LiteralPath (Join-Path $releaseRoot 'SHA256SUMS.txt') -Encoding ascii
Compress-Archive -Path (Join-Path $releaseRoot '*') -DestinationPath $releaseZip -CompressionLevel Optimal

$workshopPlugin = Join-Path $workshopRoot 'plugin'
New-Item -ItemType Directory -Path $workshopPlugin -Force | Out-Null
Copy-Item -LiteralPath $DllPath -Destination (Join-Path $workshopPlugin 'RedWolfRadio.dll')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'RedWolfRadio.ini') -Destination $workshopPlugin
foreach ($name in $commonFiles) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $workshopRoot
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md') -Destination (Join-Path $workshopRoot 'ARTWORK-TERMS.md')
$workshopAssets = Join-Path $workshopRoot 'assets'
New-Item -ItemType Directory -Path $workshopAssets -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'assets\README.md'), (Join-Path $PSScriptRoot 'assets\red-wolf-radio-primary-logo-transparent.png') -Destination $workshopAssets
$workshopNotices = Join-Path $workshopRoot 'third_party\miniaudio'
New-Item -ItemType Directory -Path $workshopNotices -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'third_party\miniaudio\LICENSE') -Destination $workshopNotices

$workshopDll = Join-Path $workshopPlugin 'RedWolfRadio.dll'
$workshopHash = (Get-FileHash -LiteralPath $workshopDll -Algorithm SHA256).Hash
"$workshopHash  plugin/RedWolfRadio.dll" | Set-Content -LiteralPath (Join-Path $workshopRoot 'SHA256SUMS.txt') -Encoding ascii
Compress-Archive -Path (Join-Path $workshopRoot '*') -DestinationPath $workshopZip -CompressionLevel Optimal

Get-Item -LiteralPath $releaseZip, $workshopZip | Select-Object FullName, Length
