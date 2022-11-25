# Project 4

## 目录
- [Project 4](#project-4)
  - [目录](#目录)
  - [页目录](#页目录)
  - [页的回收](#页的回收)
  - [用户地址空间](#用户地址空间)
  - [一个 bug 的调试历程](#一个-bug-的调试历程)
    - [已知复现方法](#已知复现方法)
    - [已知信息](#已知信息)
    - [分析](#分析)
    - [修复](#修复)
    - [接下来](#接下来)

## 页目录
实地址空间
```
            ......
0x52004000 ---------------------------- <- FREEMEM_KERNEL
            内核栈（从）
0x52003000 ----------------------------
            内核栈（主）
0x52002000 ----------------------------
            内核运行栈（从）
0x52001000 ----------------------------
            内核运行栈（主）
0x52000000 ----------------------------
            ......
0x51002000 ----------------------------
            二级页表1 (启动页表)
0x51002000 ----------------------------
            二级页表0 (内核页表)
0x51001000 ----------------------------
            ......
0x51000808  一级页表[257] -> 内核页表
            ......
0x51000008  一级页表[1] -> 启动页表
            一级页表[0] -> NULL
0x51000000 ---------------------------- <- PGDIR_PA / SATP
```

## 页的回收
引用计数法

分配页 / 获取共享页时`ref++`

`do_exec()`和可用页框不足时进行`do_garbage_collector()`

尝试回收时`ref--`

## 用户地址空间
```
                ... (more thread stack)
0xf00020000 ---------------------------- <- USER_STACK_ADDR + PAGE_SIZE * 16
                 ↓  thread#0 stack
                ...
0xf00010000 ---------------------------- <- USER_STACK_ADDR
                 ↓  process stack
                ...
                 ↑  share pages
0x800000000 ---------------------------- <- SHM_PAGE_BASE
                ...
                 ↑  .text .bss .data
0x000010000 ---------------------------- <- USER_ENTRYPOINT
                ... (reserved)
0x000000000 ---------------------------- <- NULL
```

## 一个 bug 的调试历程
在本次 A/C-Core 的调试过程中，遇到了一个很头疼的 bug，特此记录

### 已知复现方法
在上板使用多核运行时：
1. 运行`exec consensus`，有一定概率出错，不能稳定复现
2. 先运行`exec fly &`，再运行`exec swap`可以稳定复现

单核及 QEMU 无法触发错误

### 已知信息
1. 问题来自换页相关逻辑，增大页上限后问题概率显著下降
2. 在-O0下，fly 有3个页（2代码/数据+1栈），-O2下减少一个代码/数据段用的页
3. 使用复现方法2时，swap 运行到一定程度时，fly 的3个页被全部换出（符合FIFO策略）
   - 根据 printk 输出，这三次换出的所有参数正确，页表项设置正确，sd 卡分配扇区正确，page / swap 结构体内数据正确
4. 随后，fly 报出一次缺页异常，sepc = 0x0
5. 目前的硬件刷新机制是：`do_scheduler()`末尾，`switch_to()`之前刷新；缺页异常处理完`handle_page_fault()`末尾刷新。两处均刷新 tlb 和 icache

### 分析
假定`swap_in()`/`swap_out()`实现错误，其余部分工作正常，则预期的行为应该是：
1. fly 三个页被换出
2. fly 换入一个页（但其内容存在错误）
3. fly 执行了错误的指令，报异常（指令不存在 / pc 归零导致 sepc = 0x0 的缺页 / 等）

但是实际行为中，fly 从未触发换入，而是 pc 直接归零触发 sepc = 0x0 的缺页

检查 sepc 的来源，是从 CSR 寄存器中直接读出，写入栈中。由于内核 / 用户地址空间隔离，且从写入到打印 sepc 间很短，排除栈被覆盖，认为触发异常时 sepc 确实等于0

假定 sepc 相关的硬件行为正常，怀疑是在 swap 换出页后，fly 进程认为某个（已经换出而无效）页表项仍然有效，做了正常的取指和执行，导致 pc 归零，而0x0地址没有映射，触发缺页异常

而根据 printk 输出可以看到页表项的设置是正确的（最低位，即`_PRESENT`位为0）

怀疑出于某种原因，硬件和软件没有同步，最容易想到的一种是`tlb`和内存上的页表没有同步

再次回顾 QEMU 与实际硬件、单核与双核的差异，我考虑到可能存在这样一种执行序列：

1. swap 缺页，进入内核
2. fly 某种原因（系统调用）进入内核，等待内核锁
3. swap 将 fly 的页换出，处理完刷新 tlb，释放内核锁，返回
4. fly 进入内核，处理完返回
5. fly 没有刷新 tlb，认为页表项仍有效，取指执行（但这些指令实际无效），导致 pc 归零，触发缺页异常

由于我不够了解 RISC-V 的 TLB，上述过程只是推测

后续查看 The RISC-V Instruction Set Manual Volume II: Privileged Architecture 指令集手册，注意到对于`SFENCE.VMA`指令有下述注解：

> Note the instruction has no effect on the translations of other RISC-V threads, which must be notified separately. One approach is to use 1) a local data fence to ensure local writes are visible globally, then 2) an interprocessor interrupt to the other thread, then 3) a local SFENCE.VMA in the interrupt handler of the remote thread, and finally 4) signal back to originating thread that operation is complete.

这从某种意义上证实了我的推测，即两个 CPU 核有各自的 TLB 和 icache，而`sfence.vma`指令只刷新当前 CPU 核的，因此修改页表后有必要通过其它方法（核间中断等）让另一个核心也进行刷新

### 修复
在 commit 0aa32ca35c6dbf9d81e99bbf12cb95886794edac 中

在异常处理结束，返回用户程序前，刷新硬件，即

```c
void interrupt_helper(...) {
    ...
    local_flush_tlb_all();
    local_flush_icache_all();
}
```

### 接下来
这一修复并不是完善的，系统仍然可能在以下情况中出现类似错误：

a)
1. A 缺页异常
2. B 调用`sys_write()`/`sys_mbox_send()`等函数，等待内核锁
3. A 将 B 的页换出，释放内核锁，返回
4. B 进入内核，读取用户页中的缓冲区，缺页

b)
1. A 缺页异常
2. B 在用户态
3. A 将 B 的页换出，返回
4. B 仍在用户态，但此时 pc 所指的页已经被换出，而 tlb 未刷新，从已经无效的页中取指执行，行为未定义

对于前者，可能需要在所有使用用户缓冲区的系统调用前增加检测与换入的逻辑

对于后者，可能需要采用指令集手册建议的核间中断方法

出于时间精力所限，我没有做这些修复，仅作为 known-bugs 留在此处

最后，希望老师们可以考虑将换页作为 C-Core 的任务或者适当降低难度（例如在学案上给出更多有关`flush_tlb()`的指导等）
