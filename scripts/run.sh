#!/bin/bash
# Runs our Linux build windowed against the read-only AQtion install.
#
# The install is taken from $AQ_BASEPATH (the AQtion Steam folder, the one
# with baseaq/ and action/; set it in your shell, never commit it). Configs,
# logs (<homedir>/action/logs/<name>.log) and screenshots
# (<homedir>/action/screenshots/) go to <repo>/work/aqhome (or --homedir);
# the engine never writes into the install.
#
# The listen server uses Steam's own action/gamex86_64.so (TNG 2.82, legacy
# game API 4, through the adapter in src/server/game.c) unless --gamelib own,
# which forces our build/linux/gamex86_64.so via sys_forcegamelib.
#
# Real pads are never opened (+set pad_enable 0; padinject drives virtual
# pads) unless --pads (+set pad_enable 1); sound is off (+set s_enable 0)
# unless --sound. The game is always started under `timeout` (--timeout,
# default 900 s) and only when no other q2pro / aqtion process runs. Without
# a display in the environment (an SSH / agent shell) the desktop session's
# XWayland / Wayland variables are filled in.
#
# usage: scripts/run.sh [options] [-- extra engine args...]
#   --map <name>          +set deathmatch 1 +map <name> (after the extra args)
#   --exec <cfg>          +exec <cfg> after the map
#   --exe <path>          binary to run (default build/linux/q2pro-splitscreen)
#   --self-locate         no basedir/libdir/game on the command line (the exe
#                         locates itself; use with an exe placed in the install)
#   --homedir <dir>       homedir (default work/aqhome; relative = repo-relative;
#                         with --self-locate none unless given: the exe picks
#                         <AQtion>/splitscreen or <exe dir>/home itself)
#   --workdir <dir>       working directory to start in (default work/cwd)
#   --logname <name>      logfile_name (default q2pro-splitscreen)
#   --gamelib steam|own   game library (default steam)
#   --timeout <s>         kill the game after <s> seconds (default 900)
#   --pads / --sound      open real pads / the sound device
#   --ded                 run build/linux/q2proded instead (no window)
#   --gamescope           run nested in a Gamescope window at Steam Deck size
#                         (gamescope -W 1280 -H 800 -w 1280 -h 800 -- <exe>),
#                         game window 1280x800; Gamescope gives the game its
#                         own Xwayland DISPLAY (vid_driver x11 is picked)
#                         ($AQ_GAMESCOPE_ARGS: extra gamescope flags, e.g.
#                         --expose-wayland for its Wayland socket)
#   --size <WxH>          window size (default 1280x720; with --gamescope
#                         1280x800, and the Gamescope window gets this size)
#
# example (bot game, 60 s):
#   export AQ_BASEPATH=<AQtion install>
#   scripts/run.sh --map teamjungle --timeout 75 --logname bots -- +set am 1 +set am_botcount 4
set -e
REPO="$(cd "$(dirname "$0")/.." && pwd)"

MAP= EXEC= EXE= SELF=0 HOMEDIR= WORKDIR= LOGNAME_=q2pro-splitscreen GAMELIB=steam TIMEOUT=900 PADS=0 SOUND=0 DED=0 GAMESCOPE=0 SIZE=
while [ $# -gt 0 ]; do
  case "$1" in
    --map) MAP="$2"; shift 2 ;;
    --exec) EXEC="$2"; shift 2 ;;
    --exe) EXE="$2"; shift 2 ;;
    --self-locate) SELF=1; shift ;;
    --homedir) HOMEDIR="$2"; shift 2 ;;
    --workdir) WORKDIR="$2"; shift 2 ;;
    --logname) LOGNAME_="$2"; shift 2 ;;
    --gamelib) GAMELIB="$2"; shift 2 ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --pads) PADS=1; shift ;;
    --sound) SOUND=1; shift ;;
    --ded) DED=1; shift ;;
    --gamescope) GAMESCOPE=1; shift ;;
    --size) SIZE="$2"; shift 2 ;;
    --) shift; break ;;
    *) echo "unknown option $1 (see the header of $0)" >&2; exit 2 ;;
  esac
done

