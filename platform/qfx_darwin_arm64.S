.text

.globl _qfx_darwin_arm64_write
.globl _qfx_darwin_arm64_read
.globl _qfx_darwin_arm64_close

_qfx_darwin_arm64_write:
    mov x16, #4
    svc #0x80
    ret

_qfx_darwin_arm64_read:
    mov x16, #3
    svc #0x80
    ret

_qfx_darwin_arm64_close:
    mov x16, #6
    svc #0x80
    ret
