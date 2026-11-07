#!/bin/bash
# Copies the built plugin DLL into tSIP's "phone" directory (backing up the existing one as .orig).
# Usage: ./install-plugin.sh /path/to/tSIP [Win10|Win7]
set -e
cd "$(dirname "$0")/.."

TSIP_DIR="${1:?usage: $0 /path/to/tSIP [Win10|Win7]}"
TARGET="${2:-Win10}"
NAME="PhonePolycomCX300_${TARGET}.dll"
SRC="bin/Release/$NAME"
DST="$TSIP_DIR/phone/$NAME"

[ -f "$SRC" ] || { echo "$SRC not found - run linux/build.sh first" >&2; exit 1; }
[ -d "$TSIP_DIR/phone" ] || { echo "$TSIP_DIR/phone does not exist" >&2; exit 1; }

if [ -f "$DST" ] && [ ! -f "$DST.orig" ]; then
    cp -p "$DST" "$DST.orig"
    echo "Backup: $DST.orig"
fi
cp "$SRC" "$DST"
echo "Installed: $DST"
echo "Make sure tSIP.json has \"dllName\" : \"$NAME\" in phoneConf."
