.code32

.section .text
.globl _start
_start:
    # set selectors
    movw    $0x0010, %ax    // index = 2 => 3rd entry
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    # set up the stack
    movl    $0x00080000, %esp

    jmp     main
hang:
    jmp hang

.section .data
msg:
    .asciz  "kboot86"
