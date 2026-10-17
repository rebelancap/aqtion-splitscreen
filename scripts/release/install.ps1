<#
.SYNOPSIS
  Install (or -Uninstall / -Purge) AQtion splitscreen into <AQtion>\splitscreen\.

.DESCRIPTION
  Optional developer script (not shipped since 0.0.0.20: the release zip is
  just q2pro-splitscreen.exe + README.txt, unpacked by hand). Put it next to
  the unpacked zip's files and run it there. It copies them into a new
  "splitscreen" folder inside the AQtion game folder and records each one with its SHA-256 in
  splitscreen\INSTALL-MANIFEST.txt (entry by entry, so an install that
  stopped half way can simply be run again). It also creates an empty
  splitscreen\home\ folder: started from splitscreen\, q2pro-splitscreen.exe
  uses it as its write folder, so configs, profiles, screenshots, demos and the console history
  stay inside splitscreen\. Nothing outside that folder is written, and no
  AQtion file is replaced: the original q2pro.exe, aqtion.exe and game DLLs
  stay as they are.

  -Uninstall removes exactly the files in the manifest (only if they are
  unchanged, unless -Force) and keeps home\ (your settings). -Purge does the
  same and then deletes the whole splitscreen folder, home\ included.

  The AQtion folder is found from -GamePath, else $env:AQ_BASEPATH, else the
  Steam libraries (registry + libraryfolders.vdf).

  Windows PowerShell 5.1 or newer. If scripts are blocked, run:
    powershell -ExecutionPolicy Bypass -File .\install.ps1

.PARAMETER GamePath
  The AQtion folder (the one with baseaq\ and action\ in it).

.PARAMETER Uninstall
  Remove the installed files again; keep the settings in splitscreen\home\.

.PARAMETER Purge
  Uninstall and delete the whole splitscreen folder, settings included.

.PARAMETER Force
  With -Uninstall: also remove manifest files that were changed since the
  install.
