<#
.SYNOPSIS
  Run our build\q2pro.exe windowed against the read-only AQtion install.

.DESCRIPTION
  The install is taken from $env:AQ_BASEPATH (the AQtion Steam folder; set it
  locally, never commit it). Configs, logs (logs\<LogName>.log) and
  screenshots go to <repo>\work\aqhome (or -HomeDir); the install is never
  written by the engine (Steam's game DLL writes items.txt into the working
  directory: use -WorkDir for a scratch one).

  By default the listen server uses our own build\gamex86_64.dll via
  sys_forcegamelib. -GameLib steam loads the install's action\gamex86_64.dll
  (TNG 2.82, legacy game API 4; works since 0.0.0.20 through the adapter in
  src/server/game.c), which is what the release exe uses.

  Real pads are never opened (+set pad_enable 0; padinject drives virtual
  pads) unless -Pads is given.

  -Offscreen (agent test runs; docs/BUILD-AND-PROVENANCE.md "Agent testing:
  offscreen window"): the client runs in a 1280x720 vid_noactivate window
  (never focused, raised, fullscreen or mouse-grabbing; s_enable 0: no sound device) on
  an attached virtual display (Sunshine / Parsec / SudoMaker / IDD). With no
  virtual display attached it stops and lists the adapters, unless
  -AllowPrimary, which puts the window at the bottom-right corner of the
  primary display, at the bottom of the window stack. Display settings are
  never changed. With -Wait the foreground window is sampled every 100 ms
  and the script reports whether the game's window ever had focus.

.EXAMPLE
  $env:AQ_BASEPATH = '<AQtion install>'
  .\scripts\run.ps1 -Map wizs -Wait -Extra '+set name SpikeA +set bot_enable 1' -Exec spikea.cfg

.EXAMPLE
  .\scripts\run.ps1 -Offscreen -AllowPrimary -GameLib steam -Exec menus-a.cfg -LogName menus-a -WorkDir work\scratch -Wait

.EXAMPLE
  # release exe placed in the AQtion root (self-locating), settings kept in the repo
  .\scripts\run.ps1 -Offscreen -Exe "$env:AQ_BASEPATH\q2pro-splitscreen-test.exe" -SelfLocate -HomeDir work\steamtest-home -Wait
#>
[CmdletBinding()]
param(
    [string]$Map = '',
    [ValidateSet('own', 'steam')]
    [string]$GameLib = 'own',
    [switch]$Dedicated,
    [switch]$Wait,
    [string]$Extra = '',
    [string]$Exec = '',
    [switch]$Offscreen,
    [switch]$AllowPrimary,
    [string]$Exe = '',
    [switch]$SelfLocate,
    [string]$HomeDir = '',
    [string]$WorkDir = '',
    [string]$LogName = '',
    [switch]$Pads,
    [int]$TimeoutSec = 900
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot

function Resolve-RepoPath([string]$p) {
    if ([System.IO.Path]::IsPathRooted($p)) { return $p }
    return (Join-Path $Repo $p)
}

Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices; using System.Text; using System.Collections.Generic;
public static class SplitWin {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DISPLAY_DEVICE {
        public int cb;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string DeviceName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceString;
        public int StateFlags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceID;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceKey;
    }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct DEVMODE {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
        public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra;
        public int dmFields, dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;
        public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
        public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
        public int dmICMMethod, dmICMIntent, dmMediaType, dmDitherType, dmReserved1, dmReserved2, dmPanningWidth, dmPanningHeight;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool EnumDisplayDevicesW(string dev, uint i, ref DISPLAY_DEVICE dd, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool EnumDisplaySettingsW(string dev, int mode, ref DEVMODE dm);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern int GetWindowLongW(IntPtr h, int idx);

    // "name|adapter|attached|primary|x|y|w|h" per adapter output (physical pixels)
    public static string[] Displays() {
        var list = new List<string>();
        for (uint i = 0; i < 256; i++) {
            var d = new DISPLAY_DEVICE(); d.cb = Marshal.SizeOf(typeof(DISPLAY_DEVICE));
            if (!EnumDisplayDevicesW(null, i, ref d, 0)) break;
            bool attached = (d.StateFlags & 1) != 0, primary = (d.StateFlags & 4) != 0;
            int x = 0, y = 0, w = 0, h = 0;
            if (attached) {
                var m = new DEVMODE(); m.dmSize = (short)Marshal.SizeOf(typeof(DEVMODE));
                if (EnumDisplaySettingsW(d.DeviceName, -1, ref m)) { x = m.dmPositionX; y = m.dmPositionY; w = m.dmPelsWidth; h = m.dmPelsHeight; }
            }
            list.Add(string.Join("|", d.DeviceName, d.DeviceString, attached, primary, x, y, w, h));
        }
        return list.ToArray();
    }
    public static uint ForegroundPid() {
        uint pid; GetWindowThreadProcessId(GetForegroundWindow(), out pid); return pid;
    }
    // extended style of the process' first visible top-level window, -1 if none
    public static int ExStyleOf(uint pid) {
        int ex = -1;
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p == pid && IsWindowVisible(h)) { ex = GetWindowLongW(h, -20); return false; }
            return true;
        }, IntPtr.Zero);
        return ex;
    }
}
'@

$base = $env:AQ_BASEPATH
if (-not $base) { $base = [Environment]::GetEnvironmentVariable('AQ_BASEPATH', 'User') }
if (-not $base) { throw 'AQ_BASEPATH is not set: point it at the AQtion install folder (the one containing baseaq\ and action\).' }
if (-not (Test-Path (Join-Path $base 'baseaq'))) { throw "AQ_BASEPATH '$base' has no baseaq\ folder: not an AQtion install." }

if ($Exe) {
    $exe = Resolve-RepoPath $Exe
} else {
    $exeName = 'q2pro.exe'
    if ($Dedicated) { $exeName = 'q2proded.exe' }
    $exe = Join-Path $Repo "build\$exeName"
}
if (-not (Test-Path $exe)) { throw "$exe not found: run scripts\build.ps1 first." }

# one client and one dedicated server may run side by side (scripts\run-ded.ps1)
$names = @('q2pro', 'aqtion', 'q2pro-splitscreen', 'q2pro-splitscreen-test')
if ($Dedicated) { $names = @('q2proded') }
$running = Get-Process -Name $names -ErrorAction SilentlyContinue
if ($running) { throw "Already running: $(($running | ForEach-Object { "$($_.Name)($($_.Id))" }) -join ', '). Not starting a second instance." }

# -Offscreen: pick the display before anything is started
$geometry = '1280x720'
if ($Offscreen -and -not $Dedicated) {
    $displays = [SplitWin]::Displays() | ForEach-Object {
        $f = $_ -split '\|'
        [pscustomobject]@{ Name = $f[0]; Adapter = $f[1]; Attached = [bool]::Parse($f[2]); Primary = [bool]::Parse($f[3])
                           X = [int]$f[4]; Y = [int]$f[5]; W = [int]$f[6]; H = [int]$f[7] }
    }
    $virtualRx = 'Sunshine|Parsec|SudoMaker|IDD|Virtual Display'
    $virt = $displays | Where-Object { $_.Attached -and -not $_.Primary -and $_.Adapter -match $virtualRx -and $_.W -ge 1280 -and $_.H -ge 720 } | Select-Object -First 1
    if ($virt) {
        $x = $virt.X; $y = $virt.Y
        Write-Host "offscreen: virtual display $($virt.Name) ($($virt.Adapter)) at $($virt.X),$($virt.Y) $($virt.W)x$($virt.H)"
    } else {
        $byAdapter = $displays | Group-Object Adapter | ForEach-Object {
            $att = @($_.Group | Where-Object Attached)
            "  $($_.Name): $($_.Count) outputs, $($att.Count) attached to the desktop" + $(if ($att) { " (" + (($att | ForEach-Object { "$($_.Name) $($_.W)x$($_.H) at $($_.X),$($_.Y)$(if ($_.Primary) {' primary'})" }) -join '; ') + ")" } else { '' })
        }
        Write-Host "offscreen: no virtual display is attached to the desktop. Adapters:"
        $byAdapter | ForEach-Object { Write-Host $_ }
        if (-not $AllowPrimary) {
            throw 'No virtual display attached; display settings are never changed by this script. Use -AllowPrimary for a no-activate window in a corner of the primary display.'
        }
        $prim = $displays | Where-Object Primary | Select-Object -First 1
        Add-Type -AssemblyName System.Windows.Forms
        # the engine is per-monitor DPI aware: place it in physical pixels, bottom-right,
        # above the taskbar (work area scaled from this DPI-unaware host)
        $wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
        $sb = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
        $scale = $prim.W / [double]$sb.Width
        $x = $prim.X + [int]($wa.Right * $scale) - 1280 - 16
        $y = $prim.Y + [int]($wa.Bottom * $scale) - 720 - 40
        Write-Host "offscreen: fallback, primary display $($prim.Name) corner $x,$y (window at the bottom of the stack, never activated)"
    }
    $geometry = "1280x720+$x+$y"
}

if ($HomeDir) { $home_ = Resolve-RepoPath $HomeDir } else { $home_ = Join-Path $Repo 'work\aqhome' }
New-Item -ItemType Directory -Force $home_ | Out-Null

$a = @()
if (-not $SelfLocate) {
    $a += @("+set basedir `"$base`"", "+set libdir `"$base`"", '+set game action')
}
if (-not $SelfLocate -or $HomeDir) { $a += "+set homedir `"$home_`"" }
$a += @('+set logfile_flush 1', '+set logfile 2')
if ($LogName) { $a += "+set logfile_name $LogName" }
if (-not $Pads) { $a += '+set pad_enable 0' }
if ($GameLib -eq 'own' -and -not $SelfLocate) { $a += "+set sys_forcegamelib `"$(Join-Path $Repo 'build\gamex86_64.dll')`"" }
if (-not $Dedicated) {
    $a += @('+set vid_fullscreen 0', "+set vid_geometry $geometry")
    if ($Offscreen) { $a += @('+set vid_noactivate 1', '+set s_enable 0') }
}
# Extra goes before +map so latched cvars (bot_enable, deathmatch, ...) apply;
# -Exec <cfg> (looked up in the game search path, e.g. work/aqhome/action) runs after the map has loaded.
if ($Extra) { $a += $Extra }
if ($Map) { $a += @('+set deathmatch 1', "+map $Map") }
if ($Exec) { $a += "+exec $Exec" }

$sp = @{ FilePath = $exe; ArgumentList = ($a -join ' '); PassThru = $true }
if ($WorkDir) {
    $wd = Resolve-RepoPath $WorkDir
    New-Item -ItemType Directory -Force $wd | Out-Null
    $sp.WorkingDirectory = $wd
}
Write-Host "$exe $($a -join ' ')"
$fgBefore = [SplitWin]::ForegroundPid()
$p = Start-Process @sp
Write-Host "pid $($p.Id)"
if ($Wait) {
    $samples = 0; $focused = 0; $ex = -1
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while (-not $p.HasExited) {
        if ((Get-Date) -gt $deadline) {
            Write-Host "timeout after $TimeoutSec s: stopping pid $($p.Id) (started by this script)"
            Stop-Process -Id $p.Id -Force
            break
        }
        $samples++
        if ([SplitWin]::ForegroundPid() -eq [uint32]$p.Id) { $focused++ }
        if ($ex -eq -1) { $ex = [SplitWin]::ExStyleOf([uint32]$p.Id) }
        Start-Sleep -Milliseconds 100
    }
    $p.WaitForExit()
    Write-Host "exit code $($p.ExitCode)"
    if ($Offscreen) {
        $noact = if ($ex -ne -1) { (($ex -band 0x08000000) -ne 0) } else { 'window not seen' }
        Write-Host ("focus: game window was foreground in {0} of {1} samples (100 ms); WS_EX_NOACTIVATE={2}; foreground pid before {3}, after {4}" -f `
            $focused, $samples, $noact, $fgBefore, [SplitWin]::ForegroundPid())
    }
}
