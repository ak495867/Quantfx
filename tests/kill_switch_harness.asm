BITS 64

global _start
extern qfx_kill_switch_simulate
extern qfx_kill_switch_active

section .text
_start:
    call qfx_kill_switch_active
    test eax,eax
    jnz fail
    call qfx_kill_switch_simulate
    call qfx_kill_switch_active
    cmp eax,1
    jne fail
    mov eax,60
    xor edi,edi
    syscall
fail:
    mov eax,60
    mov edi,1
    syscall
