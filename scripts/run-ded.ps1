<#
.SYNOPSIS
  Run our build\q2proded.exe as a "remote" Action server on this machine.

.DESCRIPTION
  The M3 test rig (design doc section 10 M3): a dedicated server from the same
  build, with our build\gamex86_64.dll, bound to one of this machine's
  addresses so that a client on the same box connects to it as an ordinary
  remote client (NA_IP from that address, not loopback). Point the client at
  it with `connect <ip>:<port>`.

  The install is read-only via $env:AQ_BASEPATH (falls back to the persistent
  user variable of that name). Configs, logs (logs\console.log) go to
  <repo>\work\aqhome-ded, separate from the client's work\aqhome.

  The server console is the process's own window; send it commands with
  -Exec <cfg> (looked up in the game search path, e.g.
  work\aqhome-ded\action) or over rcon (-RconPassword).

.PARAMETER Ip
  Address to bind (net_ip). Default: this machine's first private LAN IPv4.

.EXAMPLE
  .\scripts\run-ded.ps1 -Map wizs -Extra '+set sv_iplimit 3'
#>
[CmdletBinding()]
param(
    [string]$Map = 'wizs',
    [string]$Ip = '',
    [int]$Port = 27920,
    [int]$MaxClients = 8,
    [string]$RconPassword = '',
    [switch]$Wait,
    [string]$Extra = '',
    [string]$Exec = ''
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot

$base = $env:AQ_BASEPATH
if (-not $base) { $base = [Environment]::GetEnvironmentVariable('AQ_BASEPATH', 'User') }
if (-not $base) { throw 'AQ_BASEPATH is not set: point it at the AQtion install folder (the one containing baseaq\ and action\).' }
if (-not (Test-Path (Join-Path $base 'baseaq'))) { throw "AQ_BASEPATH '$base' has no baseaq\ folder: not an AQtion install." }

$exe = Join-Path $Repo 'build\q2proded.exe'
if (-not (Test-Path $exe)) { throw "$exe not found: run scripts\build.ps1 first." }

$running = Get-Process -Name q2proded -ErrorAction SilentlyContinue
if ($running) { throw "Already running: $(($running | ForEach-Object { "$($_.Name)($($_.Id))" }) -join ', '). Not starting a second server." }

if (-not $Ip) {
    $Ip = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -match '^(10\.|192\.168\.|172\.(1[6-9]|2[0-9]|3[01])\.)' } |
        Select-Object -First 1 -ExpandProperty IPAddress
    if (-not $Ip) { throw 'No private LAN IPv4 address found; pass -Ip.' }
}

$home_ = Join-Path $Repo 'work\aqhome-ded'
New-Item -ItemType Directory -Force $home_ | Out-Null

$a = @(
    "+set basedir `"$base`"",
    "+set libdir `"$base`"",
    "+set homedir `"$home_`"",
    '+set game action',
    "+set sys_forcegamelib `"$(Join-Path $Repo 'build\gamex86_64.dll')`"",
    "+set net_ip $Ip",
    "+set net_port $Port",
    '+set logfile_flush 1',
    '+set logfile 2',
    '+set deathmatch 1',
    "+set maxclients $MaxClients"
)
if ($RconPassword) { $a += "+set rcon_password $RconPassword" }
# Extra goes before +map so latched cvars (sv_force_reconnect, ...) apply;
# -Exec runs after the map has loaded.
if ($Extra) { $a += $Extra }
$a += "+map $Map"
if ($Exec) { $a += "+exec $Exec" }

Write-Host "$exe $($a -join ' ')"
$p = Start-Process -FilePath $exe -ArgumentList ($a -join ' ') -PassThru
Write-Host "pid $($p.Id) on ${Ip}:$Port"
if ($Wait) {
    $p.WaitForExit()
    Write-Host "exit code $($p.ExitCode)"
}
