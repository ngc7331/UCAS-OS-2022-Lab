# Project 6

## 目录
- [Project 6](#project-6)
  - [目录](#目录)
    - [间址块的支持](#间址块的支持)
      - [`find_indirect_block`](#find_indirect_block)



### 间址块的支持
#### `find_indirect_block`
递归函数，假设`parent`是第`level+1`级间址块，从它中寻找第`block_no`个块。当`new_block`不为`-1`时，建立不存在的索引项。

- 若`level == 0`
  - `parent`的第`block_no`项即为这个块的编号，返回
  - 若`new_block`不为`-1`时，将它的值填入该项中
- 否则
  - 找到第`level`级间址块，也即`parent`的第`block_no / addr_num_per_block ^ level`项
  - 若`new_block`不为`-1`时，创建不存在的第`level`级间址块，并将其编号填入该项中
  - 递归调用，从`level`级间址块中查找第`block_no % addr_num_per_block ^ level`项
