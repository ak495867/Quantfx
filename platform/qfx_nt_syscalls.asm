BITS 64
default rel

global qfx_nt_syscall0
global qfx_nt_syscall1
global qfx_nt_syscall2
global qfx_nt_syscall3
global qfx_nt_syscall4
global qfx_nt_syscall5
global qfx_nt_syscall6

global qfx_nt_ids

section .data
qfx_nt_ids dq 0,0,0,0,0,0,0

section .text
qfx_nt_syscall0:
    mov eax,[qfx_nt_ids]
    mov r10,rcx
    syscall
    ret

qfx_nt_syscall1:
    mov eax,[qfx_nt_ids+8]
    mov r10,rcx
    syscall
    ret

qfx_nt_syscall2:
    mov eax,[qfx_nt_ids+16]
    mov r10,rcx
    syscall
    ret

qfx_nt_syscall3:
    mov eax,[qfx_nt_ids+24]
    mov r10,rcx
    syscall
    ret

qfx_nt_syscall4:
    mov eax,[qfx_nt_ids+32]
    mov r10,rcx
    syscall
    ret

qfx_nt_syscall5:
    mov eax,[qfx_nt_ids+40]
    mov r10,rcx
    syscall
    ret

qfx_nt_syscall6:
    mov eax,[qfx_nt_ids+48]
    mov r10,rcx
    syscall
    ret
