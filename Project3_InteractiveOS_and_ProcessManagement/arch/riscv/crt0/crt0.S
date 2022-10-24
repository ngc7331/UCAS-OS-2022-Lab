#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)
    // setup C runtime environment for the user program
    // push return address
    addi sp, sp, -8
    sd   ra, (sp)

    // clear .BSS
    lui  t0, %hi(__bss_start)
    addi t0, t0, %lo(__bss_start)
    lui  t1, %hi(_end)
    addi t1, t1, %lo(_end)
    // for (i=__bss_start; i<_end; i+=4) MEM[i]=0;
    bge  t0, t1, clend
clstart:
    sw   zero, (t0)
    addi t0, t0, 4
    blt  t0, t1, clstart
clend:

    // enter main function
    call main

    // finish task and return to the kernel
    // ld   ra, (sp)
    // addi sp, sp, 8
    // jr    ra

    /* call sys_exit */
    call sys_exit

// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
