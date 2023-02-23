<!-- toc -->
虽然之前内存管理相关的数据结构已经定义好了，但是我们还没有在内存中建立对应的实例变量。在代码中实际操作的数据结构必须在内存中有相应的变量，这节我们就去建立对应的实例变量，并初始化它们。  
- [初始化](#初始化)
<!-- tocstop -->

# 初始化
_之前，我们在 hal 层初始化中，初始化了从二级引导器中获取的内存布局信息，也就是那个 e820map_t 数组，并把这个数组转换成了 phymmarge_t 结构数组，还对它做了排序。但我们 HuOS 物理内存管理器剩下的部分还没有完成初始化。_  

HuOS 的物理内存管理器，我们依然要放在 HuOS 的 hal 层。
因为物理内存还和硬件平台相关，所以我们要在 HuOS/hal/x86/ 目录下建立一个 memmgrinit.c 文件，在这个文件中写入一个 HuOS 物理内存管理器初始化的大总管——init_memmgr 函数，并在 init_halmm 函数中调用它
```
//HuOS/hal/x86/halmm.c

//hal层的内存初始化函数
void init_halmm()
{
    init_phymmarge();
    init_memmgr();
    return;
}

//HuOS物理内存管理器初始化
void init_memmgr()
{
    //初始化内存页结构msadsc_t
    //初始化内存区结构memarea_t
    return;
}
```






