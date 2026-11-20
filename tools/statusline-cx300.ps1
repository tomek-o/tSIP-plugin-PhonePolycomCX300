# Claude Code status line command (Windows / PowerShell) that also pushes the rate-limit usage
# to the Polycom CX300 display. Equivalent of statusline-cx300.sh.
# Receives the status-line JSON on stdin, prints the terminal status line and sends
# "CC: <5h>%/<7d>%" to the plugin's loopback UDP listener (see send-extra-text.ps1).
# Nothing is sent when the JSON has no rate_limits, so the last text stays on the phone.
# Setup (user settings, %USERPROFILE%\.claude\settings.json), use forward slashes in the path:
#   "statusLine": { "type": "command",
#     "command": "powershell -NoProfile -ExecutionPolicy Bypass -File C:/path/to/tools/statusline-cx300.ps1" }
$ErrorActionPreference = 'SilentlyContinue'

$raw = [Console]::In.ReadToEnd()
try { $d = $raw | ConvertFrom-Json } catch { exit 0 }
if ($null -eq $d) { exit 0 }

function Round-Pct($v) { return [int][math]::Floor([double]$v + 0.5) }

$parts = @()
if ($d.model.display_name) { $parts += $d.model.display_name }
$ctx  = $d.context_window.used_percentage
$five = $d.rate_limits.five_hour.used_percentage
$week = $d.rate_limits.seven_day.used_percentage
if ($null -ne $ctx) { $parts += ("ctx {0}%" -f (Round-Pct $ctx)) }
if ($null -ne $five -and $null -ne $week) {
    $parts += ("5h {0}% 7d {1}%" -f (Round-Pct $five), (Round-Pct $week))
}
Write-Output ($parts -join ' | ')

if ($null -ne $five -and $null -ne $week) {
    $port = 47123
    if ($env:EXTRA_TEXT_PORT) { $port = [int]$env:EXTRA_TEXT_PORT }
    $text = "CC: {0}%/{1}%" -f (Round-Pct $five), (Round-Pct $week)
    & (Join-Path $PSScriptRoot 'send-extra-text.ps1') -Text $text -Port $port
}
exit 0
