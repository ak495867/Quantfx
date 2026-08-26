BITS 64
default rel

global qfx_nt_api_init
global qfx_nt_read
global qfx_nt_write
global qfx_nt_open
global qfx_nt_close
global qfx_nt_seek
global qfx_nt_sync
global qfx_nt_clock
global qfx_nt_net_open
global qfx_nt_net_read
global qfx_nt_net_write
global qfx_nt_net_close
global qfx_nt_start

%define QFX_NT_STATUS_NOT_IMPLEMENTED -1
%define QFX_NT_TABLE_READ 8
%define QFX_NT_TABLE_WRITE 16
%define QFX_NT_TABLE_OPEN 24
%define QFX_NT_TABLE_CLOSE 32
%define QFX_NT_TABLE_SEEK 40
%define QFX_NT_TABLE_SYNC 48
%define QFX_NT_TABLE_CLOCK 72
%define QFX_NT_TABLE_NET_OPEN 112
%define QFX_NT_TABLE_NET_READ 120
%define QFX_NT_TABLE_NET_WRITE 128
%define QFX_NT_TABLE_NET_CLOSE 136

section .bss
qfx_nt_dispatch resq 1

section .text
qfx_nt_api_init:
    mov [qfx_nt_dispatch],rdi
    xor eax,eax
    ret

qfx_nt_read:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_READ]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_write:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_WRITE]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_open:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_OPEN]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_close:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_CLOSE]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_seek:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_SEEK]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_sync:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_SYNC]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_clock:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_CLOCK]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_net_open:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_NET_OPEN]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_net_read:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_NET_READ]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_net_write:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_NET_WRITE]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_net_close:
    mov r11,[qfx_nt_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_NT_TABLE_NET_CLOSE]
.fail:
    mov eax,QFX_NT_STATUS_NOT_IMPLEMENTED
    ret

qfx_nt_start:
    xor eax,eax
    ret
