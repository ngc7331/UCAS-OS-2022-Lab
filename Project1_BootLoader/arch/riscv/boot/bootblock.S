#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence

	// call BIOS to print string "It's Boot Loader!"
    li   a0, 9
    lui  a1, %hi(msg)
    addi a1, a1, %lo(msg)
    call bios_func_entry


	// TODO: [p1-task2] call BIOS to read kernel in SD card


	// TODO: [p1-task4] load task-related arguments and pass them to kernel


	// TODO: [p1-task2] jump to kernel to start UCAS-OS


// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's Boot Loader!\n\r"

