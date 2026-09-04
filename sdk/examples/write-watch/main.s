.section .text, code
.global entry
.global result
.local loop
entry:
    LDAA #$42
    STAA result
    LDAA #$99
    STAA result + 1
loop:
    BRA loop
.section .bss, bss
result:
    .space 2
