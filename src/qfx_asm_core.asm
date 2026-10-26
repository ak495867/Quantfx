default rel
BITS 64

global qfx_asm_risk_check
global qfx_asm_fast_score

section .text

qfx_asm_risk_check:
    mov rax,rdi
    test rax,rax
    jns .abs_pos
    neg rax
.abs_pos:
    cmp rax,1000000
    ja .fail
    cmp rsi,1000
    jb .fail
    xor eax,eax
    ret
.fail:
    mov eax,1
    ret

qfx_asm_fast_score:
    mov rax,rsi
    sub rax,rdi
    mov r8,rdx
    imul r8,100
    add rax,r8
    add rax,rcx
    add rax,r8
    ret
