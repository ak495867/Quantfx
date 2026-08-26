BITS 64
default rel

global efi_main

section .text

efi_main:
    sub rsp,40
    mov rax,[rdx+64]
    lea r8,[rel qfx_uefi_message]
    mov rcx,rax
    mov rdx,r8
    call [rax+8]
    xor eax,eax
    add rsp,40
    ret

section .data
qfx_uefi_message dw 'Q','F','X',' ','U','E','F','I',' ','R','E','A','D','Y',13,10,0
