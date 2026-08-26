BITS 16
ORG 0x7c00

start:
    cli
    xor ax,ax
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x7c00
    mov si,message
.print:
    lodsb
    test al,al
    jz .halt
    mov ah,0x0e
    mov bx,0x0007
    int 0x10
    jmp .print
.halt:
    hlt
    jmp .halt

message db 'QFX BIOS READY',0
TIMES 510-($-$$) db 0
DW 0xaa55
