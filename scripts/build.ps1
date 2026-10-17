<#
.SYNOPSIS
  Build q2pro.exe / q2proded.exe / gamex86_64.dll with MSVC 2019 x64 + Meson into build\.

.DESCRIPTION
  Recipe A of docs/BUILD-AND-PROVENANCE.md. PowerShell 5.1 safe.
  - imports the vcvars64.bat environment into this process
  - writes scripts\msvc-native.ini (machine-specific, gitignored) pinning
    python3/python to the real CPython, because python3 on PATH can be the
    Windows Store stub
  - meson setup (first time) / reconfigure, then meson compile
  - writes build\BUILD-INFO.txt

  Prerequisites: VS 2019 (or newer) with the C++ x64 toolchain, CPython 3
  as `python` on PATH with meson installed (`python -m pip install --user meson`),
  Ninja on PATH. Network on the first configure (wraps are downloaded into
  subprojects\packagecache).

.PARAMETER Clean
  Delete build\ before configuring.

.PARAMETER BuildType
  Meson buildtype (default: release).

.PARAMETER Vs
  Visual Studio generation whose x64 toolchain is used: 2019 (default,
  MSVC 14.29, the project's reference toolchain), 2022, or latest.
  -Clean is needed when switching between toolchains.
#>
[CmdletBinding()]
param(
    [switch]$Clean,
    [string]$BuildType = 'release',
    [ValidateSet('2019', '2022', 'latest')]
    [string]$Vs = '2019'
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Repo 'build'
$NativeFile = Join-Path $PSScriptRoot 'msvc-native.ini'

# Options: Windows CI set (build.yml) minus --auto-features/--fatal-meson-warnings,
# Discord and SDL2 video/audio off, gamepad (static SDL2 joystick only) on. default_library=static is forced for every wrap by meson.build.
$MesonOptions = @(
    "--buildtype=$BuildType",
    '-Dwrap_mode=forcefallback',
    '-Daqtion-build=true',
    '-Ddiscord-sdk=false',
    '-Davcodec=disabled',
    '-Dsdl2=disabled',
    '-Dgamepad=enabled',
    '-Dx11=disabled',
    '-Dwayland=disabled',
    '-Dwindows-egl=true',
    '-Dtests=false',
    '-Dwerror=false'
)

function Import-VcVars {
    if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64') {
        Write-Host "Using the already-initialised VS $env:VisualStudioVersion x64 environment"
        return
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found: install Visual Studio with the C++ x64 toolchain.' }
    $range = @{ '2019' = @('-version', '[16.0,17.0)'); '2022' = @('-version', '[17.0,18.0)'); 'latest' = @('-latest') }[$Vs]
    $vcvars = $null
    $insts = & $vswhere @range -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    foreach ($inst in $insts) {
        $cand = Join-Path $inst 'VC\Auxiliary\Build\vcvars64.bat'
        if (Test-Path $cand) { $vcvars = $cand; break }
    }
    if (-not $vcvars) { throw "vcvars64.bat for VS $Vs not found (try -Vs latest)." }
    Write-Host "Using $vcvars"
    $lines = & cmd.exe /c "`"$vcvars`" >nul 2>&1 & set"
    foreach ($line in $lines) {
        $i = $line.IndexOf('=')
        if ($i -gt 0) {
            Set-Item -Path ("Env:" + $line.Substring(0, $i)) -Value $line.Substring($i + 1)
        }
    }
    if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') { throw "vcvars64.bat did not set up an x64 environment." }
}

function Get-RealPython {
    foreach ($c in (Get-Command python -All -ErrorAction SilentlyContinue)) {
        if ($c.Source -notlike '*\WindowsApps\*') { return $c.Source }
    }
    throw 'No real CPython found on PATH (only the Windows Store stub). Install Python 3 and put python.exe on PATH.'
}

$python = Get-RealPython
& $python -c "import mesonbuild" 2>$null
if ($LASTEXITCODE -ne 0) { throw "meson is not installed for $python (python -m pip install --user meson)." }
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) { throw 'ninja not found on PATH.' }

Import-VcVars

$py = $python -replace '\\', '/'
@"
[binaries]
python3 = '$py'
python = '$py'
"@ | Out-File -Encoding ascii $NativeFile

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host 'Removing build\'
    Remove-Item -Recurse -Force $BuildDir
}

Push-Location $Repo
try {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (Test-Path (Join-Path $BuildDir 'build.ninja')) {
        & $python -m mesonbuild.mesonmain setup --reconfigure build --native-file $NativeFile @MesonOptions
    } else {
        & $python -m mesonbuild.mesonmain setup build --native-file $NativeFile @MesonOptions
    }
    if ($LASTEXITCODE -ne 0) { throw "meson setup failed ($LASTEXITCODE)" }

    & $python -m mesonbuild.mesonmain compile -C build
    if ($LASTEXITCODE -ne 0) { throw "meson compile failed ($LASTEXITCODE)" }
    $sw.Stop()

    $version = (Get-Content (Join-Path $Repo 'VERSION') -TotalCount 1).Trim()
    $sha = (& git -C $Repo rev-parse --short HEAD).Trim()
    $dirty = (& git -C $Repo status --porcelain --untracked-files=no)
    if ($dirty) { $sha = "$sha-dirty" }
    $mesonVer = (& $python -m mesonbuild.mesonmain --version).Trim()
    $info = @(
        "version: $version",
        "git: $sha",
        "built: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))",
        "toolchain: MSVC $env:VCToolsVersion x64, meson $mesonVer",
        "meson options: $($MesonOptions -join ' ')",
        "build time: $([int]$sw.Elapsed.TotalSeconds) s"
    )
    $info | Out-File -Encoding ascii (Join-Path $BuildDir 'BUILD-INFO.txt')
    $info | ForEach-Object { Write-Host $_ }
    foreach ($f in 'q2pro.exe', 'q2proded.exe', 'gamex86_64.dll') {
        $p = Join-Path $BuildDir $f
        if (Test-Path $p) { Write-Host ("built {0} ({1:N0} bytes)" -f $f, (Get-Item $p).Length) }
        else { Write-Warning "missing $f" }
    }
}
finally {
    Pop-Location
}
