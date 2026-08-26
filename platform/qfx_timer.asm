BITS 64
default rel

global qfx_timer_init_pit
global qfx_timer_read_tsc
global qfx_timer_tick

global qfx_irq_default

global qfx_irq_install_idt

global qfx_irq_enable

global qfx_irq_disable

section .bss
qfx_tick_count resq 1
qfx_idt resb 4096

section .text
qfx_timer_init_pit:
    mov al,0x34
    out 0x43,al
    mov eax,1193
    out 0x40,al
    mov al,ah
    out 0x40,al
    xor eax,eax
    ret

qfx_timer_read_tsc:
    rdtsc
    shl rdx,32
    or rax,rdx
    ret

qfx_timer_tick:
    inc qword [qfx_tick_count]
    ret

qfx_irq_default:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    call qfx_timer_tick
    mov al,0x20
    out 0x20,al
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

qfx_irq_install_idt:
    lea rdi,[qfx_idt]
    xor eax,eax
    mov ecx,4096/8
    rep stosq
    lea rax,[rel qfx_irq_default]
    lea rdi,[qfx_idt+32*16]
    mov word [rdi],ax
    mov word [rdi+2],0x08
    mov byte [rdi+4],0
    mov byte [rdi+5],0x8e
    shr rax,16
    mov word [rdi+6],ax
    shr rax,16
    mov dword [rdi+8],eax
    mov qword [rdi+12],0
    lidt [qfx_idtr]
    ret

qfx_irq_enable:
    sti
    ret

qfx_irq_disable:
    cli
    ret

section .data
qfx_idtr:
    dw 4095
    dq qfx_idt
