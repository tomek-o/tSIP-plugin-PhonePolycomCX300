#!/bin/bash
# Claude Code status line command that also pushes the rate-limit usage to the Polycom CX300 display.
# Receives the status-line JSON on stdin, prints the terminal status line on stdout and sends
# "CC: <5h>%/<7d>%" to the plugin's loopback UDP listener (see tools/send-extra-text.sh).
# Nothing is sent when the JSON has no rate_limits (e.g. API-key usage), so the last text stays.
# Setup (user settings, ~/.claude/settings.json):
#   "statusLine": { "type": "command", "command": "/path/to/tools/statusline-cx300.sh" }
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 -c '
import json, sys
try:
    d = json.load(sys.stdin)
except Exception:
    sys.exit(0)
model = (d.get("model") or {}).get("display_name", "")
ctx = (d.get("context_window") or {}).get("used_percentage")
rl = d.get("rate_limits") or {}
five = (rl.get("five_hour") or {}).get("used_percentage")
week = (rl.get("seven_day") or {}).get("used_percentage")
parts = [model]
if ctx is not None:
    parts.append("ctx %s%%" % round(ctx))
if five is not None and week is not None:
    parts.append("5h %s%% 7d %s%%" % (round(five), round(week)))
print(" | ".join(p for p in parts if p))
# second output line: text for the phone (empty = nothing to send)
if five is not None and week is not None:
    print("CC: %d%%/%d%%" % (round(five), round(week)))
' | {
    read -r line1
    read -r phone_text
    printf '%s\n' "$line1"
    if [ -n "$phone_text" ]; then
        "$DIR/send-extra-text.sh" "$phone_text" 2>/dev/null
    fi
}
exit 0
