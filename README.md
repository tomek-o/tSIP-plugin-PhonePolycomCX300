# tSIP-plugin-PhonePolycomCX300

tSIP plugin for Polycom CX300 USB desk phone.

![Polycom_CX300](https://tomeko.net/software/SIPclient/Polycom_CX300/Polycom_CX300_01.jpg)

Based on the info from https://github.com/probonopd/OpenPhone.

https://tomeko.net/software/SIPclient/Polycom_CX300/

## Extra display text

The plugin can show a short extra text on the idle screen, after the clock (e.g. `14:32:07   Claude 50%/20%`).
The text is set from outside, so any script can feed it. Two sources (the last update wins):

- UDP datagram to `127.0.0.1:<extraTextUdpPort>` (loopback only),
- first line of a text file (`extraTextFile`), re-read when it changes.

Enable it in `PhonePolycomCX300_Win10.cfg` (edit while tSIP is closed), inside `customConf`:

```json
"customConf" : {
   "extraTextUdpPort" : 47123,
   "extraTextFile" : "",
   "extraTextMaxChars" : 14,
   "extraTextSeparator" : "   "
}
```

`extraTextUdpPort = 0` (default) disables the listener. The display uses a variable-width font, so
`extraTextMaxChars` is only an approximate limit - tune it on your phone. Only printable ASCII is shown.

Send text:

```bash
tools/send-extra-text.sh "Claude 50%/20%"          # Linux / Git Bash
powershell -File tools\send-extra-text.ps1 "Claude 50%/20%"   # Windows
echo "text" > /dev/udp/127.0.0.1/47123            # bash only, no script needed
```

Windows setup for the extra text and the Claude Code usage feed: [windows/README.md](windows/README.md).

## Linux (Wine)

The plugin also works with tSIP running under Wine on Linux. See [linux/README.md](linux/README.md) for setup (udev rule, Wine settings) and for building the DLL with mingw-w64.
