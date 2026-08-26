BITS 64
default rel

global qfx_darwin_api_init
global qfx_darwin_read
global qfx_darwin_write
global qfx_darwin_open
global qfx_darwin_close
global qfx_darwin_seek
global qfx_darwin_sync
global qfx_darwin_clock
global qfx_darwin_net_open
global qfx_darwin_net_read
global qfx_darwin_net_write
global qfx_darwin_net_close
global qfx_darwin_start

%define QFX_DARWIN_STATUS_NOT_IMPLEMENTED -1
%define QFX_DARWIN_TABLE_READ 8
%define QFX_DARWIN_TABLE_WRITE 16
%define QFX_DARWIN_TABLE_OPEN 24
%define QFX_DARWIN_TABLE_CLOSE 32
%define QFX_DARWIN_TABLE_SEEK 40
%define QFX_DARWIN_TABLE_SYNC 48
%define QFX_DARWIN_TABLE_CLOCK 72
%define QFX_DARWIN_TABLE_NET_OPEN 112
%define QFX_DARWIN_TABLE_NET_READ 120
%define QFX_DARWIN_TABLE_NET_WRITE 128
%define QFX_DARWIN_TABLE_NET_CLOSE 136

section .bss
qfx_darwin_dispatch resq 1

section .text
qfx_darwin_api_init:
    mov [qfx_darwin_dispatch],rdi
    xor eax,eax
    ret

qfx_darwin_read:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_READ]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_write:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_WRITE]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_open:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_OPEN]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_close:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_CLOSE]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_seek:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_SEEK]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_sync:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_SYNC]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_clock:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_CLOCK]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_net_open:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_NET_OPEN]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_net_read:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_NET_READ]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_net_write:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_NET_WRITE]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_net_close:
    mov r11,[qfx_darwin_dispatch]
    test r11,r11
    jz .fail
    jmp [r11+QFX_DARWIN_TABLE_NET_CLOSE]
.fail:
    mov eax,QFX_DARWIN_STATUS_NOT_IMPLEMENTED
    ret

qfx_darwin_start:
    xor eax,eax
    ret
