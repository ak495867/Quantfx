BITS 64
default rel

global qfx_kill_irq_handler
global qfx_kill_switch_trip
global qfx_kill_switch_active
global qfx_kill_switch_simulate

section .bss
qfx_kill_latched resb 1

section .text
qfx_kill_irq_handler:
    push rax
    cli
    mov byte [qfx_kill_latched],1
    mov al,0x20
    out 0x20,al
    pop rax
    iretq

qfx_kill_switch_trip:
    cli
    mov byte [qfx_kill_latched],1
    ret

qfx_kill_switch_simulate:
    mov byte [qfx_kill_latched],1
    ret

qfx_kill_switch_active:
    movzx eax,byte [qfx_kill_latched]
    ret
