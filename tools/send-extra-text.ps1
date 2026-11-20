# Sends text to be shown on the Polycom CX300 display (after the clock) via the plugin's
# loopback UDP listener (see "extraTextUdpPort" in PhonePolycomCX300_Win10.cfg).
# Usage: .\send-extra-text.ps1 "Claude 50%/20%"      (empty text clears it)
param(
    [string]$Text = "",
    [int]$Port = 47123
)
$client = New-Object System.Net.Sockets.UdpClient
try {
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($Text + "`n")
    [void]$client.Send($bytes, $bytes.Length, "127.0.0.1", $Port)
} finally {
    $client.Close()
}