repo_path() { case "$1" in /*) echo "$1" ;; *) echo "$REPO/$1" ;; esac; }

if [ -z "$AQ_BASEPATH" ]; then
  echo "AQ_BASEPATH is not set: point it at the AQtion install folder (the one containing baseaq/ and action/)." >&2
  exit 1
fi
if [ ! -d "$AQ_BASEPATH/baseaq" ] || [ ! -d "$AQ_BASEPATH/action" ]; then
  echo "AQ_BASEPATH '$AQ_BASEPATH' has no baseaq/ and action/: not an AQtion install." >&2
  exit 1
fi

if [ -n "$EXE" ]; then
  EXE="$(repo_path "$EXE")"
elif [ "$DED" = 1 ]; then
  EXE="$REPO/build/linux/q2proded"
else
  EXE="$REPO/build/linux/q2pro-splitscreen"
fi
[ -x "$EXE" ] || { echo "$EXE not found: run scripts/build.sh first." >&2; exit 1; }

# one game at a time; never touch a process this script did not start
# (process names, not command lines: the repo path itself contains "aqtion")
if RUNNING="$(pgrep -a '^(q2pro|aqtion)')"; then
  echo "Already running, not starting a second instance:" >&2
  echo "$RUNNING" >&2
  exit 1
fi

if [ "$DED" = 0 ] && [ -z "$DISPLAY" ] && [ -z "$WAYLAND_DISPLAY" ]; then
  PLASMA="$(pgrep -o -u "$(id -u)" plasmashell || true)"
  [ -n "$PLASMA" ] || { echo "No display in the environment and no desktop session found." >&2; exit 1; }
  export DISPLAY="${DISPLAY:-:0}" WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
  XAUTHORITY="$(tr '\0' '\n' </proc/"$PLASMA"/environ | sed -n 's/^XAUTHORITY=//p')"
  export XAUTHORITY
fi

if [ -n "$HOMEDIR" ]; then HOMEDIR="$(repo_path "$HOMEDIR")"
elif [ "$SELF" = 0 ]; then HOMEDIR="$REPO/work/aqhome"; fi
[ -n "$HOMEDIR" ] && mkdir -p "$HOMEDIR"

ARGS=()
if [ "$SELF" = 0 ]; then
  ARGS+=(+set basedir "$AQ_BASEPATH" +set libdir "$AQ_BASEPATH" +set game action)
fi
[ -n "$HOMEDIR" ] && ARGS+=(+set homedir "$HOMEDIR")
ARGS+=(+set logfile_flush 1 +set logfile 1 +set logfile_name "$LOGNAME_")
ARGS+=(+set allow_download 0)
if [ "$PADS" = 0 ]; then ARGS+=(+set pad_enable 0); else ARGS+=(+set pad_enable 1); fi
[ "$SOUND" = 0 ] && [ "$DED" = 0 ] && ARGS+=(+set s_enable 0)
if [ "$GAMELIB" = own ]; then
  ARGS+=(+set sys_forcegamelib "$REPO/build/linux/gamex86_64.so")
fi
if [ -z "$SIZE" ]; then
  if [ "$GAMESCOPE" = 1 ]; then SIZE=1280x800; else SIZE=1280x720; fi
fi
case "$SIZE" in [0-9]*x[0-9]*) ;; *) echo "bad --size '$SIZE' (WxH)" >&2; exit 2 ;; esac
[ "$DED" = 0 ] && ARGS+=(+set vid_fullscreen 0 +set vid_geometry "$SIZE")
# extra args go before +map so latched cvars (am, deathmatch, ...) apply
ARGS+=("$@")
[ -n "$MAP" ] && ARGS+=(+set deathmatch 1 +map "$MAP")
[ -n "$EXEC" ] && ARGS+=(+exec "$EXEC")

# the game library reads and writes its own files (items.txt, action/terrain/,
# action/bots/) relative to the working directory: a scratch one under work/
# keeps them out of the repo (the repo has its own action/ source folder); a
# self-locating exe placed in the install changes to the AQtion root itself
if [ -n "$WORKDIR" ]; then WORKDIR="$(repo_path "$WORKDIR")"; else WORKDIR="$REPO/work/cwd"; fi
mkdir -p "$WORKDIR"
cd "$WORKDIR"
echo "$EXE ${ARGS[*]}"
echo "(cwd $WORKDIR, timeout ${TIMEOUT}s, log ${HOMEDIR:-<self-located homedir>}/action/logs/$LOGNAME_.log)"
WRAP=()
if [ "$GAMESCOPE" = 1 ] && [ "$DED" = 0 ]; then
  command -v gamescope >/dev/null 2>&1 || { echo "gamescope not found" >&2; exit 1; }
  W="${SIZE%x*}" H="${SIZE#*x}"
  WRAP=(gamescope -W "$W" -H "$H" -w "$W" -h "$H" $AQ_GAMESCOPE_ARGS --)
  echo "(nested in: ${WRAP[*]})"
fi
set +e
timeout -k 5 "$TIMEOUT" "${WRAP[@]}" "$EXE" "${ARGS[@]}"
RC=$?
echo "exit code $RC$([ $RC = 124 ] && echo ' (timeout: stopped by this script)')"
exit $RC
