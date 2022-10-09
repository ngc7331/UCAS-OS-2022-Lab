# Project 2
## Part 1
### 代码说明
注：实验前由老师给出、且未作修改的代码不再说明

#### libs/list.c
带有头结点的双向循环链表 API
- 要求头结点`l != NULL`
- 当`l == l->next && l == l->prev`时链表为空
- 具体函数说明：
  * `list_init()`：初始化空链表，即将头结点的`next`和`prev`域设为`l`自身
  * `list_insert()`：在结点`l`后插入结点`n`
  * `list_delete()`：从链表中删除结点`l`
  * `list_is_empty()`：判断链表是否为空

注：由于目前使用该 API 仅是实现 PCB 的 ready 和 block 队列，因此目前的设计非常简陋，对于一些特殊情况，例如：
- `l == NULL`：尝试访问 NULL 的`next`和`prev`域
- 要插入的结点`n`已经在`l`所在链表内：链表指针错乱，如下图所示
  ![](docs/img/p2-1.drawio.png)
  (红色为prev，蓝色为next指针)

等情况会出现问题，编写时需要留意

#### include/os/list.h
libs/list.c 的头文件，需要特殊说明两个宏如下：
- `offset_of(tp, mem)`：计算域`mem`在结构体类型`tp`中的以字节为单位的偏移量
- `list_entry(ptr, tp, mem)`：根据成员`mem`的指针`ptr`和结构体类型`tp`计算结构体的指针


#### init/main.c
- `pcb_enqueue()` & `pcb_dequeue()`：对 list API 的一层封装，简单的队列API
- `init_pcb_stack()`：初始化分配给某一 pcb 的栈空间
  * 将初始的返回地址（即用户程序入口地址`entrypoint`）和用户栈指针写入内核栈，以便`switch_to()`使用
- `init_pcb()`：载入所需的用户程序
  1. 宏定义了所需的用户程序名数组`NEEDED_TASKS`，遍历该数组，对每一个所需的任务：
  2. 复用 Project 1 的`get_taskid_by_name()`函数获得其任务 id
  3. 复用 Project 1 的`load_task_img()`函数从镜像中载入任务到内存
  4. 分配内核栈和用户栈
  5. 分配 pid，这里使用了一个全局变量`pid_n`，值为上一个分配出去的 pid
  6. 复制进程名（自行添加的）
  7. 调用`init_pcb_stack()`初始化栈空间
  8. 设置为就绪态，进入`ready_queue`

#### arch/riscv/kernel/entry.S
- `switch_to()`：
  1. 暂存前一pcb的用户栈指针sp
  2. 切换到前一pcb的内核栈
  3. 保存sp、ra、s0~s11
  4. 切换到后一pcb的内核栈
  5. 载入sp、ra、s0~s11
  6. 切换到后一pcb的用户栈
  7. 修改tp为新的current_running

#### kernel/sched/sched.c
调度相关
- `do_scheduler()`：
  ![](docs/img/p2-2.drawio.png)
- `do_block()`：将 pcb 加入某一阻塞队列，设为阻塞态，调用调度器
- `do_unblock()`：从某一阻塞队列取出一 pcb，设为就绪态，加入`ready_queue`

#### kernel/locking/lock.c
锁相关
- `do_mutex_lock_init()`：为`key`分配一个锁，返回下标`idx`
  * 当已经有分配给`key`的锁时，直接返回它的下标
  * 否则，将第一个未分配的锁分配给`key`，返回它的下标
  * 若所有锁已分配（分配失败），返回-1
- `do_mutex_lock_acquire()`：尝试获取锁
  * 若获取到，返回
  * 否则，调用`do_block()`将进程阻塞
- `do_mutex_lock_release()`：释放锁
  * 若阻塞队列空，锁的状态改为`UNLOCKED`
  * 否则，调用`do_unblock()`解除一个进程的阻塞
