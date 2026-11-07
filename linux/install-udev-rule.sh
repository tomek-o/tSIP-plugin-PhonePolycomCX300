#!/bin/bash
# Installs a udev rule giving the logged-in user access to the Polycom CX300
# HID interfaces (needed for Wine's winebus / hidraw access).
set -e

RULE_FILE=/etc/udev/rules.d/70-polycom-cx300.rules

if [ "$(id -u)" -ne 0 ]; then
    exec sudo "$0" "$@"
fi

cat > "$RULE_FILE" <<'EOF'
# Polycom CX300 USB desk phone (tSIP plugin under Wine)
KERNEL=="hidraw*", ATTRS{idVendor}=="095d", ATTRS{idProduct}=="9201", MODE="0660", TAG+="uaccess"
EOF

udevadm control --reload
udevadm trigger --subsystem-match=hidraw --action=change

echo "Installed $RULE_FILE"
echo "If permissions on /dev/hidraw* don't change, unplug and replug the phone."
ls -l /dev/hidraw*
