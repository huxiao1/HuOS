<!-- toc -->
实现文件的六大基本操作
- [辅助操作](#辅助操作)
    - [操作根目录文件](#操作根目录文件)
    - [获取文件名](#获取文件名)
    - [判断文件是否存在](#判断文件是否存在)
- [文件相关操作](#文件相关操作)
    - [新建文件](#新建文件)
    - [删除文件](#删除文件)
    - [打开文件](#打开文件)
    - [读写文件](#读写文件)
    - [关闭文件](#关闭文件)
- [串联整合](#串联整合)
- [测试](#测试)
<!-- tocstop -->

# 辅助操作
完成文件相关的操作，我们也需要大量的辅助函数  
## 操作根目录文件
不管是新建、删除、打开一个文件，首先都要找到与该文件对应的 rfsdir_t 结构。文件系统中，一个文件的 rfsdir_t 结构就储存在根目录文件中，所以想要读取文件对应的 rfsdir_t 结构，首先就要获取和释放根目录文件。  
```c
//获取根目录文件
void* get_rootdirfile_blk(device_t* devp)
{
    void* retptr = NULL;
    rfsdir_t* rtdir = get_rootdir(devp);//获取根目录文件的rfsdir_t结构
    //分配4KB大小的缓冲区并清零
    void* buf = new_buf(FSYS_ALCBLKSZ);
    hal_memset(buf, FSYS_ALCBLKSZ, 0);
    //读取根目录文件的逻辑储存块到缓冲区中
    read_rfsdevblk(devp, buf, rtdir->rdr_blknr)
    retptr = buf;//设置缓冲区的首地址为返回值
    goto errl1;
errl:
    del_buf(buf, FSYS_ALCBLKSZ);
errl1:
    del_rootdir(devp, rtdir);//释放根目录文件的rfsdir_t结构
    return retptr;
}
//释放根目录文件
void del_rootdirfile_blk(device_t* devp,void* blkp)
{
    //因为逻辑储存块的头512字节的空间中，保存的就是fimgrhd_t结构
    fimgrhd_t* fmp = (fimgrhd_t*)blkp;
    //把根目录文件回写到储存设备中去，块号为fimgrhd_t结构自身所在的块号
    write_rfsdevblk(devp, blkp, fmp->fmd_sfblk)
    //释放缓冲区
    del_buf(blkp, FSYS_ALCBLKSZ); 
    return;
}
```
get_rootdir 函数的作用就是读取文件系统超级块中 rfsdir_t 结构到一个缓冲区中，del_rootdir 函数则是用来释放这个缓冲区。超级块存的是根目录的rfsdir_t。  
释放根目录文件，就是把根目录文件的储存块回写到储存设备中去，最后释放对应的缓冲区就可以了。  
## 获取文件名
“/HuOS13.0/drivers/drvrfs.c”，这样的文件名包含了完整目录路径。除了第一个“/”是根目录外，其它的“/”只是一个目录路径分隔符。然而，在很多情况下，我们通常需要把目录路径分隔符去除，提取其中的目录名称或者文件名称。为了简化问题，我们对文件系统来点限制，我们的文件名只能是“/xxxx”这种类型的。  
实现去除路径分隔符提取文件名称的函数  
```c
//检查文件路径名
sint_t rfs_chkfilepath(char_t* fname)
{
    char_t* chp = fname;
    //检查文件路径名的第一个字符是否为“/”，不是则返回2
    if(chp[0] != '/') { return 2; }
    for(uint_t i = 1; ; i++)
    {
        //检查除第1个字符外其它字符中还有没有为“/”的，有就返回3
        if(chp[i] == '/') { return 3; }
        //如果这里i大于等于文件名称的最大长度，就返回4
        if(i >= DR_NM_MAX) { return 4; }
        //到文件路径字符串的末尾就跳出循环
        if(chp[i] == 0 && i > 1) { break; }
    }
    //返回0表示正确
    return 0;
}
//提取纯文件名
sint_t rfs_ret_fname(char_t* buf,char_t* fpath)
{
    //检查文件路径名是不是“/xxxx”的形式
    sint_t stus = rfs_chkfilepath(fpath);
    //如果不为0就直接返回这个状态值表示错误
    if(stus != 0) { return stus; }
    //从路径名字符串的第2个字符开始复制字符到buf中
    rfs_strcpy(&fpath[1], buf);
    return 0;
}
```
## 判断文件是否存在
新建文件时，无法新建相同文件名的文件；删除文件时，不能删除不存在的文件  
```c
sint_t rfs_chkfileisindev(device_t* devp,char_t* fname)
{
    sint_t rets = 6;
    sint_t ch = rfs_strlen(fname);//获取文件名的长度，注意不是文件路径名
    //检查文件名的长度是不是合乎要求
    if(ch < 1 || ch >= (sint_t)DR_NM_MAX) { return 4; }

    void* rdblkp = get_rootdirfile_blk(devp);
    fimgrhd_t* fmp = (fimgrhd_t*)rdblkp;
    //检查该fimgrhd_t结构的类型是不是FMD_DIR_TYPE，即这个文件是不是目录文件
    if(fmp->fmd_type != FMD_DIR_TYPE) { rets = 3; goto err; }
    //检查根目录文件是不是为空，即没有写入任何数据，所以返回0，表示根目录下没有对应的文件
    if(fmp->fmd_curfwritebk == fmp->fmd_fleblk[0].fb_blkstart &&
 fmp->fmd_curfinwbkoff == fmp->fmd_fileifstbkoff) {
        rets = 0; goto err;
    }

    rfsdir_t* dirp = (rfsdir_t*)((uint_t)(fmp) + fmp->fmd_fileifstbkoff);//指向根目录文件的第一个字节
    //指向根目录文件的结束地址
    void* maxchkp = (void*)((uint_t)rdblkp + FSYS_ALCBLKSZ - 1);
    //当前的rfsdir_t结构的指针比根目录文件的结束地址小，就继续循环    
    for(;(void*)dirp < maxchkp;) {
        //如果这个rfsdir_t结构的类型是RDR_FIL_TYPE，说明它对应的是文件而不是目录，所以下面就继续比较其文件名
        if(dirp->rdr_type == RDR_FIL_TYPE) {
            if(rfs_strcmp(dirp->rdr_name,fname) == 1) {//比较其文件名
                rets = 1; goto err;
            }
        }
        dirp++;
    }
    rets = 0; //到了这里说明没有找到相同的文件
err:
    del_rootdirfile_blk(devp,rdblkp);//释放根目录文件
    return rets;
}
```
rfs_chkfileisindev 函数逻辑很简单。首先是检查文件名的长度，接着获取了根目录文件，然后遍历根其中的所有 rfsdir_t 结构并比较文件名是否相同，相同就返回 1，不同就返回其它值，最后释放了根目录文件。  
因为 get_rootdirfile_blk 函数已经把根目录文件读取到内存里了，所以可以用 dirp 指针和 maxchkp 指针操作其中的数据。  

# 文件相关操作
## 新建文件
1. 从文件路径名中提取出纯文件名，检查储存设备上是否已经存在这个文件。
2. 分配一个空闲的逻辑储存块，并在根目录文件的末尾写入这个新建文件对应的 rfsdir_t 结构。
3. 在一个新的 4KB 大小的缓冲区中，初始化新建文件对应的 fimgrhd_t 结构。
4. 把第 3 步对应的缓冲区里的数据，写入到先前分配的空闲逻辑储存块中。
```c
//新建文件的接口函数
drvstus_t rfs_new_file(device_t* devp, char_t* fname, uint_t flg)
{
    //在栈中分配一个字符缓冲区并清零
    char_t fne[DR_NM_MAX];
    hal_memset((void*)fne, DR_NM_MAX, 0);
    //从文件路径名中提取出纯文件名
    if(rfs_ret_fname(fne, fname) != 0) { return DFCERRSTUS; }
    //检查储存介质上是否已经存在这个新建的文件，如果是则返回错误
    if(rfs_chkfileisindev(devp, fne) != 0) {return DFCERRSTUS; }
    //调用实际建立文件的函数
    return rfs_new_dirfileblk(devp, fne, RDR_FIL_TYPE, 0);
}
```
```c
drvstus_t rfs_new_dirfileblk(device_t* devp,char_t* fname,uint_t flgtype,uint_t val)
{
    drvstus_t rets = DFCERRSTUS;
    void* buf = new_buf(FSYS_ALCBLKSZ);//分配一个4KB大小的缓冲区
    hal_memset(buf, FSYS_ALCBLKSZ, 0);//清零该缓冲区
    uint_t fblk = rfs_new_blk(devp);//分配一个新的空闲逻辑储存块
    void* rdirblk = get_rootdirfile_blk(devp);//获取根目录文件
    fimgrhd_t* fmp = (fimgrhd_t*)rdirblk;
    //指向文件当前的写入地址，因为根目录文件已经被读取到内存中了
    rfsdir_t* wrdirp = (rfsdir_t*)((uint_t)rdirblk + fmp->fmd_curfinwbkoff);
    //对文件当前的写入地址进行检查
    if(((uint_t)wrdirp) >= ((uint_t)rdirblk + FSYS_ALCBLKSZ)) {
        rets=DFCERRSTUS; goto err;
    }
    wrdirp->rdr_stus = 0;
    wrdirp->rdr_type = flgtype;//设为文件类型
    wrdirp->rdr_blknr = fblk;//设为刚刚分配的空闲逻辑储存块
    rfs_strcpy(fname, wrdirp->rdr_name);//把文件名复制到rfsdir_t结构
    fmp->fmd_filesz += (uint_t)(sizeof(rfsdir_t));//增加根目录文件的大小
    //增加根目录文件当前的写入地址，保证下次不被覆盖
    fmp->fmd_curfinwbkoff += (uint_t)(sizeof(rfsdir_t));
    fimgrhd_t* ffmp = (fimgrhd_t*)buf;//指向新分配的缓冲区
    fimgrhd_t_init(ffmp);//调用fimgrhd_t结构默认的初始化函数
    ffmp->fmd_type = FMD_FIL_TYPE;//因为建立的是文件，所以设为文件类型
    ffmp->fmd_sfblk = fblk;//把自身所在的块，设为分配的逻辑储存块
    ffmp->fmd_curfwritebk = fblk;//把当前写入的块，设为分配的逻辑储存块
    ffmp->fmd_curfinwbkoff = 0x200;//把当前写入块的写入偏移量设为512
    //把文件储存块数组的第1个元素的开始块，设为刚刚分配的空闲逻辑储存块
    ffmp->fmd_fleblk[0].fb_blkstart = fblk;
    //因为只分配了一个逻辑储存块，所以设为1
    ffmp->fmd_fleblk[0].fb_blknr = 1;
    //把缓冲区中的数据写入到刚刚分配的空闲逻辑储存块中
    if(write_rfsdevblk(devp, buf, fblk) == DFCERRSTUS) {
        rets = DFCERRSTUS; goto err;
    }
    rets = DFCOKSTUS;
err:
    del_rootdirfile_blk(devp, rdirblk);//释放根目录文件
err1:
    del_buf(buf, FSYS_ALCBLKSZ);//释放缓冲区
    return rets;
}
```
前面反复提到的目录文件中存放的就是一系列的 rfsdir_t 结构。  
fmp 和 ffmp 这两个指针很重要。fmp 指针指向的是根目录文件的 fimgrhd_t 结构，因为要写入一个新的 rfsdir_t 结构，所以要获取并改写根目录文件的 fimgrhd_t 结构中的数据。而 ffmp 指针指向的是新建文件的 fimgrhd_t 结构，并且初始化了其中的一些数据。最后，该函数把这个缓冲区中的数据写入到分配的空闲逻辑储存块中，同时释放了根目录文件和缓冲区。  
## 删除文件
1. 从文件路径名中提取出纯文件名。
2. 获取根目录文件，从根目录文件中查找待删除文件的 rfsdir_t 结构，然后释放该文件占用的逻辑储存块。
3. 初始化与待删除文件相对应的 rfsdir_t 结构，并设置 rfsdir_t 结构的类型为 RDR_DEL_TYPE。
4. 释放根目录文件。
接口函数  
```c
//文件删除的接口函数
drvstus_t rfs_del_file(device_t* devp, char_t* fname, uint_t flg)
{
    if(flg != 0) {
        return DFCERRSTUS;
    }
    return rfs_del_dirfileblk(devp, fname, RDR_FIL_TYPE, 0);
}
```
```c
drvstus_t rfs_del_dirfileblk(device_t* devp, char_t* fname, uint_t flgtype, uint_t val)
{
    if(flgtype != RDR_FIL_TYPE || val != 0) { return DFCERRSTUS; }
    char_t fne[DR_NM_MAX];
    hal_memset((void*)fne, DR_NM_MAX, 0);
    //提取纯文件名
    if(rfs_ret_fname(fne,fname) != 0) { return DFCERRSTUS; }
    //调用删除文件的核心函数
    if(del_dirfileblk_core(devp, fne) != 0) { return DFCERRSTUS; }
    return DFCOKSTUS;
}
```
```c
//删除文件的核心函数
sint_t del_dirfileblk_core(device_t* devp, char_t* fname)
{
    sint_t rets = 6;
    void* rblkp = get_rootdirfile_blk(devp);//获取根目录文件
    fimgrhd_t* fmp = (fimgrhd_t*)rblkp;
    if(fmp->fmd_type!=FMD_DIR_TYPE) { //检查根目录文件的类型
        rets=4; goto err;
    }
    if(fmp->fmd_curfwritebk == fmp->fmd_fleblk[0].fb_blkstart && fmp->fmd_curfinwbkoff == fmp->fmd_fileifstbkoff) { //检查根目录文件中有没有数据
        rets = 3; goto err;
    }
    rfsdir_t* dirp = (rfsdir_t*)((uint_t)(fmp) + fmp->fmd_fileifstbkoff);
    void* maxchkp = (void*)((uint_t)rblkp + FSYS_ALCBLKSZ-1);
    for(;(void*)dirp < maxchkp;) {
        if(dirp->rdr_type == RDR_FIL_TYPE) {//检查其类型是否为文件类型
            //如果文件名相同，就执行以下删除动作
            if(rfs_strcmp(dirp->rdr_name, fname) == 1) {
                //释放rfsdir_t结构的rdr_blknr中指向的逻辑储存块
                rfs_del_blk(devp, dirp->rdr_blknr);
                //初始化rfsdir_t结构，实际上是清除其中的数据
                rfsdir_t_init(dirp);
                //设置rfsdir_t结构的类型为删除类型，表示它已经删除
                dirp->rdr_type = RDR_DEL_TYPE;
                rets = 0; goto err;
            }
        }
        dirp++;//下一个rfsdir_t
    }
    rets=1;
err:
    del_rootdirfile_blk(devp,rblkp);//释放根目录文件
    return rets;
}
```
遍历根目录文件中所有的 rfsdir_t 结构，并比较其文件名，看看删除的文件名称是否相同，相同就释放该 rfsdir_t 结构的 rdr_blknr 字段对应的逻辑储存块，清除该 rfsdir_t 结构中的数据，同时设置该 rfsdir_t 结构的类型为删除类型。  
rfsdir_t* dirp = (rfsdir_t*)((uint_t)(fmp) + fmp->fmd_fileifstbkoff);   根目录文件的第一个字节  
void* maxchkp = (void*)((uint_t)rblkp + FSYS_ALCBLKSZ-1);  根目录文件的结束地址  
_删除一个文件，就是把这个文件对应的 rfsdir_t 结构中的数据清空，这样就无法查找到这个文件了。同时，也要释放该文件占用的逻辑储存块。因为没有清空文件数据，所以可以通过反删除软件找回文件。_  
## 打开文件
内核上层组件调用设备驱动程序时，都需要建立一个相应的 objnode_t 结构，把这个 I/O 包发送给相应的驱动程序，但是 objnode_t 结构不仅仅是用于驱动程序，它还用于表示进程使用了哪些资源，例如打开了哪些设备或者文件，而每打开一个设备或者文件就建立一个 objnode_t 结构，放在特定进程的资源表中。  
HuOS13.0/include/krlinc/krlobjnode_t.h 文件中，需要在 objnode_t 结构中增加一些东西  
```c
#define OBJN_TY_DEV 1//设备类型
#define OBJN_TY_FIL 2//文件类型
#define OBJN_TY_NUL 0//默认类型
typedef struct s_OBJNODE
{
    spinlock_t  on_lock;
    list_h_t    on_list;
    sem_t       on_complesem;
    uint_t      on_flgs;
    uint_t      on_stus;
    //……
    void*       on_fname;//文件路径名指针，表示打开哪个文件
    void*       on_finode;//文件对应的fimgrhd_t结构指针，因为要知道一个文件的所有信息
    void*       on_extp;//扩展所用
}objnode_t;
```
1. 从 objnode_t 结构的文件路径提取文件名。
2. 获取根目录文件，在该文件中搜索对应的 rfsdir_t 结构，看看文件是否存在。
3. 分配一个 4KB 缓存区，把该文件对应的 rfsdir_t 结构中指向的逻辑储存块读取到缓存区中，然后释放根目录文件。
4. 把缓冲区中的 fimgrhd_t 结构的地址，保存到 objnode_t 结构的 on_finode 域中。
```c
//打开文件的接口函数
drvstus_t rfs_open_file(device_t* devp, void* iopack)
{
    objnode_t* obp = (objnode_t*)iopack;
    //检查objnode_t中的文件路径名
    if(obp->on_fname == NULL) {
        return DFCERRSTUS;
    }
    //调用打开文件的核心函数
    void* fmdp = rfs_openfileblk(devp, (char_t*)obp->on_fname);
    if(fmdp == NULL) {
        return DFCERRSTUS;
    }
    //把返回的fimgrhd_t结构的地址保存到objnode_t中的on_finode字段中
    obp->on_finode = fmdp;
    return DFCOKSTUS;
}
```
接口函数 rfs_open_file 中只是对参数进行了检查。然后调用了核心函数，这个函数就是 rfs_openfileblk
```c
//打开文件的核心函数
void* rfs_openfileblk(device_t *devp, char_t* fname)
{
    char_t fne[DR_NM_MAX]; 
    void* rets = NULL,*buf = NULL;
    hal_memset((void*)fne,DR_NM_MAX,0);
    if(rfs_ret_fname(fne, fname) != 0) {//从文件路径名中提取纯文件名
        return NULL;
    }
    void* rblkp = get_rootdirfile_blk(devp); //获取根目录文件
    fimgrhd_t* fmp = (fimgrhd_t*)rblkp;
    if(fmp->fmd_type != FMD_DIR_TYPE) {//判断根目录文件的类型是否合理
        rets = NULL; goto err;
    }
    //判断根目录文件里有没有数据
    if(fmp->fmd_curfwritebk == fmp->fmd_fleblk[0].fb_blkstart && 
fmp->fmd_curfinwbkoff == fmp->fmd_fileifstbkoff) { 
        rets = NULL; goto err;
    }
    rfsdir_t* dirp = (rfsdir_t*)((uint_t)(fmp) + fmp->fmd_fileifstbkoff); 
    void* maxchkp = (void*)((uint_t)rblkp + FSYS_ALCBLKSZ - 1);
    for(;(void*)dirp < maxchkp;) {//开始遍历文件对应的rfsdir_t结构
        if(dirp->rdr_type == RDR_FIL_TYPE) {
            //如果文件名相同就跳转到opfblk标号处运行
            if(rfs_strcmp(dirp->rdr_name, fne) == 1) {
                goto opfblk;
            }
        }
        dirp++;
    }
    //如果到这里说明没有找到该文件对应的rfsdir_t结构，所以设置返回值为NULL
    rets = NULL; goto err;
opfblk:
    buf = new_buf(FSYS_ALCBLKSZ);//分配4KB大小的缓冲区
    //读取该文件占用的逻辑储存块
    if(read_rfsdevblk(devp, buf, dirp->rdr_blknr) == DFCERRSTUS) {
        rets = NULL; goto err1;
    }
    fimgrhd_t* ffmp = (fimgrhd_t*)buf;
    if(ffmp->fmd_type == FMD_NUL_TYPE || ffmp->fmd_fileifstbkoff != 0x200) {//判断将要打开的文件是否合法
        rets = NULL; goto err1;
    }
    rets = buf; goto err;//设置缓冲区首地址为返回值
err1:
    del_buf(buf, FSYS_ALCBLKSZ); //上面的步骤若出现问题就要释放缓冲区
err:
    del_rootdirfile_blk(devp, rblkp); //释放根目录文件
    return rets;
        }
```
rfs_openfileblk 函数中的 for 循环，可以遍历要打开的文件在根目录文件中对应的 rfsdir_t 结构，然后把对应文件占用的逻辑储存块读取到缓冲区中，最后返回这个缓冲区的首地址。因为这个缓冲区开始的空间中，就存放着其文件对应的 fimgrhd_t 结构，所以返回 fimgrhd_t 结构的地址，整个打开文件的流程就结束了。  
## 读写文件
文件的读写包含两个操作，一个是从储存设备中读取文件的数据，另一个是把文件的数据写入到储存设备中。
1. 检查 objnode_t 结构中用于存放文件数据的缓冲区及其大小。
2. 检查 imgrhd_t 结构中文件相关的信息。
3. 把文件的数据读取到 objnode_t 结构中指向的缓冲区中。
读文件  
```c
//读取文件数据的接口函数
drvstus_t rfs_read_file(device_t* devp,void* iopack)
{
    objnode_t* obp = (objnode_t*)iopack;
    //检查文件是否已经打开，以及用于存放文件数据的缓冲区和它的大小是否合理
    if(obp->on_finode == NULL || obp->on_buf == NULL || obp->on_bufsz != FSYS_ALCBLKSZ) {
        return DFCERRSTUS;
    }
    return rfs_readfileblk(devp, (fimgrhd_t*)obp->on_finode, obp->on_buf, obp->on_len);
}
//实际读取文件数据的函数
drvstus_t rfs_readfileblk(device_t* devp, fimgrhd_t* fmp, void* buf, uint_t len)
{
    //检查文件的相关信息是否合理
    if(fmp->fmd_sfblk != fmp->fmd_curfwritebk || fmp->fmd_curfwritebk != fmp->fmd_fleblk[0].fb_blkstart) {
        return DFCERRSTUS;
    }
    //检查读取文件数据的长度是否大于（4096-512）
    if(len > (FSYS_ALCBLKSZ - fmp->fmd_fileifstbkoff)) {
        return DFCERRSTUS;
    }
    //指向文件数据的开始地址
    void* wrp = (void*)((uint_t)fmp + fmp->fmd_fileifstbkoff);
    //把文件开始处的数据复制len个字节到buf指向的缓冲区中
    hal_memcpy(wrp, buf, len);
    return DFCOKSTUS;
}
```
打开文件的函数已经把文件数据复制到一个缓冲区中了，rfs_readfileblk 函数中的参数 buf、len 都是接口函数 rfs_read_file 从 objnode_t 结构中提取出来的  
向文件中写入数据，和读取文件的流程一样，只不过要将要写入的数据复制到打开文件时为其分配的缓冲区中，最后还要把打开文件时为其分配的缓冲区中的数据，写入到相应的逻辑储存块中。  
```c
//写入文件数据的接口函数
drvstus_t rfs_write_file(device_t* devp, void* iopack)
{
    objnode_t* obp = (objnode_t*)iopack;
    //检查文件是否已经打开，以及用于存放文件数据的缓冲区和它的大小是否合理
    if(obp->on_finode == NULL || obp->on_buf == NULL || obp->on_bufsz != FSYS_ALCBLKSZ) {
        return DFCERRSTUS;
    }
    return rfs_writefileblk(devp, (fimgrhd_t*)obp->on_finode, obp->on_buf, obp->on_len);
}
//实际写入文件数据的函数
drvstus_t rfs_writefileblk(device_t* devp, fimgrhd_t* fmp, void* buf, uint_t len)
{
    //检查文件的相关信息是否合理
    if(fmp->fmd_sfblk != fmp->fmd_curfwritebk || fmp->fmd_curfwritebk != fmp->fmd_fleblk[0].fb_blkstart) {
        return DFCERRSTUS;
    }
    //检查当前将要写入数据的偏移量加上写入数据的长度，是否大于等于4KB
    if((fmp->fmd_curfinwbkoff + len) >= FSYS_ALCBLKSZ) {
        return DFCERRSTUS;
    }
    //指向将要写入数据的内存空间
    void* wrp = (void*)((uint_t)fmp + fmp->fmd_curfinwbkoff);
    //把buf缓冲区中的数据复制len个字节到wrp指向的内存空间中去
    hal_memcpy(buf, wrp, len);
    fmp->fmd_filesz += len;//增加文件大小
    //使fmd_curfinwbkoff指向下一次将要写入数据的位置
    fmp->fmd_curfinwbkoff += len;
    //把文件数据写入到相应的逻辑储存块中，完成数据同步
    write_rfsdevblk(devp, (void*)fmp, fmp->fmd_curfwritebk);
    return DFCOKSTUS;
}
```
rfs_writefileblk 函数永远都是从 fimgrhd_t 结构的 fmd_curfinwbkoff 字段中的偏移量开始写入文件数据的，比如向空文件中写入 2 个字节，那么其 fmd_curfinwbkoff 字段的值就是 2，因为第 0、1 个字节空间已经被占用了，这就是追加写入数据的方式。  
## 关闭文件
首先检查文件是否已经打开。然后把文件写入到对应的逻辑储存块中，完成数据的同步。最后释放文件数据占用的缓冲区。  
```c
//关闭文件的接口函数
drvstus_t rfs_close_file(device_t* devp, void* iopack)
{
    objnode_t* obp = (objnode_t*)iopack;
    //检查文件是否已经打开了
    if(obp->on_finode == NULL) {
        return DFCERRSTUS;
    }
    return rfs_closefileblk(devp, obp->on_finode);
}
//关闭文件的核心函数
drvstus_t rfs_closefileblk(device_t *devp, void* fblkp)
{
    //指向文件的fimgrhd_t结构
    fimgrhd_t* fmp = (fimgrhd_t*)fblkp;
    //完成文件数据的同步
    write_rfsdevblk(devp, fblkp, fmp->fmd_sfblk);
    //释放缓冲区
    del_buf(fblkp, FSYS_ALCBLKSZ);
    return DFCOKSTUS;
}
```
目前的情况下，rfs_closefileblk 函数中是没有必要调用 write_rfsdevblk 函数的，因为前面在写入文件数据的同时，就已经把文件的数据写入到逻辑储存块中去了。最后释放了先前打开文件时分配的缓冲区  
objnode_t 结构不应该在此释放，它是由 HuOS 内核上层组件进行释放的。  

# 串联整合
我们的文件系统是以设备的形式存在的，所以文件操作的接口，必须要串联整合到文件系统设备驱动程序之中，文件系统才能真正工作。  
首先来串联整合文件系统的打开文件操作和新建文件操作  
```c
drvstus_t rfs_open(device_t* devp, void* iopack)
{
    objnode_t* obp=(objnode_t*)iopack;
    //根据objnode_t结构中的访问标志进行判断
    if(obp->on_acsflgs == FSDEV_OPENFLG_OPEFILE) {
        return rfs_open_file(devp, iopack);
    }
    if(obp->on_acsflgs == FSDEV_OPENFLG_NEWFILE) {
        return rfs_new_file(devp, obp->on_fname, 0);
    }
    return DFCERRSTUS;
}
//rfs_open 函数对应于设备驱动程序的打开功能派发函数，但没有相应的新建功能派发函数，于是我们就根据 objnode_t 结构中访问标志域设置不同的编码，来进行判断。
```
接着是关闭文件  
```c
drvstus_t rfs_close(device_t* devp, void* iopack)
{
    return rfs_close_file(devp, iopack);
}
```
然后是文件读写操作的串联整合，设备驱动程序也有对应的读写功能派发函数，同样也是直接调用文件读写操作的接口函数即可  
```c
drvstus_t rfs_read(device_t* devp, void* iopack)
{
    //调用读文件操作的接口函数
    return rfs_read_file(devp, iopack);
}
drvstus_t rfs_write(device_t* devp, void* iopack)
{
    //调用写文件操作的接口函数
    return rfs_write_file(devp, iopack);
}
```
删除文件  
```c
drvstus_t rfs_ioctrl(device_t* devp, void* iopack)
{
    objnode_t* obp = (objnode_t*)iopack;
    //根据objnode_t结构中的控制码进行判断
    if(obp->on_ioctrd == FSDEV_IOCTRCD_DELFILE)
    {
        //调用删除文件操作的接口函数
        return rfs_del_file(devp, obp->on_fname, 0);
    }
    return DFCERRSTUS;
}
```
# 测试
HuOS 下的任何设备驱动程序都必须要有 objnode_t 结构才能运行。所以，在这里我们需要手动建立一个 objnode_t 结构并设置好其中的字段，模拟一下 HuOS 上层组件调用设备驱动程序的过程。  
```c
void test_fsys(device_t *devp)
{
    kprint("开始文件操作测试\n");
    void *rwbuf = new_buf(FSYS_ALCBLKSZ);//分配缓冲区
    //把缓冲区中的所有字节都置为0xff
    hal_memset(rwbuf, 0xff, FSYS_ALCBLKSZ);
    objnode_t *ondp = krlnew_objnode();//新建一个objnode_t结构
    ondp->on_acsflgs = FSDEV_OPENFLG_NEWFILE;//设置新建文件标志
    ondp->on_fname = "/testfile";//设置新建文件名
    ondp->on_buf = rwbuf;//设置缓冲区
    ondp->on_bufsz = FSYS_ALCBLKSZ;//设置缓冲区大小
    ondp->on_len = 512;//设置读写多少字节
    ondp->on_ioctrd = FSDEV_IOCTRCD_DELFILE;//设置控制码
    if (rfs_open(devp, ondp) == DFCERRSTUS) {//新建文件
        hal_sysdie("新建文件错误");
    }
    ondp->on_acsflgs = FSDEV_OPENFLG_OPEFILE;//设置打开文件标志
    if (rfs_open(devp, ondp) == DFCERRSTUS) {//打开文件
        hal_sysdie("打开文件错误");
    }
    if (rfs_write(devp, ondp) == DFCERRSTUS) {//把数据写入文件
        hal_sysdie("写入文件错误");
    }
    hal_memset(rwbuf, 0, FSYS_ALCBLKSZ);//清零缓冲区
    if (rfs_read(devp, ondp) == DFCERRSTUS) {//读取文件数据
        hal_sysdie("读取文件错误");
    }
    if (rfs_close(devp, ondp) == DFCERRSTUS) {//关闭文件
        hal_sysdie("关闭文件错误");
    }
    u8_t *cb = (u8_t *)rwbuf;//指向缓冲区
    for (uint_t i = 0; i < 512; i++) {//检查缓冲区空间中的头512个字节的数据，是否为0xff
        if (cb[i] != 0xff) {//如果不等于0xff就死机
            hal_sysdie("检查文件内容错误");
        }
        kprint("testfile文件第[%x]个字节数据:%x\n", i, (uint_t)cb[i]);//打印文件内容
    }
    if (rfs_ioctrl(devp, ondp) == DFCERRSTUS){//删除文件
        hal_sysdie("删除文件错误");
    }
    ondp->on_acsflgs = FSDEV_OPENFLG_OPEFILE;//再次设置打开文件标志
    if (rfs_open(devp, ondp) == DFCERRSTUS) {//再次打开文件
        hal_sysdie("再次打开文件失败");
    }
    hal_sysdie("结束文件操作测试");
    return;
}
```
开始会建立并打开一个文件，接着写入数据，然后读取文件中数据进行比较，看看是不是和之前写入的数据相等，最后删除这个文件并再次打开，看是否会出错。因为文件已经删除了，打开一个已经删除的文件自然要出错，出错就说明测试成功。  