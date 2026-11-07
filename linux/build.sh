#!/bin/bash
# Cross-compiles the plugin DLL (32-bit, like tSIP) on Linux using mingw-w64.
# Requires: g++-mingw-w64-i686, and the tSIP source tree at <repo>/../tSIP (providing tSIP/phone/Phone.h etc.),
# i.e. next to this repository directory.
# Usage: ./build.sh [Win10|Win7]      -> ../bin/Release/PhonePolycomCX300_<target>.dll
set -e
cd "$(dirname "$0")/.."

TARGET="${1:-Win10}"

command -v i686-w64-mingw32-g++ >/dev/null || {
    echo "Missing cross compiler. Install it with: sudo apt install g++-mingw-w64-i686" >&2
    exit 1
}
[ -f ../tSIP/tSIP/phone/Phone.h ] || {
    echo "tSIP sources not found: expected $(cd .. && pwd)/tSIP/tSIP/phone/Phone.h" >&2
    exit 1
}

make TARGET="$TARGET" -j"$(nproc)"
echo "Built: $(pwd)/bin/Release/PhonePolycomCX300_${TARGET}.dll"
