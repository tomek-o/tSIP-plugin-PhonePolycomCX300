#!/bin/bash
# Sends text to be shown on the Polycom CX300 display (after the clock) via the plugin's
# loopback UDP listener (see "extraTextUdpPort" in PhonePolycomCX300_Win10.cfg).
# Usage: send-extra-text.sh "Claude 50%/20%"      (empty text clears it)
# Port:  EXTRA_TEXT_PORT env variable, default 47123
PORT="${EXTRA_TEXT_PORT:-47123}"
# a trailing newline makes an empty text a valid (empty) datagram
printf '%s\n' "$1" > "/dev/udp/127.0.0.1/$PORT"
