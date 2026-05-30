# Windows Setup — cc-buddy-bridge

Step-by-step for a new Windows machine. Assumes Python 3.11+ is already installed.

## 1. Clone and install cc-buddy-bridge

Use the Swissola fork which includes the Windows BLE radio auto-reset fix (upstream PR #15).

```powershell
git clone https://github.com/Swissola/cc-buddy-bridge.git
cd cc-buddy-bridge
git checkout fix/windows-ble-radio-reset
pip install -e .
```

## 2. Install Claude Code hooks

This wires up the session/tool hooks so the daemon receives live session data:

```powershell
cc-buddy-bridge install
```

## 3. Pair the device

Power on the M5StickC Plus. In Windows Settings → Bluetooth & devices, pair it — it will appear as **Claude-XXXX**. No PIN required (just works pairing).

## 4. Set up the daemon as a silent startup task

`cc-buddy-bridge install --service` creates a Task Scheduler entry but the log redirect doesn't work correctly on Windows. Use this instead:

```powershell
# Run this once in an elevated (admin) PowerShell prompt

$logDir = "$env:LOCALAPPDATA\cc-buddy-bridge"
New-Item -ItemType Directory -Force $logDir | Out-Null

# Find the cc-buddy-bridge executable
$exe = (Get-Command cc-buddy-bridge).Source

# Write the silent VBScript launcher
@"
Set oShell = CreateObject("WScript.Shell")
oShell.Run """$exe"" daemon", 0, False
"@ | Set-Content "$logDir\start-daemon.vbs"

# Register the startup task (runs silently at login, no window)
schtasks /create /tn "cc-buddy-bridge-daemon" /tr "wscript.exe ""$logDir\start-daemon.vbs""" /sc onlogon /f

Write-Host "Done. Log will appear at: $logDir\daemon.log"
Write-Host "Start now with: schtasks /run /tn cc-buddy-bridge-daemon"
```

## 5. Start the daemon

```powershell
schtasks /run /tn "cc-buddy-bridge-daemon"
```

The daemon will:

1. Scan for the device
2. If not found after 2 scans (~9s), automatically toggle the BT radio to unstick the WinRT scanner
3. Connect within ~18 seconds of starting

From the next login onwards it starts silently in the background automatically.

## 6. Verify

```powershell
# Should show battery %, token count, session state
$env:PYTHONUTF8=1; cc-buddy-bridge hud --ascii
```

## Logs

```text
%LOCALAPPDATA%\cc-buddy-bridge\daemon.log
```

## Troubleshooting

**Device not connecting after 60s:** Toggle Bluetooth off and on in Windows Settings. The daemon will reconnect automatically within ~18s.

**"buddy: off" in hud but device is on:** The daemon is running but BLE is disconnected. Wait ~18s for the radio reset to fire, or toggle BT.

**Hooks not firing:** Check `~/.claude/settings.json` has SessionStart/SessionEnd/PreToolUse/PostToolUse/UserPromptSubmit/Stop hooks pointing to cc-buddy-bridge. Re-run `cc-buddy-bridge install` if missing.

**Multiple machines:** The firmware supports up to 7 bonded hosts. Each machine pairs independently — just pair normally on the new machine. The device reconnects to whichever host is in range and has the daemon running. No need to clear bonds when switching.
