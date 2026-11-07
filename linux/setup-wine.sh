#!/bin/bash
# Configures a Wine prefix so that Wine exposes the Polycom CX300 (hidraw) to Windows applications.
#
# Wine's default "winebus" driver only starts its SDL backend, which ignores hidraw
# devices such as the CX300. Setting "Enable SDL" = 0 makes it use the udev/hidraw backend.
# Side effect: SDL gamepad support is disabled in this prefix (tSIP does not use it).
#
# Usage: ./setup-wine.sh [WINEPREFIX]      (default: $WINEPREFIX or ~/.wine)
#        ./setup-wine.sh --revert [WINEPREFIX]
set -e

ACTION=set
if [ "$1" = "--revert" ]; then
    ACTION=revert
    shift
fi

export WINEPREFIX="${1:-${WINEPREFIX:-$HOME/.wine}}"
export WINEDEBUG=-all
KEY='HKLM\System\CurrentControlSet\Services\winebus'

command -v wine >/dev/null || { echo "wine not found in PATH" >&2; exit 1; }

echo "Wine prefix: $WINEPREFIX"
# make sure no running wineserver keeps old settings
wineserver -k 2>/dev/null || true

if [ "$ACTION" = "set" ]; then
    wine reg add "$KEY" /v "Enable SDL" /t REG_DWORD /d 0 /f
    echo "Done: winebus uses hidraw now. Restart tSIP."
else
    wine reg delete "$KEY" /v "Enable SDL" /f
    echo "Reverted to Wine default."
fi
wineserver -k 2>/dev/null || true
