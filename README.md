# UCAS-OS-2022-Lab
2022 UCAS 操作系统（研讨）课代码

## 涉及内容
- 启动引导：prj4 以前单阶段引导（bootloader -> 实地址 kernel）；以后二阶段引导（bootloader -> 实地址 boot -> 虚地址 kernel）
- 进程调度：RR 算法
- SMP：从核启动，内核全局锁
- 进程间通信：mutex，cond，mailbox，barrier
- 虚拟内存：RISC-V Sv39，swap
- 设备驱动：e1000 网卡，PLIC 控制器
- 文件系统：自定义
- 简易 shell：支持方向键选择历史（上下）和移动光标（左右）

## BB
有些关于折腾开发环境的经验见 [CS-Guide](https://github.com/ngc7331/UCAS-CS-Guide/blob/main/doc/course_example/OS-Lab.md) 项目

swap 和文件系统写的特别丑陋，建议别看

还是看看远处的[操作系统内核赛](https://os.educg.net/#/index?name=2023%E5%85%A8%E5%9B%BD%E5%A4%A7%E5%AD%A6%E7%94%9F%E8%AE%A1%E7%AE%97%E6%9C%BA%E7%B3%BB%E7%BB%9F%E8%83%BD%E5%8A%9B%E5%A4%A7%E8%B5%9B%E6%93%8D%E4%BD%9C%E7%B3%BB%E7%BB%9F%E8%AE%BE%E8%AE%A1%E8%B5%9B-%E5%86%85%E6%A0%B8%E5%AE%9E%E7%8E%B0%E8%B5%9B&index=1&img=0)吧家人们

## **请 勿 抄 袭**
