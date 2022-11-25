# Project 4

## 目录
- [Project 4](#project-4)
  - [目录](#目录)
  - [启用虚地址](#启用虚地址)
    - [pgdir.h](#pgdirh)
    - [对原先 head.S 的修改](#对原先-heads-的修改)
    - [对原先 main.c 的修改](#对原先-mainc-的修改)
    - [实地址空间示意](#实地址空间示意)
  - [内存管理](#内存管理)
    - [`page_t` 结构](#page_t-结构)
    - [页分配](#页分配)
      - [`allocPage()`](#allocpage)
      - [`alloc_page1()`](#alloc_page1)
      - [`alloc_page_helper()`](#alloc_page_helper)
    - [页回收](#页回收)
      - [`free_page1()`](#free_page1)
      - [`do_garbage_collector()`](#do_garbage_collector)
    - [swap](#swap)
      - [`swap_t` 结构](#swap_t-结构)
      - [`swap_out()` / `swap_in()`](#swap_out--swap_in)
      - [`check_and_swap()`](#check_and_swap)
  - [运行用户程序](#运行用户程序)
    - [创建：对 exec 的修改](#创建对-exec-的修改)
    - [载入：对 loader 的修改](#载入对-loader-的修改)
    - [调度：对 scheduler 的修改](#调度对-scheduler-的修改)
    - [缺页异常的处理：`handle_page_fault()`](#缺页异常的处理handle_page_fault)
    - [用户地址空间示意](#用户地址空间示意)
  - [线程](#线程)
    - [`pthread_create()`](#pthread_create)
    - [`pthread_join()`](#pthread_join)
    - [`pthread_exit()`](#pthread_exit)
  - [共享页](#共享页)
    - [`shm_page_get()`](#shm_page_get)
    - [`shm_page_dt()`](#shm_page_dt)
  - [快照](#快照)
    - [`do_snapshot()`](#do_snapshot)
  - [测试程序](#测试程序)
    - [thread](#thread)
    - [test](#test)
    - [swap](#swap-1)
    - [snapshot](#snapshot)
  - [一个 bug 的调试历程](#一个-bug-的调试历程)
    - [已知复现方法](#已知复现方法)
    - [已知信息](#已知信息)
    - [分析](#分析)
    - [修复](#修复)
    - [接下来](#接下来)

## 启用虚地址
注：如 boot.c 等基本完全由老师完成的代码不再说明
### pgdir.h
基本就是通过左/右移、和掩码做按位与，把内核虚地址/物理地址/虚地址/页属性等字段和页表项 PTE 做相互转换，实现较为简单不再赘述

需要一提的是我新增的`get_pte_of()`，其作用是从 pgdir 中逐级查找 va 的映射，未找到则返回NULL。allow_invalid是独热编码，用于控制允许无效的页表级数，以便查找因[换出](#swap)而置为无效的页表项。例如`allow_invalid=4`即表示允许查找到无效的3级页表项（此时需要由调用者判断其各个字段是否有效）

### 对原先 head.S 的修改
一个差异是原先主核使用的运行栈（sp 初值）简单地设为了`0x50500000`，而在本实验中，一方面需要使用内核虚地址，另一方面地址空间的划分已经产生变化，不再能使用`0x50500000`的物理地址。考虑后，我将部分实地址空间划分为[下表](#实地址空间示意)所示，并选择`[0x52000000, 0x52001000)`作为内核进程的运行栈空间

需要注意的是 sp 需要载入为`0xffffffc052001000`，而此值超出了`la`伪指令（`auipc + addi`的组合）允许的范围，因此我选择将其存到 c 语言代码的全局变量中（即`.data`段中），并在此处使用`la pid0_stack`载入标号地址，配合`ld`载入值

### 对原先 main.c 的修改
在进入 main.c 后，需要取消`[0x50000000, 0x50400000)`这一部分用于启动（从 boot.c 开启虚存，进入内核前，仍使用实地址的代码）的映射，实现为`unmap_boot_addr()`函数，但在多核的情况下，我对此处的实现仍有疑虑：

1. 若由主核完成 unmap，则可能从核尚未进入内核，仍需使用虚地址，出错
2. 若由从核完成 unmap，则可能主核已经到达第一次时钟中断，进入用户程序，若用户程序访问了这一段虚地址，产生冲突出错

因此可能需要实现类似 barrier 的机制，保证两个核都进入内核，且未进入用户程序，此时进行 unmap

但处于时间和精力所限，我没有完成这个机制，而是直接使用从核进行 unmap，这是考虑到上述两种情况中后者出错的概率更小

对于现在的代码，若要使用单核，需要手动将 init/main.c 中第236~239行的代码移动到主核的分支上，并重新编译，才能保证取消映射正确

### 实地址空间示意
与内核虚地址空间以`0xffffffc0`为前缀的地址做一一映射
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

## 内存管理
### `page_t` 结构
使用 `page_t` 结构来描述一个页，其各域：
- `ptr_t kva`：内核虚地址，当页在硬盘上时为0，当页在内存上时有效，且对应唯一的一个物理地址，此时可用于描述一个物理页框
- `ptr_t va`：用户虚地址，如果页由内核使用则为0
- `list_node_t list`：链表结点，连接在持有者的页链表中，用于[回收](#do_garbage_collector)时遍历并[释放](#free_page1)
- `swap_t *swap`：[指向一个磁盘页](#swap_t-结构)，当页在内存上时为NULL，当页在磁盘上时有效
- `list_node_t onmem`：链表结点，连接在内存页链表（FIFO）中，用于[换页](#swap)
- `pcb_t *owner`：指向持有者
- `enum { PAGE_USER, PAGE_KERNEL, PAGE_SHM } tp;`：页类型：用户页、内核页、共享用户页

从某种角度上，可以认为一个 `page_t` 结构可以动态地绑定到一个物理页框或者一个磁盘区域上，同时绑定到一个进程的pcb上
```
---------       ----------        ----------
|       |  kva  |        |        |        |
| page  | <---> | page_t | *swap  | swap_t |
| frame |       |        | <----> |        |
|       |       |        |        |        |
---------       ----------        ----------
                    ^
                    | *owner, list, va
                    v
                ----------
                |        |
                | pcb_t  |
                |        |
                ----------
```
- 当它未被分配时：位于`freepage_list` 中，仅 kva 有效，此时表示一个物理页框
- 当它已被分配时：
  - 位于内存中，kva，owner 等域有效，既表示一个虚页，也表示它对应的物理页框
  - 位于硬盘中，swap，owner 等域有效，表示一个虚页

### 页分配
分配内存总共经过3层封装，从底层向上：
#### `allocPage()`
这一层可以一次分配多个连续的页，仅会将空闲空间指针`kernMemCurr`上移`num`个页大小，并返回原值

通过这层分配的空间是不可回收的，在内核实现中仅用在了`kmalloc()`、`alloc_page1()`和 loader 的缓冲区分配3处

#### `alloc_page1()`
1. 检查空闲页列表`freepage_list`是否有页，若有则取出一页。否则创建一页（利用 kmalloc 创建一个 page_t 结构）并初始化
2. 初始化各域
3. 调用`memset`清空页的原始内容

一次仅能分配一个页，且不保证连续两次调用分配的页地址连续

通过 [`free_page1()`](#free_page1) 回收

#### `alloc_page_helper()`
用于分配用户态的页、分配页表并建立页表项，同时负责将页连接到进程持有的页列表中

其中“分配页表并建立页表项”是通过独立封装的`map_page()`函数实现，这么做是为了 [snapshot](#快照) 等功能可以复用代码

所有页都是通过 [`alloc_page1()`](#alloc_page1) 分配

### 页回收
#### `free_page1()`
- 若页在内存中，将其加入空闲页列表`freepage_list`
  - 若页是用户页，`remaining_pf+1`（见 [swap](#swap) 一节）
- 若页在硬盘上，将其对应的`swap_t`结构体释放（调用`free_swap1()`函数）

#### `do_garbage_collector()`
遍历所有 pcb，找到状态处于`TASK_EXITED`的，遍历它们的持有页列表`page_list`并调用`free_page1()`进行回收

为了避免过早的回收内核页导致出错（e.g.在`do_exit()`过程中，运行栈仍在内核页上），在`do_exec()`的开头调用该函数，而不是`do_kill()`的末尾

### swap
通过`remaining_pf`计数器模拟内存限制，仅对用户态的页进行计数和换入/换出，避免内核态的页被换出导致出现

由于所有用户态的页通过`alloc_page_helper()`分配，因此在该函数中，当新分配了页时将`remaining_pf-1`

#### `swap_t` 结构
使用 `page_t` 结构来描述一个硬盘区域，其各域：
- `unsigned int pa`：所在的硬盘分区号（连续的`8=PAGE_SIZE/SECTOR_SIZE`个分区中第一个的分区号）
- `list_node_t list`：链表结点，连接在空闲硬盘链表`freeswap_list`中（或指向它自身）

使用类似[页](#内存管理)的方式通过`allocDBlock()`、`alloc_swap1()`、`free_swap1()`进行底层分配、分配和回收

`allocDBlock()`分配的起始地址为 `taskinfo` 数组末尾向后对齐一个扇区处

#### `swap_out()` / `swap_in()`
两者基本是逆过程，out：
1. 分配一个 `swap_t` 结构
2. 将页从内存页列表`onmem_list`中删除
3. 设置页表项为无效
4. 写入磁盘
5. 解除与页框的绑定
6. 返回释放的页框（对应的内核虚地址`kva`）

in：
1. 绑定页框
2. 读取磁盘
3. 设置页表项为有效
4. 将页加入内存页列表`onmem_list`
5. 回收 `swap_t` 结构

#### `check_and_swap()`
遍历一个进程持有的所有页，找到位于磁盘上、va 所在的用户态的页

根据 FIFO 策略换出一个页（释放一个页框），并将刚刚找到的页换入

## 运行用户程序
### 创建：对 exec 的修改
1. [检查并回收垃圾](#do_garbage_collector)
2. 遍历 pcb 数组找到一个未使用（`TASK_UNUSED`或`TASK_EXITED`）的项
3. 分配页目录，从内核复制页表项（以便系统调用/异常时无需切换页表）
4. 每次`alloc_page_helper()`分配一个用户态页，并`load_img()`读取一个页的内容，直至用户程序载入完成
5. `alloc_page1()`分配一个内核栈的页，设置内核栈指针为分配的 kva
6. `alloc_page_helper()`分配一个用户栈的页，记录分配的 kva 用于复制参数，设置用户栈指针为固定值`USER_STACK_ADDR`
7. 其余类似 Project 3

2~5步分配出的所有页均被连接在 pcb 的 `page_list` 中，以便回收

### 载入：对 loader 的修改
首先，各用户程序间是隔离的，不能像[以前](../Project1_BootLoader/README.md#取消应用程序间的-padding)一样直接将用户程序载入到`memaddr+offset`处再向前移动（可能刚好由于offset的存在超出了分配给用户程序的页，造成其它进程的页被覆盖而出错）

因此使用`allocPage()`直接分配一大块内存空间作为 buffer，`bios_sdread()`到 buffer 中，再复制需要的部分到分配给用户程序的页中

其次，根据学案提示，一次`bios_sdread()`不应超过64个扇区，因此使用一个循环，若剩余大于64个扇区，则仅读入和复制64个扇区，将目标地址`memaddr`、读取位置`block_id`后移，剩余大小`num_of_blocks`、`size`减小，重置`offset`

最后，由于该函数也被用于读取 taskinfo 数组，这一读取行为后由调用者将数据复制到正确的地方，因此在函数内无需进行复制，只需返回 buffer 的地址即可。将这种用法封装为`load_img_tmp()`函数以便使用

### 调度：对 scheduler 的修改
在`switch_to`之前，切换页目录并刷新硬件

### 缺页异常的处理：`handle_page_fault()`
将12、13、15三个异常号，即取指页错误、读取页错误、写入页错误3种异常连接到该函数上
1. 检查va对应的页表项
2. 若不存在：
   1. [检查并换入在硬盘上的页](#check_and_swap)
   2. 不在硬盘上，[分配新页](#alloc_page_helper)
3. 若存在，是否是只读页产生的写入页错误，若是（一定是[快照](#快照)）：
   1. [分配新页](#alloc_page_helper)，将旧页的内容复制到新页
4. 根据异常类型置页表项的 A / D 位

### 用户地址空间示意
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

## 线程
复用 pcb 结构，同进程一样调度
### `pthread_create()`
类似进程的`do_exec()`，差异主要在于
1. 分配新的 tid，继承 pid
2. 不使用自身的 `page_list`，所有页连接在父进程的 `page_list` 中
3. 使用父进程的页目录和页表
4. 不载入用户程序
5. 只通过 a0 寄存器传递一个参数`void *arg`
6. 入口地址为用户程序传入的函数地址

### `pthread_join()`
类似进程的`do_waitpid()`

### `pthread_exit()`
类似进程的`do_exit()`

需要用户程序（的线程）主动在 return 前调用

## 共享页
类似锁、条件变量等，将共享页作为系统资源分配

一个共享页仅允许同时被`SHM_PAGE_MAX_REF`个进程获取，系统中最大只能存在`SHM_PAGE_NUM`个共享页

采用类似引用计数法的思路，当 ref 归零时回收共享页

### `shm_page_get()`
1. 从`SHM_PAGE_BASE`开始搜索用户虚地址空间，最大只能搜索到`SHM_PAGE_LIM`，找到用户未使用（页表项不存在）的虚地址，作为用户访问共享页的虚地址。如果没有可用的用户虚地址，则失败
2. 类似锁、条件变量等，分配一个共享页，若没有可用的，或 key 匹配但 ref 超出`SHM_PAGE_MAX_REF`则失败
3. `ref ++`
4. 若共享页未被绑定在某一物理页框上，调用`alloc_page1()`分配（该函数保证分配的页内容为全0）
5. 将调用者的 pid 和 va 记录在 map 域中
6. 设置页表项

### `shm_page_dt()`
1. 遍历所有共享页，找到 pid 和 addr 同时匹配的项
2. `ref --`
3. 将调用者的信息从 map 中移除
4. 无效页表项
5. 若 ref 归零，调用`free_page1()`回收绑定的页

## 快照
写时复制已经在[缺页异常的处理](#缺页异常的处理handle_page_fault)中完成，只需完成创建快照即可
### `do_snapshot()`
1. 将传入的`va`向下对齐到页便于后续处理
2. 设置`va`对应的页表项为不可写
3. 类似共享页，从`va`开始搜索用户虚地址空间，最大搜索到`va+SNAPSHOT_LIM`，找到用户未使用（页表项不存在）的虚地址，作为用户访问快照的虚地址`new_va`
4. 调用`map_page()`创建`new_va`的页目录
5. 设置页表项，其指向`va`对应的页，但属性为可写
6. 返回`new_va`

## 测试程序
### thread
除了老师使用的 mailbox 外，还使用了 Project 2 使用的 thread 测试程序，用以测试`pthread_join()`和`pthread_exit()`的工作情况

输出类似：
```
> [TASK] main thread: waiting for thread#0
>        thread#0   : add (328/500), partsum=53956
>        thread#1   : add (310/500), partsum=203205
>        thread#2   : add (298/500), partsum=342551
>        thread#3   : add (291/500), partsum=478986

> root@UCAS_OS: exec thread
```

### test
测试内核对`0x0`地址的保护（不允许访问`NULL`）

输出类似：
```
[0][00000000000565243650][mm        ][E] alloc page for addr 0x0 is prohibited
kernel panic: alloc page failed

> root@UCAS_OS: exec test
```

### swap
测试换出/入页，连续写入`ACCESS_NUM`个页，随后读取这些页，校验数据和写入一致

输出类似：
```
write................................
read ................................Success!

> root@UCAS_OS: exec swap
```
每一个`.`表示一次写入/读取

### snapshot
测试快照，创建10次快照，每次有一半的概率写入一个新值

输出类似：
```
snapshot[0] @ 0x10001000: 0x114514 @ 0x52020000
snapshot[1] @ 0x10002000: 0x114515 @ 0x52021000
snapshot[2] @ 0x10003000: 0x114515 @ 0x52021000
snapshot[3] @ 0x10004000: 0x114515 @ 0x52021000
snapshot[4] @ 0x10005000: 0x114518 @ 0x52022000
snapshot[5] @ 0x10006000: 0x114518 @ 0x52022000
snapshot[6] @ 0x10007000: 0x114518 @ 0x52022000
snapshot[7] @ 0x10008000: 0x114518 @ 0x52022000
snapshot[8] @ 0x10009000: 0x114518 @ 0x52022000
snapshot[9] @ 0x1000a000: 0x114518 @ 0x52022000
base        @ 0x10000000: 0x11451e @ 0x52023000

> root@UCAS_OS: exec snapshot
```
输出格式为`快照次数 @ 用户虚地址: 值 @ 物理地址`

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

出于时间精力所限，我没有做这些修复，而是作为已知 bug 留在此处

最后，我个人认为换页需要考虑的情况较多，难度比后面的线程、共享页、快照都要难上不少。老师们也许可以考虑将它作为 C-Core 的任务或者适当降低难度（例如在学案上给出更多有关`flush_tlb()`的指导等）
