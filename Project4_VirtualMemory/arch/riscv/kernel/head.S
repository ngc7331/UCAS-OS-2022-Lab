/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define PAGE_SIZE           4096

ENTRY(_start)
  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

  /* read coreid */
  csrr t2, CSR_MHARTID
  bne  t2, zero, secondary

  /* clear BSS for flat non-ELF images */
  la  t0, __bss_start
  la  t1, _end
  // for (i=__bss_start; i<_end; i+=4) MEM[i]=0;
  bge  t0, t1, clend
clstart:
  sw   zero, (t0)
  addi t0, t0, 4
  blt  t0, t1, clstart
clend:

  /* setup sp */
  la   t0, pid0_stack
  ld   sp, (t0)
  call main

secondary:
  /* setup sp */
  la   t0, pid0_stack
  addi t0, t0, 8
  ld   sp, (t0)
  call main

loop:
  wfi
  j loop

END(_start)