#>
[CmdletBinding()]
param(
    [string]$GamePath = '',
    [switch]$Uninstall,
    [switch]$Purge,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$FolderName = 'splitscreen'
$ManifestName = 'INSTALL-MANIFEST.txt'
$HomeName = 'home'
$ExeName = 'q2pro-splitscreen.exe'
$Files = @(
    $ExeName,
    'README.txt'
)

function Test-AQtionFolder([string]$p) {
    if (-not $p) { return $false }
    return (Test-Path -LiteralPath (Join-Path $p 'baseaq') -PathType Container) -and
           (Test-Path -LiteralPath (Join-Path $p 'action') -PathType Container)
}

function Get-SteamLibraries {
    $roots = @()
    foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam') {
        try {
            $item = Get-ItemProperty -LiteralPath $key -ErrorAction Stop
            foreach ($name in 'SteamPath', 'InstallPath') {
                if ($item.$name) { $roots += ($item.$name -replace '/', '\') }
            }
        } catch { }
    }
    $libs = @()
    foreach ($root in ($roots | Select-Object -Unique)) {
        $libs += $root
        $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            foreach ($line in (Get-Content -LiteralPath $vdf)) {
                if ($line -match '^\s*"path"\s*"(.+)"\s*$') {
                    $libs += ($Matches[1] -replace '\\\\', '\')
                }
            }
        }
    }
    return $libs | Select-Object -Unique
}

function Find-AQtion {
    if ($GamePath) {
        if (Test-AQtionFolder $GamePath) { return (Resolve-Path -LiteralPath $GamePath).Path }
        throw "'$GamePath' is not an AQtion folder (no baseaq\ and action\ in it)."
    }
    foreach ($p in $env:AQ_BASEPATH, [Environment]::GetEnvironmentVariable('AQ_BASEPATH', 'User')) {
        if (Test-AQtionFolder $p) { return (Resolve-Path -LiteralPath $p).Path }
    }
    foreach ($lib in Get-SteamLibraries) {
        $p = Join-Path $lib 'steamapps\common\AQtion'
        if (Test-AQtionFolder $p) { return (Resolve-Path -LiteralPath $p).Path }
    }
    # run from an installed splitscreen folder (-Uninstall from inside it)
    $up = Split-Path -Parent $PSScriptRoot
    if ((Split-Path -Leaf $PSScriptRoot) -eq $FolderName -and (Test-AQtionFolder $up)) { return $up }
    throw 'AQtion folder not found. Pass it: .\install.ps1 -GamePath "<path to AQtion>"'
}

function Get-Sha256([string]$path) {
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

# manifest lines: "<sha256>  <file name>"; '#' lines are comments
function Read-Manifest([string]$path) {
    $entries = @()
    foreach ($line in (Get-Content -LiteralPath $path)) {
        if ($line -match '^([0-9a-f]{64})  (.+)$') {
            $entries += [pscustomobject]@{ Hash = $Matches[1]; Name = $Matches[2] }
        }
    }
    return $entries
}

function Write-Manifest([string]$path, [string[]]$header, $entries) {
    $lines = @($header)
    foreach ($k in $Files) { if ($entries.Contains($k)) { $lines += "$($entries[$k])  $k" } }
    foreach ($k in $entries.Keys) { if ($Files -notcontains $k) { $lines += "$($entries[$k])  $k" } }
    $lines | Out-File -LiteralPath $path -Encoding ascii
}

function Test-GameRunning {
    $p = @(Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($ExeName)) -ErrorAction SilentlyContinue)
    return ($p.Count -gt 0)
}

# a folder that is some shell's current directory cannot be removed: step out
# of it first (this shell), and only print a note if another window holds it
function Remove-Folder([string]$path, [switch]$Recurse) {
    $full = $path.TrimEnd('\')
    $here = (Get-Location).ProviderPath.TrimEnd('\')
    if ($here -eq $full -or $here.StartsWith($full + '\', [StringComparison]::OrdinalIgnoreCase)) {
        Set-Location -LiteralPath $game
        Write-Host "(this window's current folder was inside $full; it is now $game)"
    }
    $cwd = [Environment]::CurrentDirectory.TrimEnd('\')
    if ($cwd -eq $full -or $cwd.StartsWith($full + '\', [StringComparison]::OrdinalIgnoreCase)) {
        [Environment]::CurrentDirectory = $game
    }
    try {
        if ($Recurse) { Remove-Item -LiteralPath $full -Recurse -Force -ErrorAction Stop }
        else { Remove-Item -LiteralPath $full -Force -ErrorAction Stop }
        Write-Host "removed $full"
        return $true
    } catch {
        Write-Host "Could not remove $full (is it open in another window or program?)."
        Write-Host 'Close that window and delete the folder by hand; nothing else is left to undo.'
        return $false
    }
}

$game = Find-AQtion
$target = Join-Path $game $FolderName
$manifest = Join-Path $target $ManifestName
$homeDir = Join-Path $target $HomeName
Write-Host "AQtion folder: $game"

if ($Uninstall -or $Purge) {
    if (Test-GameRunning) {
        Write-Host "$ExeName is running: quit the game first, then run this again."
        exit 1
    }
    $haveManifest = Test-Path -LiteralPath $manifest
    if (-not $haveManifest) {
        if ($Purge -and (Test-Path -LiteralPath $homeDir -PathType Container)) {
            Write-Host 'The program files were already removed; deleting the settings folder.'
        } else {
            throw "Nothing to uninstall: $manifest not found."
        }
    }
    $kept = @()
    if ($haveManifest) {
        foreach ($e in (Read-Manifest $manifest)) {
            # names are plain file names written by the installer; never follow a path out of the folder
            if ($e.Name -match '[\\/:]' -or $e.Name -eq '..') { Write-Warning "skipping odd manifest entry '$($e.Name)'"; continue }
            $f = Join-Path $target $e.Name
            if (-not (Test-Path -LiteralPath $f)) { continue }
            if ((Get-Sha256 $f) -ne $e.Hash -and -not $Force -and -not $Purge) {
                Write-Warning "$($e.Name) was changed since the install: kept (use -Force to remove it)"
                $kept += $e.Name
                continue
            }
            Remove-Item -LiteralPath $f -Force
            Write-Host "removed $($e.Name)"
        }
        if ($kept.Count -eq 0) {
            Remove-Item -LiteralPath $manifest -Force
            Write-Host "removed $ManifestName"
        }
    }

    if ($Purge) {
        $left = @(Get-ChildItem -LiteralPath $target -Force | Where-Object { $_.Name -ne $HomeName })
        if ($left.Count -gt 0) {
            Write-Host 'Also deleting what the game left in the folder:'
            $left | ForEach-Object { Write-Host "  $($_.Name)" }
        }
        if (Remove-Folder $target -Recurse) {
            Write-Host 'Purged: the program and your splitscreen settings are gone; the AQtion folder is as it was before the install.'
        }
        return
    }

    $left = @(Get-ChildItem -LiteralPath $target -Force | Where-Object { $_.Name -ne $HomeName })
    $homeLeft = Test-Path -LiteralPath $homeDir -PathType Container
    if ($left.Count -eq 0 -and -not $homeLeft) {
        if (Remove-Folder $target) {
            Write-Host 'Uninstalled. The AQtion folder is as it was before the install.'
        }
        return
    }
    Write-Host 'Uninstalled the program files.'
    if ($left.Count -gt 0) {
        Write-Host "Kept these in $target (the installer did not put them there):"
        $left | ForEach-Object { Write-Host "  $($_.Name)" }
    }
    if ($homeLeft) {
        Write-Host "Your settings were kept in $homeDir"
        Write-Host '(config, profiles, screenshots, demos); installing again picks them up.'
        Write-Host "To delete them too, delete the folder $target,"
        Write-Host 'or run install.ps1 -Purge from the release zip.'
    }
    return
}

# --- install ---
$src = $PSScriptRoot
foreach ($name in $Files) {
    if (-not (Test-Path -LiteralPath (Join-Path $src $name))) { throw "$name is missing next to install.ps1: unpack the whole zip first." }
}
if ((Resolve-Path -LiteralPath $src).Path.TrimEnd('\') -eq $target.TrimEnd('\')) {
    throw "Run install.ps1 from the unpacked zip, not from $target (to remove it: .\install.ps1 -Uninstall)."
}
if (Test-GameRunning) {
    Write-Host "$ExeName is running: quit the game first, then run the install again."
    exit 1
}

# an earlier install of ours (complete or not) may be replaced; any other file there is left alone
$ours = [ordered]@{}
if (Test-Path -LiteralPath $manifest) {
    foreach ($e in (Read-Manifest $manifest)) { $ours[$e.Name] = $e.Hash }
}
if (Test-Path -LiteralPath $target) {
    foreach ($name in $Files) {
        $f = Join-Path $target $name
        if ((Test-Path -LiteralPath $f) -and -not $ours.Contains($name)) {
            # same bytes as the file we ship = left by an install that stopped half way
            if ((Get-Sha256 $f) -eq (Get-Sha256 (Join-Path $src $name))) {
                $ours[$name] = Get-Sha256 $f
                continue
            }
            throw "$f exists and was not installed by this script: not overwriting it. Move it away and run the install again."
        }
    }
} else {
    New-Item -ItemType Directory -Path $target | Out-Null
    Write-Host "created $target"
}
if (-not (Test-Path -LiteralPath $homeDir -PathType Container)) {
    New-Item -ItemType Directory -Path $homeDir | Out-Null
    Write-Host "created $homeDir (your settings go here)"
} else {
    Write-Host "kept $homeDir (your settings)"
}

$version = (Get-Item -LiteralPath (Join-Path $src $ExeName)).VersionInfo.ProductVersion
if (-not $version) { $version = '(unknown)' }

$header = @(
    "# AQtion splitscreen $version, installed $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))",
    "# Files the installer put into this folder (SHA-256). install.ps1 -Uninstall",
    "# removes exactly these and keeps $HomeName\ (settings); -Purge deletes the folder."
)
foreach ($name in $Files) {
    $from = Join-Path $src $name
    $to = Join-Path $target $name
    try {
        # record the name before copying, so a copy that fails half way is still ours on the next run
        $ours[$name] = Get-Sha256 $from
        Write-Manifest $manifest $header $ours
        Copy-Item -LiteralPath $from -Destination $to -Force -ErrorAction Stop
    } catch {
        Write-Host ''
        Write-Host "Copying $name failed: $($_.Exception.Message)"
        Write-Host 'The files copied so far are recorded in the manifest. Fix the cause (close the'
        Write-Host 'game or any program using the file) and run the install again: it carries on.'
        exit 1
    }
    $ours[$name] = Get-Sha256 $to
    Write-Manifest $manifest $header $ours
    Write-Host "copied $name"
}
# files an older install of ours put there that this release no longer ships
# (launch-splitscreen.bat up to 0.0.0.18; gamex86_64.dll, install.ps1, docs
# up to 0.0.0.19 - an old gamex86_64.dll beside the exe would still be loaded
# instead of Steam's): remove them if unchanged
foreach ($name in @($ours.Keys)) {
    if ($Files -contains $name) { continue }
    if ($name -match '[\\/:]' -or $name -eq '..') { continue }
    $f = Join-Path $target $name
    if (-not (Test-Path -LiteralPath $f)) { $ours.Remove($name); continue }
    if ((Get-Sha256 $f) -eq $ours[$name]) {
        Remove-Item -LiteralPath $f -Force
        $ours.Remove($name)
        Write-Host "removed $name (no longer part of this release)"
    }
}
Write-Manifest $manifest $header $ours
Write-Host "wrote $ManifestName"
Write-Host ''
Write-Host "Installed AQtion splitscreen $version into $target"
Write-Host "Start it with $(Join-Path $target $ExeName); a desktop shortcut to it works too."
Write-Host "Settings, profiles, screenshots and demos are kept in $homeDir"
Write-Host "Remove it with: powershell -ExecutionPolicy Bypass -File .\install.ps1 -Uninstall"
Write-Host '(keeps your settings; -Purge instead of -Uninstall deletes them too)'
