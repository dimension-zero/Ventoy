#requires -Version 5.1
<#
.SYNOPSIS
    Fetch pre-built boot/data files from the matching upstream Ventoy release.

.DESCRIPTION
    The Ventoy source tree does not ship the GRUB2 boot binaries or the packed
    Ventoy disk image. Those are produced by the Linux-side build pipeline
    (INSTALL/script/*.sh) and only land in the released zip. To run the built
    Ventoy2Disk*.exe from this tree, you need them.

    This script downloads the matching upstream Windows release zip, verifies
    its SHA-256, and copies only the missing data files into INSTALL/:
        INSTALL/boot/boot.img
        INSTALL/boot/core.img.xz
        INSTALL/ventoy/version
        INSTALL/ventoy/ventoy.disk.img.xz

    Existing files are not overwritten unless -Force is passed.

.PARAMETER Force
    Re-download and overwrite even if the files are already present.

.EXAMPLE
    .\bootstrap-data.ps1
    .\bootstrap-data.ps1 -Force
#>
[CmdletBinding()]
param(
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$Version  = '1.1.12'
$ZipName  = "ventoy-$Version-windows.zip"
$Sha256   = 'e70c505be08d99c55e506832f596c430a9c36f8d087f25542d3f6d332d9b6473'
$ZipUrl   = "https://github.com/ventoy/Ventoy/releases/download/v$Version/$ZipName"

$Root      = $PSScriptRoot
$InstallDir = Join-Path $Root 'INSTALL'

$NeededFiles = @(
    'boot\boot.img',
    'boot\core.img.xz',
    'ventoy\version',
    'ventoy\ventoy.disk.img.xz',
    'ventoy\ventoy_4k.disk.img.xz',
    'ventoy\languages.json',
    'ventoy\plugson.tar.xz'
)

function Test-AllPresent {
    foreach ($rel in $NeededFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $InstallDir $rel))) {
            return $false
        }
    }
    return $true
}

if ((Test-AllPresent) -and -not $Force) {
    Write-Host "All bootstrap data files already present. Use -Force to refetch." -ForegroundColor Green
    exit 0
}

$TempRoot = Join-Path $env:TEMP "ventoy-bootstrap-$Version"
if (Test-Path -LiteralPath $TempRoot) { Remove-Item -Recurse -Force $TempRoot }
New-Item -ItemType Directory -Path $TempRoot | Out-Null
$ZipPath = Join-Path $TempRoot $ZipName

Write-Host "Downloading $ZipUrl ..."
$progPref = $ProgressPreference
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest is dramatically faster without the progress bar
try {
    Invoke-WebRequest -Uri $ZipUrl -OutFile $ZipPath -UseBasicParsing
} finally {
    $ProgressPreference = $progPref
}

Write-Host "Verifying SHA-256 ..."
$actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $ZipPath).Hash.ToLower()
if ($actual -ne $Sha256) {
    throw "SHA-256 mismatch on $ZipName`n  expected: $Sha256`n  actual:   $actual"
}
Write-Host "  OK ($Sha256)" -ForegroundColor Green

Write-Host "Extracting ..."
$ExtractDir = Join-Path $TempRoot 'extract'
Expand-Archive -LiteralPath $ZipPath -DestinationPath $ExtractDir -Force

# The release zip nests everything under ventoy-<version>/
$ReleaseRoot = Get-ChildItem -LiteralPath $ExtractDir -Directory | Select-Object -First 1 -ExpandProperty FullName
if (-not $ReleaseRoot) { throw "Could not find release root inside $ExtractDir" }

foreach ($rel in $NeededFiles) {
    $src = Join-Path $ReleaseRoot $rel
    $dst = Join-Path $InstallDir $rel
    if (-not (Test-Path -LiteralPath $src)) {
        throw "Expected file missing in release zip: $rel"
    }
    $dstDir = Split-Path -Parent $dst
    if (-not (Test-Path -LiteralPath $dstDir)) {
        New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    }
    Copy-Item -LiteralPath $src -Destination $dst -Force
    Write-Host "  copied  $rel" -ForegroundColor DarkGray
}

Remove-Item -Recurse -Force $TempRoot

Write-Host ""
Write-Host "Bootstrap complete. You can now run Ventoy2Disk from INSTALL/." -ForegroundColor Green
Write-Host "  $(Join-Path $InstallDir 'Ventoy2Disk_X64.exe')"
