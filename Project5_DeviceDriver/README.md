# Project 5

## 目录
- [Project 5](#project-5)
  - [目录](#目录)
  - [发送](#发送)
    - [初始化](#初始化)
    - [工作](#工作)
      - [net.c: `do_net_send()`](#netc-do_net_send)
      - [e1000.c: `e1000_transmit()`](#e1000c-e1000_transmit)
      - [net.c: `check_net_send()`](#netc-check_net_send)
  - [接收](#接收)
    - [初始化](#初始化-1)
    - [工作](#工作-1)
      - [net.c: `do_net_recv()`](#netc-do_net_recv)
      - [e1000.c: `e1000_poll()`](#e1000c-e1000_poll)
      - [net.c: `check_net_recv()`](#netc-check_net_recv)
  - [网卡中断](#网卡中断)
    - [使能](#使能)
    - [处理](#处理)
  - [echo](#echo)
    - [主进程（入口）](#主进程入口)
    - [recv](#recv)
    - [send](#send)

## 发送
### 初始化
1. 发送描述符数组`tx_desc_array`
   1. 将 CMD 域的 RS（回报状态）、EOP（包结束）位置1，其余清零
   2. 在发送时填充 address 域和 length 域，不改变其余位
   3. 注：本实验中保证一次发送的长度在包长度限制内，因此可以如上述将 EOP 置一
2. 寄存器
   1. 描述符基地址寄存器`TDBAL`和`TDBAH`分别填充`tx_desc_array`的**物理地址**的低32位和高32位
   2. 描述符长度寄存器`E1000_TDLEN`填充`tx_desc_array`的**以字节为单位**大小
   3. 队列头尾指针寄存器`TDH`和`TDT`置0，表明初始时所有描述符由**软件**持有
   4. 控制寄存器`TCTL`的 EN（使能）、PSP（填充短包）位置1，CT（碰撞阈值）位置0x10，COLD（碰撞距离，0x40=全双工）位置0x40

### 工作
#### net.c: `do_net_send()`
1. 尝试发送`e1000_transmit()`
2. 若失败，启用 TXQE 中断，阻塞
3. 成功，检查阻塞队列和发送队列情况，唤醒（一个）其他进程
4. 返回长度

#### e1000.c: `e1000_transmit()`
1. 检查发送队列情况（`TDH`和`TDT`，若`(TDT+1) % LXDESCS == TDH`则满）
2. 若满，返回-1表示失败（S-Core 中使用`while`循环等待发送队列非满，而不是直接返回）
3. 将消息从用户地址空间复制到内核地址空间的缓冲区，以便确定硬件使用的物理地址
4. 填充发送描述符的 address 域和 length 域
5. `TDT+1`
6. 刷新 dcache
7. 返回长度

#### net.c: `check_net_send()`
检查并唤醒
1. 检查发送队列情况（`TDH`和`TDT`）
2. 检查阻塞队列情况
3. 若发送队列非满、阻塞队列非空，则从阻塞队列中唤醒**一个**进程
4. 在时钟中断（A-Core）或[网卡中断](#网卡中断)（S-Core）时调用该函数
5. 被唤醒的进程完成发送后重复调用该函数，实现链式唤醒。从而避免 a)一次性唤醒所有进程，而大部分要重新阻塞浪费 CPU 时间；或 b)有进程一直被阻塞的两种情况。

## 接收
与发送的相似点很多，下面仅说明几个差异点
### 初始化
1. 接收描述符数组`rx_desc_array`
   1. 将 address 域填充为缓冲区数组`rx_pkt_buffer[i]`的物理地址
   2. 其余域置0，由硬件填充
2. 寄存器
   1. 队列头尾指针寄存器`RDH`置0，`RDT`置`RXDESCS-1`，表明初始时所有描述符由**硬件**持有
   2. 控制寄存器`RCTL`的 EN（使能）、BAM（接收广播）位置1；BSEX、BSIZE置0，表明使用2048字节大小的缓冲区；RXDMT置0，表明当接收描述符数组填充至一半时发起中断

### 工作
#### net.c: `do_net_recv()`
1. 循环`pkt_num`次，尝试接收`e1000_poll()`
2. 若失败，直接阻塞（[RXDMT0 中断在初始化时使能](#使能)）
3. 成功，将本次接收的长度写入`pkt_lens[i]`
4. 循环结束，检查阻塞队列和接收队列情况，唤醒（一个）其他进程
5. 返回总长度

#### e1000.c: `e1000_poll()`
1. 检查接收队列情况（`RDH`和`RDT`，若`(RDT+1) % RXDESCS == RDH`则空）
2. 若空，返回-1表示失败（S-Core 中使用`while`循环等待发送队列非满，而不是直接返回）
3. 刷新 dcache
4. 检查接收描述符 STA 域的 DD 位，确保该位为1（接收成功完成）
5. 将消息从内核地址空间的缓冲区复制回用户地址空间
6. `RDT+1`
7. 返回长度

#### net.c: `check_net_recv()`
检查并唤醒，类似[`check_net_send()`](#netc-check_net_send)

## 网卡中断
### 使能
发送的中断使能在一次发送失败（发送队列满）时开启：IMS.TXQE = 1

并且在处理发送中断时关闭：IMC.TXQE = 1

接收的中断使能在初始化时开启：IMS.RXDMT0 = 1，永远不会关闭

### 处理
调用流程如下：
```
interrupt_helper() -> handle_irq_ext() -> net_handle_irq() -> check_net_recv() / check_net_send()
                            ↓       ↗id         ↓      ↗icr, ims
                        plic_claim()      e1000_read_reg()
                            ↓
                      plic_complete()
```
具体来说：
1. 产生中断，进入内核`interrupt_helper()`，与之前的时钟中断、系统调用等相同
2. 查`irq_table[]`，`code=IRQC_S_EXT`进入`handle_irq_ext()`
   1. 调用`plic_claim()`，检查外部中断具体来自于哪个外设，若为`PLIC_E1000_PYNQ_IRQ`（即来自 E1000 网卡），则进入`net_handle_irq()`
      1. 调用`e1000_read_reg()`读取 ICR 和 IMS 寄存器，确定中断类型（本实验中只可能是 TXQE 或 RXDMT0）
      2. 若为 TXQE，关闭中断使能，调用`check_net_send()`检查发送阻塞队列并唤醒
      3. 若为 RXDMT0，调用`check_net_recv()`检查接收阻塞队列并唤醒
   2. 网卡中断处理结束，调用`plic_complete()`提交
3. 从中断返回，与之前相同

## echo
要求是使用两个进程，使用共享页传输数据（包及其长度）。基本想法是类似生产者-消费者问题，使用锁和条件变量保证临界区互斥访问

### 主进程（入口）
不提供任何参数，即`exec echo`进入入口

入口负责执行`exec echo 0`和`exec echo 1`从而启动 recv 和 send 两个进程

退出

### recv
1. 初始化一个共享页、一把锁、两个条件变量，随后循环2~6
2. 获取锁
3. 若缓冲区有数据，等待其为空
4. 调用`sys_net_recv`接收一个包，数据写入`shm->buf`，长度写入`shm->length`
5. 唤醒 sender
6. 释放锁

### send
1. 同样初始化一个共享页、一把锁、两个条件变量，随后循环2~6
2. 获取锁
3. 若缓冲区无数据，等待其不为空
4. 预处理缓冲区中的包：前6字节（接收者MAC地址）改为`0xffff ffff ffff`，表示广播包；将数据段中的"Requests:"修改为"Response:"，表示响应
5. 调用`sys_net_send`发送缓冲区中的包
6. 置零`shm->length`表示缓冲区无数据
7. 唤醒 receiver
8. 释放锁
