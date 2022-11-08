/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
#define PAGE_SIZE           4096

ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* read coreid */
  csrr t2, CSR_MHARTID
  bne  t2, zero, secondary

  /* clear BSS for flat non-ELF images */
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

  /* setup sp */
  la  sp, KERNEL_STACK
  call main

secondary:
  /* setup sp */
  la  sp, KERNEL_STACK
  la  t0, PAGE_SIZE
  add sp, sp, t0
  call main

loop:
  wfi
  j loop

END(_start)
