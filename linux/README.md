# Linux (Wine) support

tSIP is a Windows application, so on Linux it runs under **Wine**. This plugin talks to the
Polycom CX300 through the Windows HID API, which Wine maps to Linux `hidraw`.
This folder contains what is needed to make that work and to build the plugin on Linux.

Tested with: Ubuntu 24.04, Wine 9.0 (distribution package), kernel 6.8, Polycom CX300 (`095d:9201`).

## Quick start

```bash
cd linux
./install-udev-rule.sh     # 1. allow your user to access the phone's /dev/hidraw* (asks for sudo)
./setup-wine.sh            # 2. make Wine use hidraw (default prefix ~/.wine)
# unplug and replug the phone once, then start tSIP:
cd /path/to/tSIP && wine tSIP.exe
```

You need a DLL built from the current sources (see [Building on Linux](#building-on-linux)):
releases made before the Wine changes cannot find the phone's vendor-page (display) device under Wine
and will keep disconnecting. Put the DLL in tSIP's `phone` directory and select it in tSIP settings
(`"dllName"` in `phoneConf`); `install-plugin.sh` does the copy.

## Scripts

| Script | What it does |
|---|---|
| `install-udev-rule.sh` | Installs `/etc/udev/rules.d/70-polycom-cx300.rules` (`MODE=0660`, `TAG+="uaccess"` for `095d:9201`). Needs sudo. |
| `setup-wine.sh [WINEPREFIX]` | Sets `HKLM\System\CurrentControlSet\Services\winebus` -> `Enable SDL = 0` in the Wine prefix. `--revert` removes it. |
| `build.sh [Win10\|Win7]` | Cross-compiles the DLL with mingw-w64 (wrapper for the top-level `Makefile`). |
| `install-plugin.sh /path/to/tSIP [Win10\|Win7]` | Copies the built DLL to `<tSIP>/phone/`, keeping the original as `.orig`. |

## Why these steps are needed

1. **udev rule** - `/dev/hidraw*` is `root:root 0600` by default. Wine's `winebus` driver runs as
   your user, so without access it never sees the phone. Symptom: `Error opening HID device: Device not found`.
   Check with `getfacl /dev/hidraw*` - the CX300 node should list your user.
2. **`Enable SDL = 0`** - the Wine 9 `winebus` driver starts only its SDL backend by default, which
   ignores hidraw devices. Disabling it starts the udev/hidraw backend instead. Side effect: SDL
   gamepad support is disabled in that prefix. To check which backend is active run
   `WINEDEBUG=+hid wine ...` and look for `bus_main_thread L"UDEV" main loop starting`
   (versus `L"SDL"`).
3. **Plugin changes** - the CX300 report descriptor contains two top-level collections in one USB
   interface (Telephony `0x0B` and vendor `0xFF99`). Windows exposes them as two HID devices, Wine 9 exposes one.
   The plugin therefore falls back to using the same device for the vendor-page reports
   (display, LEDs, keepalive). If those writes are still rejected, the plugin logs it once, disables the
   display/LED/keepalive features and keeps the telephony part (hook, keys) working,
   instead of reconnecting in a loop.

## Building on Linux

```bash
sudo apt install g++-mingw-w64-i686      # tSIP and its plugins are 32-bit
```

The tSIP source tree must be next to this repository, as in the Code::Blocks project:

```
some_dir/
  tSIP/tSIP/phone/Phone.h ...            <- tSIP sources
  tSIP-plugin-PhonePolycomCX300/         <- this repository
```

Then:

```bash
linux/build.sh                 # -> bin/Release/PhonePolycomCX300_Win10.dll
linux/install-plugin.sh ~/tSIP
```

`make` can be used directly too (`make TARGET=Win7`, `make OUTDIR=/some/dir`, `make clean`).
The same sources still build with the original Code::Blocks/MinGW project on Windows
(`HidHeaders.h` selects the right HID headers for old and new MinGW).

## Troubleshooting

| Symptom (tSIP log) | Cause / fix |
|---|---|
| `Error opening HID device: Device not found` | udev rule missing / phone not replugged, or `setup-wine.sh` not run for this prefix. |
| `HID device for telephony connected` repeating every ~10 s | Old plugin version: display device not found -> disconnect loop. Use the current DLL. |
| `... disabling display/LED/keepalive support` | Wine rejected the vendor-page reports. Telephony still works; try a newer Wine. |
| Works as root only | The udev ACL is not applied - replug the phone, check `getfacl /dev/hidraw*`. |

Useful debug command:

```bash
WINEDEBUG=err+all,warn+hid,+hidraw wine tSIP.exe 2>&1 | tee wine.log
```

The plugin's own messages appear in tSIP's log (lines starting with `PhonePolycomCX300_Win10.dll:`).
