<#
.SYNOPSIS
  Clean build + release-candidate zip: build\aqtion-splitscreen-<VERSION>-windows-x86_64.zip.

.DESCRIPTION
  Runs scripts\build.ps1 -Clean (MSVC 2019 x64 by default), then zips the
  two files a player gets:

    q2pro-splitscreen.exe   our client (build\q2pro.exe renamed, so it can
                            never be mistaken for AQtion's own q2pro.exe).
                            It hosts with Steam's action\gamex86_64.dll
                            (legacy game API 4, src/server/game.c), so no
                            game DLL is shipped.
    README.txt              scripts\release\README.txt with the version:
                            what it is, unpack and run, pads, home\, and the
                            repo link (full README, CHANGELOG, LICENSE and
                            the GPL source)

  The version is in the exe's version resource and console banner; the
  build is checked against VERSION (BUILD-INFO.txt and the resource).
  Our exe needs only Windows DLLs and the Visual C++ runtime that AQtion's
  own q2pro.exe needs too (dumpbin /dependents), so no DLL is added.
  scripts\release\install.ps1 stays in the repo (optional, not shipped).
  The release number and its GitHub release are the maintainer's call; this
  only builds the candidate for the version in VERSION.

.PARAMETER NoBuild
  Package the existing build\ output (no clean build).

.PARAMETER Vs
  Passed to build.ps1 (2019 default).
#>
[CmdletBinding()]
param(
    [switch]$NoBuild,
    [ValidateSet('2019', '2022', 'latest')]
    [string]$Vs = '2019'
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Repo 'build'

if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Clean -Vs $Vs
}

$version = (Get-Content -LiteralPath (Join-Path $Repo 'VERSION') -TotalCount 1).Trim()
$name = "aqtion-splitscreen-$version"
$stage = Join-Path $Build $name
$zip = Join-Path $Build "$name-windows-x86_64.zip"

foreach ($f in 'q2pro.exe', 'BUILD-INFO.txt') {
    if (-not (Test-Path -LiteralPath (Join-Path $Build $f))) { throw "build\$f is missing: run without -NoBuild." }
}
$infoVersion = (Select-String -LiteralPath (Join-Path $Build 'BUILD-INFO.txt') -Pattern '^version:\s*(.+)$').Matches[0].Groups[1].Value.Trim()
if ($infoVersion -ne $version) { throw "build\BUILD-INFO.txt is version $infoVersion, VERSION says ${version}: rebuild." }
$exeVersion = (Get-Item -LiteralPath (Join-Path $Build 'q2pro.exe')).VersionInfo.ProductVersion
if ($exeVersion -ne $version) { throw "q2pro.exe version resource says '$exeVersion', VERSION says $version." }

if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item -LiteralPath (Join-Path $Build 'q2pro.exe') -Destination (Join-Path $stage 'q2pro-splitscreen.exe')
$readme = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'release\README.txt') -Raw).Replace('@VERSION@', $version)
$readme = $readme -replace "`r?`n", "`r`n"      # CRLF for Notepad; ASCII, no BOM
[IO.File]::WriteAllText((Join-Path $stage 'README.txt'), $readme, (New-Object System.Text.ASCIIEncoding))

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Remove-Item -LiteralPath $stage -Recurse -Force

Write-Host "packaged $zip ($([math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 2)) MiB)"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $z.Entries | ForEach-Object { Write-Host ("  {0,10:N0}  {1}" -f $_.Length, $_.FullName) }
} finally {
    $z.Dispose()
}
