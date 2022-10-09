#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  // save user stack pointer to pcb
  sd   sp, PCB_USER_SP(tp)
  // load kernel stack pointer from pcb
  ld   sp, PCB_KERNEL_SP(tp)
  // save all regs(except sp & tp) to kernel stack
  sd   ra, OFFSET_REG_RA(sp)
  sd   tp, OFFSET_REG_TP(sp)
  sd   gp, OFFSET_REG_GP(sp)
  sd   t0, OFFSET_REG_T0(sp)
  sd   t1, OFFSET_REG_T1(sp)
  sd   t2, OFFSET_REG_T2(sp)
  sd   t3, OFFSET_REG_T3(sp)
  sd   t4, OFFSET_REG_T4(sp)
  sd   t5, OFFSET_REG_T5(sp)
  sd   t6, OFFSET_REG_T6(sp)
  sd   a0, OFFSET_REG_A0(sp)
  sd   a1, OFFSET_REG_A1(sp)
  sd   a2, OFFSET_REG_A2(sp)
  sd   a3, OFFSET_REG_A3(sp)
  sd   a4, OFFSET_REG_A4(sp)
  sd   a5, OFFSET_REG_A5(sp)
  sd   a6, OFFSET_REG_A6(sp)
  sd   a7, OFFSET_REG_A7(sp)
  sd   s0, OFFSET_REG_S0(sp)
  sd   s1, OFFSET_REG_S1(sp)
  sd   s2, OFFSET_REG_S2(sp)
  sd   s3, OFFSET_REG_S3(sp)
  sd   s4, OFFSET_REG_S4(sp)
  sd   s5, OFFSET_REG_S5(sp)
  sd   s6, OFFSET_REG_S6(sp)
  sd   s7, OFFSET_REG_S7(sp)
  sd   s8, OFFSET_REG_S8(sp)
  sd   s9, OFFSET_REG_S9(sp)
  sd   s10, OFFSET_REG_S10(sp)
  sd   s11, OFFSET_REG_S11(sp)
  // save user stack pointer to kernel stack
  ld   t0, PCB_USER_SP(tp)
  sd   t0, OFFSET_REG_SP(sp)

  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li   t0, SR_SUM | SR_FS
  csrc CSR_SSTATUS, t0

  // save sstatus, sepc, stval and scause on kernel stack
  csrr t0, CSR_SSTATUS
  sd   t0, OFFSET_REG_SSTATUS(sp)
  csrr t0, CSR_SEPC
  sd   t0, OFFSET_REG_SEPC(sp)
  csrr t0, CSR_STVAL
  sd   t0, OFFSET_REG_SBADADDR(sp)
  csrr t0, CSR_SCAUSE
  sd   t0, OFFSET_REG_SCAUSE(sp)
.endm

.macro RESTORE_CONTEXT
  /* Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */

  // load all regs(except sp & tp) from kernel stack
  ld   ra, OFFSET_REG_RA(sp)
  ld   gp, OFFSET_REG_GP(sp)
  ld   t0, OFFSET_REG_T0(sp)
  ld   t1, OFFSET_REG_T1(sp)
  ld   t2, OFFSET_REG_T2(sp)
  ld   t3, OFFSET_REG_T3(sp)
  ld   t4, OFFSET_REG_T4(sp)
  ld   t5, OFFSET_REG_T5(sp)
  ld   t6, OFFSET_REG_T6(sp)
  ld   a0, OFFSET_REG_A0(sp)
  ld   a1, OFFSET_REG_A1(sp)
  ld   a2, OFFSET_REG_A2(sp)
  ld   a3, OFFSET_REG_A3(sp)
  ld   a4, OFFSET_REG_A4(sp)
  ld   a5, OFFSET_REG_A5(sp)
  ld   a6, OFFSET_REG_A6(sp)
  ld   a7, OFFSET_REG_A7(sp)
  ld   s0, OFFSET_REG_S0(sp)
  ld   s1, OFFSET_REG_S1(sp)
  ld   s2, OFFSET_REG_S2(sp)
  ld   s3, OFFSET_REG_S3(sp)
  ld   s4, OFFSET_REG_S4(sp)
  ld   s5, OFFSET_REG_S5(sp)
  ld   s6, OFFSET_REG_S6(sp)
  ld   s7, OFFSET_REG_S7(sp)
  ld   s8, OFFSET_REG_S8(sp)
  ld   s9, OFFSET_REG_S9(sp)
  ld   s10, OFFSET_REG_S10(sp)
  ld   s11, OFFSET_REG_S11(sp)

  // load sepc, sstatuc from kernel stack
  ld   t0, OFFSET_REG_SEPC(sp)
  csrw CSR_SEPC, t0
  ld   t0, OFFSET_REG_SSTATUS(sp)
  csrw CSR_SSTATUS, t0

  // save kernel stack pointer to pcb
  sd   sp, PCB_KERNEL_SP(tp)
  // load user stack pointer from pcb
  ld   sp, PCB_USER_SP(tp)
.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  // prepare stack space for switch_to
  addi sp, sp, -(SWITCH_TO_SIZE)

  /* save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h
   * see arch/riscv/include/asm/regs.h
   */
  sd   ra,  SWITCH_TO_RA(sp)
  sd   s0,  SWITCH_TO_S0(sp)
  sd   s1,  SWITCH_TO_S1(sp)
  sd   s2,  SWITCH_TO_S2(sp)
  sd   s3,  SWITCH_TO_S3(sp)
  sd   s4,  SWITCH_TO_S4(sp)
  sd   s5,  SWITCH_TO_S5(sp)
  sd   s6,  SWITCH_TO_S6(sp)
  sd   s7,  SWITCH_TO_S7(sp)
  sd   s8,  SWITCH_TO_S8(sp)
  sd   s9,  SWITCH_TO_S9(sp)
  sd   s10, SWITCH_TO_S10(sp)
  sd   s11, SWITCH_TO_S11(sp)
  sd   sp,  PCB_KERNEL_SP(a0)

  // switch to next kernel_stack
  ld   sp,  PCB_KERNEL_SP(a1)
  /* restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h
   * see arch/riscv/include/asm/regs.h
   */
  ld   ra,  SWITCH_TO_RA(sp)
  ld   s0,  SWITCH_TO_S0(sp)
  ld   s1,  SWITCH_TO_S1(sp)
  ld   s2,  SWITCH_TO_S2(sp)
  ld   s3,  SWITCH_TO_S3(sp)
  ld   s4,  SWITCH_TO_S4(sp)
  ld   s5,  SWITCH_TO_S5(sp)
  ld   s6,  SWITCH_TO_S6(sp)
  ld   s7,  SWITCH_TO_S7(sp)
  ld   s8,  SWITCH_TO_S8(sp)
  ld   s9,  SWITCH_TO_S9(sp)
  ld   s10, SWITCH_TO_S10(sp)
  ld   s11, SWITCH_TO_S11(sp)

  // recover stack space
  addi sp, sp, (SWITCH_TO_SIZE)

  // set tp = current_running
  add  tp, a1, zero

  jr ra
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT

  sret
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)
  // save context via the provided macro
  SAVE_CONTEXT

  /* call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
  ld   a0, PCB_KERNEL_SP(tp)
  csrr a1, CSR_STVAL
  csrr a2, CSR_SCAUSE
  call interrupt_helper

  /* load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  lui  ra, %hi(ret_from_exception)
  addi ra, ra, %lo(ret_from_exception)
  // return to ret_from_exception
  jr   ra
ENDPROC(exception_handler_entry)
