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
2. If not found after 5 consecutive scan misses (~25s), automatically toggle the BT radio to unstick the WinRT scanner. This is retryable — if one toggle doesn't recover discovery it re-arms after another run of misses, up to 3 attempts with a cooldown, then falls back to polling rather than power-cycling the radio indefinitely. (Toggling the radio briefly drops *all* the machine's Bluetooth devices, hence the conservative threshold.)
3. Connect within ~18 seconds once the device is discoverable

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

**"buddy: off" in hud but device is on:** The daemon is running but BLE is disconnected. Wait for the radio reset to fire (~25s of misses), or toggle BT.

**Connects but immediately drops, hud shows "No Claude connected", daemon log shows `start_notify ... Unreachable`:** The Windows BLE bond is stale/mismatched (can happen if the device was paired to another app like the Claude Desktop hardware-buddy window and then unpaired). The encrypted GATT subscription can't establish. Fix: remove the **Claude-XXXX** device in Windows Settings → Bluetooth, then re-pair it. (Encountered 2026-06; re-pairing fully resolves it.)

**Hooks not firing:** Check `~/.claude/settings.json` has SessionStart/SessionEnd/PreToolUse/PostToolUse/UserPromptSubmit/Stop hooks pointing to cc-buddy-bridge. Re-run `cc-buddy-bridge install` if missing.

**Multiple machines:** The BLE stack stores up to 15 bonds (`CONFIG_BT_SMP_MAX_BONDS`), so the device can remember several hosts. Each machine pairs independently via Windows Settings — pair normally on the new machine.

The intended behaviour: when you move between machines, the device reconnects to whichever host's daemon finds it first; the machine you left loses the connection, attempts a radio reset, then falls back to 60s polling without interfering with the active host.

> ⚠️ **Multi-host roaming is capability-present but NOT yet verified between two live daemon hosts** (as of 2026-06). Bond management on this ESP32 stack has proven fragile — notably, unpairing the device from one host (e.g. the Claude Desktop hardware-buddy window) was observed to break the *encryption* bond for a different host, requiring a re-pair (see the "Unreachable" troubleshooting entry above). Before relying on roaming: pair the second machine, confirm the **first** machine's connection still works afterwards, then test actually moving between them. If a host stops connecting with the "Unreachable" symptom, re-pair it on that machine. Do NOT pair the device to the Claude Desktop hardware-buddy window — it competes for the single connection slot and disturbs the daemon bonds.
