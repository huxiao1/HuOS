# HuOS: A hybrid kernel designed by me.
## Overview
## [First -- Prototype](./week1/README.md)  
首先借用GRUB实现一个最简单的内核  
![结果2](./week1/images/res2.png)

## [Second -- Pre-Design](./week2/README.md)
HuOS混合内核架构的整体设计思路：首先它是一个宏内核，但是同时具备有微内核的特点，模块化设计，支持动态可加载和卸载。    
![HuOS](./images/HuOS.png)  

## [Third -- Theoretical Background](./week3/README.md)
### Hardware  
#### [X86 CPU执行程序的三种模式](./week3/x86_mode/README.md)  
#### [程序中地址转换的方式](./week3/address_transfer/README.md)  
#### [Cache与内存](./week3/cache%26mem/README.md)
### Synchronization Primitives
#### [并发操作中，解决数据同步的四种方法](./week3/Data_Synchronization/README.md)
#### [Linux下的自旋锁和信号量的实现](./week3/Linux_Methond/README.md)