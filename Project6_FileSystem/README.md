# Project 6

## 目录
- [Project 6](#project-6)
  - [目录](#目录)
  - [文件系统概况](#文件系统概况)
    - [磁盘布局](#磁盘布局)
    - [初始化](#初始化)
    - [创建](#创建)
  - [路径解析器](#路径解析器)
  - [缓存](#缓存)
  - [ls](#ls)
  - [cd](#cd)
  - [inode 与 block 的分配和回收](#inode-与-block-的分配和回收)
  - [创建文件或文件夹](#创建文件或文件夹)
  - [删除文件或文件夹](#删除文件或文件夹)
  - [文件操作](#文件操作)
    - [打开](#打开)
    - [关闭](#关闭)
    - [读写、cat](#读写cat)
    - [lseek](#lseek)
  - [ln](#ln)
  - [间址块的支持](#间址块的支持)
    - [`find_indirect_block`](#find_indirect_block)
    - [`find_block`](#find_block)


## 文件系统概况
### 磁盘布局
`()`中为大小，单位为块（1块 = 4KB = 8扇区）
```
| kernel | swap space | superblock | Inode  | Block  | Inode | Blocks    | unused |
| image  |            | (1)        | bitmap | bitmap | table | (1048576) |        |
|        |            |            | (1)    | (32)   | (512) |           |        |
^        ^            ^
0        KERNEL_END   FS_START
```
其中我确定了文件系统的总大小为4GB，故 Block 数量为`4GB/4KB=1048576`，Block bitmap 的大小为`1048576/(8*4K)=32`
确定了 Inode bitmap 的大小为1个 block，故 Inode table 的大小为`8*sizeof(inode_t)=512`

### 初始化
系统启动时，首先从`FS_START`读出一个块作为超级块，校验其头尾的 magic 位置是否为 MAGIC，若都是，则认为文件系统有效；否则，创建文件系统

### 创建
主要分为两块：创建超级块、创建根目录

创建超级块只需依照上面的布局，将各区域的 offset、size 等信息，以及 magic、使用情况等填入内存上的超级块，随后写入磁盘即可

创建根目录的逻辑和创建普通目录是类似的，只是其`..`目录项指向的是自身，通过调用私有函数`_mkdentry()`时，将其 inode 的编号同时作为`ino`和`pino`即可实现这一点。关于这个函数的介绍见下文

## 路径解析器
将路径字符串解析成 inode 编号，顺便记录父目录的 inode 编号和文件名的指针，例如路径：
```
/home/stu/OS-Lab
```
经过解析后，pino 记录 stu 的 inode 编号，name 指向 O，返回值为 OS-Lab 的 inode 编号，未找到返回-1

解析过程大致如下：
1. 判断第一个字符是否为`/`，若是，从 ino=0（初始化时保证这一 inode 一定是根目录）开始解析；否，从 ino=current_ino 开始解析
2. 扫描 path 直到遇到下一个`/`或`\0`
3. 将两个`/`或开头结尾之间的字符串作为本级的 name，载入 ino 指向的 inode，遍历其所有 block_no；载入 block_no 指向的块，遍历其所有 dentry，比较 dentry.name 与本级的 name，若匹配则停止遍历
4. 将遍历得到的 dentry.ino 作为新的 ino，从2重复

这样后续的处理就比较显然：
- 对于 mkdir、touch 等创建新 inode 的调用，应要求 pino 不为-1，而 ino 为-1。否则分别出现父目录无效、文件（夹）已存在错误；新创建的目录项中直接使用 name 指针
- 对于 ls、rmdir、cat 等调用，则要求 ino 不为-1（此时 pino 一定不为-1）

当不需要记录 pino 和 name 时，只需输入参数 NULL

注意：为了简化解析结束条件的判断，本函数要求输入的 path 一定不以`/`结尾，由调用者保证这一点是合理的：对于文件夹，末尾有没有`/`意义一样；对于文件，文件名不应以`/`结尾

## 缓存
由于我们不能直接以字节为粒度访问磁盘（考虑到运行效率，以字节为粒度访问磁盘也不太合理），因此所有磁盘写操作都需要先将原始 block 内容读进内存，修改其中的值，再写回

考虑到经常需要同时打开 inode 及其数据块，至少需要使用两个4KB大小的缓冲区。在我的设计中0号给数据块、bitmap 用，1号给 inode 用

由于是固定地址，可以将 bios_sdread / sdwrite 进行封装以简化使用，见`write_buf()`和`read_buf()`，输入的参数只需要有缓冲区的序号和块在文件系统中的偏移。进一步的，可以封装为`get_inode()`、`write_inode()`、`get_block()`和`write_block()`。需要注意的是，如果是做写入，这两对函数必须成对出现，即不能有
```
get_inode(ino1)->link++;
get_inode(ino2);
write_inode(ino1);
```
这种情况下，第二个`get`会把前一个覆盖掉，造成错误

未来也许可以使用`alloc_page1()`和`free_page1()`来为每个需要读写的 inode 和 block 分配/回收独立的缓冲区，从而摆脱这一限制。但出于精力原因暂时没有完成

## ls
调用 path_lookup 得到目录的 inode，读取它，遍历所有有效的块，对每个块遍历其所有目录项，输出

参数`-a`、`-l`由 shell 进行解析，作为 option 参数输入。在没有`-a`时，隐藏所有以`.`开头的文件，在没有`-l`时，只显示文件名，而不显示类型、大小、link数量等信息

使用时只支持`ls [-al] [path]`的写法，path 为空时默认为`.`。`a`和`l`可以交换顺序，但不能用空格分开如`-a -l`

## cd
调用 path_lookup 得到目录的 inode，将“当前所在目录”指针`current_ino`设为它的编号

## inode 与 block 的分配和回收
分配：实际上就是遍历 bitmap、找到为0的一位，将它置1后写回磁盘，并返回这位的下标即可

回收：根据下标算出其在 bitmap 中的位置，将它置0后写回磁盘

两者是相似的，统一封装成`_alloc_bitmap()`进行使用，为了读起来方便，定义宏：
```
#define alloc_inode() _alloc_bitmap(0)
#define alloc_block() _alloc_bitmap(1)
#define free_inode(idx) _free_bitmap(0, idx);
#define free_block(idx) _free_bitmap(1, idx);
```

需要注意的是，为了支持`statfs`中输出文件系统使用情况，而不必每次都遍历 inode map 和 block map，我在 superblock 中记录了它们的使用数量。需要在分配和回收时同步加减

## 创建文件或文件夹
touch 和 mkdir 的作用是十分相似的，只是创建的 inode 类型不同、目录需要创建默认目录项、初始大小不同（文件为0，而目录至少为一个block）、初始链接数不同（文件只有父目录指向它，而目录还有自己指向自己）

因此将它们合并：
- `do_mkdentry`：找到父目录，并在其目录项列表中新建目录；若目录项占满了一个块，则分配新块；否则遍历块，找到无效的一项，将 name 和 ino 填入
- `_mkdentry()`：初始化新 inode 的各个域，创建默认目录项

这样`do_mkdir`和`do_touch`就可以简化为：
```
int do_mkdir(char *path) {
    return do_mkdentry(path, INODE_DIR);
}
int do_touch(char *path) {
    if (path[strlen(path)-1] == '/') {
        logging(LOG_ERROR, "fs", "touch: file name cannot end with \"/\"");
        return -1;
    }
    return do_mkdentry(path, INODE_FILE);
}
```
需要注意的是 touch 不允许以`/`结尾，需要额外的检查

## 删除文件或文件夹
删除目录项是相似且简单的：找到目录项，置为无效，与前面路径解析、ls、创建并无太大不同，不再赘述

但对于删除条件（文件永远可以删除，而目录只有非空才能删除）、和 inode 与 block 的回收（目录一旦被删除就可以回收，文件可以被 ln，只有 link 为0时才能回收）不尽相同

因此并没有合并成一个函数

## 文件操作
### 打开
打开文件和打开信箱/初始化锁等系统资源是类似的：遍历文件描述符数组，找到一个未使用的，初始化其 ino 指针，读写指针等信息后返回下标作为 fd

打开文件的索引是 path 字符串，在系统内通过`path_lookup()`转化为文件的 ino

在分配前还需检查文件描述符数组内是否已有 ino 的项，若是则返回失败（保证对同一个文件的互斥访问）

此后，所有针对文件描述符的操作（读、写、关闭、lseek）都需要检查 fd 是否有效、是否被当前进程持有、是否已打开，封装为`check_fd()`

### 关闭
关闭文件和释放锁等系统资源是类似的：将对应文件描述符的 ino 域置为 -1 以表示无效即可

### 读写、cat
打开当前读写指针所在的块（读写指针记录以字节为单位的相对文件头的offset，可以调用`find_block()`即可找到对应块，该函数见[下文](#间址块的支持)）

并向其中写入/从其中读取数据，当读写到一个块末尾时，打开下一个块

写入时，若要被写入的块不存在，则分配它，通过重新调用`find_block()`时传入新块的编号，即可自动建立不存在的直接地址快/一至三级间接地址块，并将新块的编号写入其中

cat 的实现和读取十分相似，只是：其读取长度由文件大小确定，而非输入参数；读取得到的数据不需要复制到缓冲区，而是直接打印到屏幕

### lseek
根据操作类型将文件描述符的读写指针设为新值（头+offset，当前+offset，尾+offset）

## ln
分别找到源文件和目的文件的父目录的 ino，类似 touch 地在目的文件的父目录中创建新的目录项，ino 设为源文件的 ino（无需分配新的 ino，但若目录项不够，可能需要分配新的 block）

## 间址块的支持
### `find_indirect_block`
递归函数，假设`parent`是第`level+1`级间址块，从它中寻找第`block_no`个块。当`new_block`不为`-1`时，建立不存在的索引项。

- 若`level == 0`
  - `parent`的第`block_no`项即为这个块的编号，返回
  - 若`new_block`不为`-1`时，将它的值填入该项中
- 否则
  - 找到第`level`级间址块，也即`parent`的第`block_no / addr_num_per_block ^ level`项
  - 若`new_block`不为`-1`时，创建不存在的第`level`级间址块，并将其编号填入该项中
  - 递归调用，从`level`级间址块中查找第`block_no % addr_num_per_block ^ level`项

### `find_block`
输入参数为“本文件的第几个 block”，即`offset/BLOCK_SIZE_BYTE`，依次在直接索引块、一至三级间接索引块中查找。间接索引块的查找是通过调用`find_indirect_block`实现的。当直接索引块、一至三级间接索引块不存在时，且`new_block`不为`-1`时创建不存在的块，并将编号填入 inode

有了这两个函数，只需在使用时调用`bno = find_block(offset/BLOCK_SIZE_BYTE);`即可得到 block 的编号
