#include <asm/biosdef.h>
#include <csr.h>

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

    // a0 is mhartid
	bnez a0, secondary

	// call BIOS to print string "It's Boot Loader!"
	li   a0, 9
	lui  a1, %hi(msg)
	addi a1, a1, %lo(msg)
	call bios_func_entry

	// call BIOS to read kernel in SD card
	li   a0, 11
	lui  a1, %hi(kernel)
	addi a1, a1, %lo(kernel)
	lui  a2, %hi(os_size_loc)
	addi a2, a2, %lo(os_size_loc)
	lh   a2, (a2)
	li   a3, 1
	call bios_func_entry

	// jump to kernel to start UCAS-OS
	j    kernel

secondary:
	// 1. Mask all interrupts
	csrw CSR_SIE, zero
	li   t0, SR_SIE
	csrw CSR_SSTATUS, t0
	// 2. let stvec pointer to kernel_main
	la   t0, kernel
	csrw CSR_STVEC, t0
	// 3. enable software interrupt for ipi
	li   t0, SIE_SSIE
	csrw CSR_SIE, t0

wait_for_wakeup:
	wfi
	j wait_for_wakeup

// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "[bootloader] It's Boot Loader!\n\r"

