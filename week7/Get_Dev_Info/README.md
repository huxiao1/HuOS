<!-- toc -->
Linux如何获取所有设备信息
- [Linux下的设备信息](#Linux下的设备信息)
- [数据结构](#数据结构)
    - [kobject和kset](#kobject和kset)
    - [总线](#总线)
    - [设备](#设备)
    - [驱动](#驱动)
    - [文件操作函数](#文件操作函数)
- [驱动程序实例](#驱动程序实例)
    - [驱动程序框架](#驱动程序框架)
    - [建立设备](#建立设备)
    - [打开，关闭，读写函数](#打开，关闭，读写函数)
    - [测试驱动](#测试驱动)
<!-- tocstop -->

从Linux 如何组织设备开始，然后研究设备驱动相关的数据结构，最后我们还是要一起写一个 Linux 设备驱动实例。  
# Linux下的设备信息
Linux 设备文件在/sys/bus 目录下。  
Linux 用 BUS（总线）组织设备和驱动，我们在 /sys/bus 目录下输入 tree 命令，就可以看到所有总线下的所有设备了  
![1](./images/1.png)  
**总线目录下有设备目录，设备目录下是设备文件**  

# 数据结构
总线、设备和驱动，加上两个数据结构来组织它们，那就是kobject和kset。  
## kobject和kset
kobject 和 kset 是构成 /sys 目录下的目录节点和文件节点的核心，也是层次化组织总线、设备、驱动的核心数据结构，kobject、kset 数据结构都能表示一个目录或者文件节点。  
```c
//每一个 kobject，都对应着 /sys 目录下（其实是 sysfs 文件系统挂载在 /sys 目录下） 的一个目录或者文件，目录或者文件的名字就是 kobject 结构中的 name
//kobject 结构中可以看出，它挂载在kset下，并且指向了kset
struct kobject {
    const char      *name;           //名称，反映在sysfs中
    struct list_head    entry;       //挂入kset结构的链表
    struct kobject      *parent;     //指向父结构 
    struct kset     *kset;           //指向所属的kset
    struct kobj_type    *ktype;
    struct kernfs_node  *sd;         //指向sysfs文件系统目录项 
    struct kref     kref;            //引用计数器结构
    unsigned int state_initialized:1;//初始化状态
    unsigned int state_in_sysfs:1;   //是否在sysfs中
    unsigned int state_add_uevent_sent:1;
    unsigned int state_remove_uevent_sent:1;
    unsigned int uevent_suppress:1;
};
```
kset 结构中本身又包含一个 kobject 结构，所以它既是 kobject 的容器，同时本身还是一个 kobject
```c
//kset 不仅仅自己是个 kobject，还能挂载多个 kobject，这说明 kset 是 kobject 的集合容器。
struct kset {
    struct list_head list; //挂载kobject结构的链表
    spinlock_t list_lock; //自旋锁
    struct kobject kobj;//自身包含一个kobject结构
    const struct kset_uevent_ops *uevent_ops;//暂时不关注
} __randomize_layout;
```
在 Linux 内核中，至少有两个顶层 kset
```c
struct kset *devices_kset;//管理所有设备
static struct kset *bus_kset;//管理所有总线
static struct kset *system_kset;
int __init devices_init(void)
{
    devices_kset = kset_create_and_add("devices", &device_uevent_ops, NULL);//建立设备kset
    return 0;
}
int __init buses_init(void)
{
    bus_kset = kset_create_and_add("bus", &bus_uevent_ops, NULL);//建立总线kset
    if (!bus_kset)
        return -ENOMEM;
    system_kset = kset_create_and_add("system", NULL, &devices_kset->kobj);//在设备kset之下建立system的kset
    if (!system_kset)
        return -ENOMEM;
    return 0;
}
```
![2](./images/2.png)  
一个类似文件目录的结构，这正是 kset 与 kobject 设计的目标之一。kset 与 kobject 结构只是基础数据结构，但是仅仅只有它的话，也就只能实现这个层次结构，其它的什么也不能干，根据我们以往的经验可以猜出，kset 与 kobject 结构肯定是嵌入到更高级的数据结构之中使用。  
## 总线
Linux 用总线组织设备和驱动，由此可见总线是 Linux 设备的基础，它可以表示 CPU 与设备的连接。Linux 把总线抽象成 bus_type 结构  
```c
struct bus_type {
    const char      *name;//总线名称
    const char      *dev_name;//用于列举设备，如（"foo%u", dev->id）
    struct device       *dev_root;//父设备
    const struct attribute_group **bus_groups;//总线的默认属性
    const struct attribute_group **dev_groups;//总线上设备的默认属性
    const struct attribute_group **drv_groups;//总线上驱动的默认属性
    //每当有新的设备或驱动程序被添加到这个总线上时调用
    int (*match)(struct device *dev, struct device_driver *drv);
    //当一个设备被添加、移除或其他一些事情时被调用产生uevent来添加环境变量。
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    //当一个新的设备或驱动程序添加到这个总线时被调用，并回调特定驱动程序探查函数，以初始化匹配的设备
    int (*probe)(struct device *dev);
    //将设备状态同步到软件状态时调用
    void (*sync_state)(struct device *dev);
    //当一个设备从这个总线上删除时被调用
    int (*remove)(struct device *dev);
    //当系统关闭时被调用
    void (*shutdown)(struct device *dev);
    //调用以使设备重新上线（在下线后）
    int (*online)(struct device *dev);
    //调用以使设备离线，以便热移除。可能会失败。
    int (*offline)(struct device *dev);
    //当这个总线上的设备想进入睡眠模式时调用
    int (*suspend)(struct device *dev, pm_message_t state);
    //调用以使该总线上的一个设备脱离睡眠模式
    int (*resume)(struct device *dev);
    //调用以找出该总线上的一个设备支持多少个虚拟设备功能
    int (*num_vf)(struct device *dev);
    //调用以在该总线上的设备配置DMA
    int (*dma_configure)(struct device *dev);
    //该总线的电源管理操作，回调特定的设备驱动的pm-ops
    const struct dev_pm_ops *pm;
    //此总线的IOMMU具体操作，用于将IOMMU驱动程序实现到总线上
    const struct iommu_ops *iommu_ops;
    //驱动核心的私有数据，只有驱动核心能够接触这个
    struct subsys_private *p;
    struct lock_class_key lock_key;
    //当探测或移除该总线上的一个设备时，设备驱动核心应该锁定该设备
    bool need_parent_lock;
};
```
总线不仅仅是组织设备和驱动的容器，还是同类设备的共有功能的抽象层。  
subsys_private是总线的驱动核心的私有数据  
```c
//通过kobject找到对应的subsys_private
#define to_subsys_private(obj) container_of(obj, struct subsys_private, subsys.kobj)
struct subsys_private {
    struct kset subsys;//定义这个子系统结构的kset
    struct kset *devices_kset;//该总线的"设备"目录，包含所有的设备
    struct list_head interfaces;//总线相关接口的列表
    struct mutex mutex;//保护设备，和接口列表
    struct kset *drivers_kset;//该总线的"驱动"目录，包含所有的驱动
    struct klist klist_devices;//挂载总线上所有设备的可迭代链表
    struct klist klist_drivers;//挂载总线上所有驱动的可迭代链表
    struct blocking_notifier_head bus_notifier;
    unsigned int drivers_autoprobe:1;
    struct bus_type *bus;   //指向所属总线
    struct kset glue_dirs;
    struct class *class;//指向这个结构所关联类结构的指针
};
```
通过 bus_kset(管理所有总线) 可以找到所有的 kset，通过 kset 又能找到 subsys_private，再通过 subsys_private 就可以找到总线了，也可以找到该总线上所有的设备与驱动。  
## 设备
```c
struct device {
    struct kobject kobj;
    struct device       *parent;//指向父设备
    struct device_private   *p;//设备的私有数据
    const char      *init_name; //设备初始化名字
    const struct device_type *type;//设备类型
    struct bus_type *bus;  //指向设备所属总线
    struct device_driver *driver;//指向设备的驱动
    void        *platform_data;//设备平台数据
    void        *driver_data;//设备驱动的私有数据
    struct dev_links_info   links;//设备供应商链接
    struct dev_pm_info  power;//用于设备的电源管理
    struct dev_pm_domain    *pm_domain;//提供在系统暂停时执行调用
#ifdef CONFIG_GENERIC_MSI_IRQ
    struct list_head    msi_list;//主机的MSI描述符链表
#endif
    struct dev_archdata archdata;
    struct device_node  *of_node; //用访问设备树节点
    struct fwnode_handle    *fwnode; //设备固件节点
    dev_t           devt;   //用于创建sysfs "dev"
    u32         id; //设备实例id
    spinlock_t      devres_lock;//设备资源链表锁
    struct list_head    devres_head;//设备资源链表
    struct class        *class;//设备的类
    const struct attribute_group **groups;  //可选的属性组
    void    (*release)(struct device *dev);//在所有引用结束后释放设备
    struct iommu_group  *iommu_group;//该设备属于的IOMMU组
    struct dev_iommu    *iommu;//每个设备的通用IOMMU运行时数据
};
```
device 结构中同样包含了 kobject 结构，这使得设备可以加入 kset 和 kobject 组建的层次结构中。device 结构中有总线和驱动指针，这能帮助设备找到自己的驱动程序和总线。  
## 驱动
```c
//指向设备的驱动
struct device_driver {
    const char      *name;//驱动名称
    struct bus_type     *bus;//指向总线
    struct module       *owner;//模块持有者
    const char      *mod_name;//用于内置模块
    bool suppress_bind_attrs;//禁用通过sysfs的绑定/解绑
    enum probe_type probe_type;//要使用的探查类型（同步或异步）
    const struct of_device_id   *of_match_table;//开放固件表
    const struct acpi_device_id *acpi_match_table;//ACPI匹配表
    //被调用来查询一个特定设备的存在
    int (*probe) (struct device *dev);
    //将设备状态同步到软件状态时调用
    void (*sync_state)(struct device *dev);
    //当设备被从系统中移除时被调用，以便解除设备与该驱动的绑定
    int (*remove) (struct device *dev);
    //关机时调用，使设备停止
    void (*shutdown) (struct device *dev);
    //调用以使设备进入睡眠模式，通常是进入一个低功率状态
    int (*suspend) (struct device *dev, pm_message_t state);
    //调用以使设备从睡眠模式中恢复
    int (*resume) (struct device *dev);
    //默认属性
    const struct attribute_group **groups;
    //绑定设备的属性
    const struct attribute_group **dev_groups;
    //设备电源操作
    const struct dev_pm_ops *pm;
    //当sysfs目录被写入时被调用
    void (*coredump) (struct device *dev);
    //驱动程序私有数据
    struct driver_private *p;
};

struct driver_private {
    struct kobject kobj;
    struct klist klist_devices;//驱动管理的所有设备的链表
    struct klist_node knode_bus;//加入bus链表的节点
    struct module_kobject *mkobj;//指向用kobject管理模块节点
    struct device_driver *driver;//指向驱动本身
};
```
在 device_driver 结构中，包含了驱动程序的名字、驱动程序所在模块、设备探查和电源相关的回调函数的指针。在 driver_private 结构中同样包含了 kobject 结构，用于组织所有的驱动，还指向了驱动本身。  
## 文件操作函数
![3](./images/3.png)  
杂项设备为例子研究一下，Linux 用 miscdevice 结构表示一个杂项设备  
```c
//this_device 指针，它指向下层的、属于这个杂项设备的 device 结构
struct miscdevice  {
    int minor;//设备号
    const char *name;//设备名称
    const struct file_operations *fops;//文件操作函数结构
    struct list_head list;//链表
    struct device *parent;//指向父设备的device结构
    struct device *this_device;//指向本设备的device结构
    const struct attribute_group **groups;
    const char *nodename;//节点名字
    umode_t mode;//访问权限
};
```
重点是file_operations 结构，设备一经注册，就会在 sys 相关的目录下建立设备对应的文件结点，对这个文件结点打开、读写等操作，最终会调用到驱动程序对应的函数，而对应的函数指针就保存在 file_operations 结构中  
```c
struct file_operations {
    struct module *owner;//所在的模块
    loff_t (*llseek) (struct file *, loff_t, int);//调整读写偏移
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);//读
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);//写
    int (*mmap) (struct file *, struct vm_area_struct *);//映射
    int (*open) (struct inode *, struct file *);//打开
    int (*flush) (struct file *, fl_owner_t id);//刷新
    int (*release) (struct inode *, struct file *);//关闭
} __randomize_layout;
```
以打开操作为例给你讲讲，Linux 的打开系统调用接口会调用 filp_open 函数，filp_open 函数的调用路径如下所示。  
```c
//filp_open
//file_open_name
//do_filp_open
//path_openat
static int do_o_path(struct nameidata *nd, unsigned flags, struct file *file)
{
    struct path path;
    int error = path_lookupat(nd, flags, &path);//解析文件路径得到文件inode节点
    if (!error) {
        audit_inode(nd->name, path.dentry, 0);
        error = vfs_open(&path, file);//vfs层打开文件接口
        path_put(&path);
    }
    return error;
}
int vfs_open(const struct path *path, struct file *file)
{
    file->f_path = *path;
    return do_dentry_open(file, d_backing_inode(path->dentry), NULL);
}
static int do_dentry_open(struct file *f, struct inode *inode,int (*open)(struct inode *, struct file *))
{
    //略过我们不想看的代码
    f->f_op = fops_get(inode->i_fop);//获取文件节点的file_operations
    if (!open)//如果open为空则调用file_operations结构中的open函数
        open = f->f_op->open;
    if (open) {
        error = open(inode, f);
    }
    //略过我们不想看的代码
    return 0;
}
```
file_operations 结构的地址存在一个文件的 inode 结构中。在 Linux 系统中，都是用 inode 结构表示一个文件，不管它是数据文件还是设备文件。  

# 驱动程序实例
这个驱动程序的主要工作，就是获取所有总线和其下所有设备的名字。为此我们需要先了解驱动程序的整体框架，接着建立我们总线和设备，然后实现驱动程序的打开、关闭，读写操作函数，最后我们写个应用程序，来测试我们的驱动程序。  
## 驱动程序框架
Linux 内核的驱动程序是在一个可加载的内核模块中实现，可加载的内核模块只需要两个函数和模块信息就行，但是我们要在模块中实现总线和设备驱动，所以需要更多的函数和数据结构  
```c
#define DEV_NAME  "devicesinfo"
#define BUS_DEV_NAME  "devicesinfobus"

static int misc_find_match(struct device *dev, void *data)
{
    printk(KERN_EMERG "device name is:%s\n", dev->kobj.name);
    return 0;
}
//对应于设备文件的读操作函数
static ssize_t misc_read (struct file *pfile, char __user *buff, size_t size, loff_t *off)
{
    printk(KERN_EMERG "line:%d,%s is call\n",__LINE__,__FUNCTION__);
    return 0;
}
//对应于设备文件的写操作函数
static ssize_t misc_write(struct file *pfile, const char __user *buff, size_t size, loff_t *off)
{
    printk(KERN_EMERG "line:%d,%s is call\n",__LINE__,__FUNCTION__);
    return 0;
}
//对应于设备文件的打开操作函数
static int  misc_open(struct inode *pinode, struct file *pfile)
{
    printk(KERN_EMERG "line:%d,%s is call\n",__LINE__,__FUNCTION__);
    return 0;
}
//对应于设备文件的关闭操作函数
static int misc_release(struct inode *pinode, struct file *pfile)
{
    printk(KERN_EMERG "line:%d,%s is call\n",__LINE__,__FUNCTION__);
    return 0;
}

static int devicesinfo_bus_match(struct device *dev, struct device_driver *driver)
{
        return !strncmp(dev->kobj.name, driver->name, strlen(driver->name));
}
//对应于设备文件的操作函数结构
static const  struct file_operations misc_fops = {
    .read     = misc_read,
    .write    = misc_write,
    .release  = misc_release,
    .open     = misc_open,
};
//misc设备的结构
static struct miscdevice  misc_dev =  {
    .fops  =  &misc_fops,         //设备文件操作方法
    .minor =  255,                //次设备号
    .name  =  DEV_NAME,           //设备名/dev/下的设备节点名
};
//总线结构
struct bus_type devicesinfo_bus = {
        .name = BUS_DEV_NAME, //总线名字
        .match = devicesinfo_bus_match, //总线match函数指针
};
//内核模块入口函数
static int __init miscdrv_init(void)
{
    printk(KERN_EMERG "INIT misc\n")；
    return 0;
}
//内核模块退出函数
static void  __exit miscdrv_exit(void)
{
    printk(KERN_EMERG "EXIT,misc\n");
}
module_init(miscdrv_init);//申明内核模块入口函数
module_exit(miscdrv_exit);//申明内核模块退出函数
MODULE_LICENSE("GPL");//模块许可
MODULE_AUTHOR("Hu");//模块开发者
```
这个模块一旦加载就会执行 miscdrv_init 函数，卸载时就会执行 miscdrv_exit 函数。insmod 指令是加载一个内核模块，一旦加载成功就会执行 miscdrv_init 函数  
## 建立设备
先来建立一个总线，然后在总线下建立一个设备。  
bus_register 函数向内核注册一个总线，相当于建立了一个总线，我们需要在 miscdrv_init 函数中调用它  
```c
static int __init miscdrv_init(void)
{
    printk(KERN_EMERG "INIT misc\n");
    busok = bus_register(&devicesinfo_bus);//注册总线
    return 0;
}
```
bus_register 函数会在系统中注册一个总线，所需参数就是总线结构的地址 (&devicesinfo_bus)，返回非 0 表示注册失败。现在我们来看看，在 bus_register 函数中都做了些什么事情  
```c
int bus_register(struct bus_type *bus)
{
    int retval;
    struct subsys_private *priv;
    //分配一个subsys_private结构
    priv = kzalloc(sizeof(struct subsys_private), GFP_KERNEL);
    //bus_type和subsys_private结构互相指向
    priv->bus = bus;
    bus->p = priv;
    //把总线的名称加入subsys_private的kobject中
    retval = kobject_set_name(&priv->subsys.kobj, "%s", bus->name);
    priv->subsys.kobj.kset = bus_kset;//指向bus_kset
    //把subsys_private中的kset注册到系统中
    retval = kset_register(&priv->subsys);
    //建立总线的文件结构在sysfs中
    retval = bus_create_file(bus, &bus_attr_uevent);
    //建立subsys_private中的devices和drivers的kset
    priv->devices_kset = kset_create_and_add("devices", NULL,
                         &priv->subsys.kobj);
    priv->drivers_kset = kset_create_and_add("drivers", NULL,
                         &priv->subsys.kobj);
    //建立subsys_private中的devices和drivers链表，用于属于总线的设备和驱动
    klist_init(&priv->klist_devices, klist_devices_get, klist_devices_put);
    klist_init(&priv->klist_drivers, NULL, NULL);
    return 0;
}
```
总线是怎么通过 subsys_private 把设备和驱动关联起来的：通过 bus_type 和 subsys_private 结构互相指向。  
下面建立一个 misc 杂项设备。misc 杂项设备需要定一个数据结构，然后调用 misc 杂项设备注册接口函数。  
```c
#define DEV_NAME  "devicesinfo"
static const  struct file_operations misc_fops = {
    .read     = misc_read,
    .write    = misc_write,
    .release  = misc_release,
    .open     = misc_open,
};
static struct miscdevice  misc_dev =  {
    .fops  =  &misc_fops,         //设备文件操作方法
    .minor =  255,                //次设备号
    .name  =  DEV_NAME,           //设备名/dev/下的设备节点名
};
static int __init miscdrv_init(void)
{
    misc_register(&misc_dev);//注册misc杂项设备
    printk(KERN_EMERG "INIT misc busok\n");
    busok = bus_register(&devicesinfo_bus);//注册总线
    return 0;
}
```
misc_register 函数到底做了什么
```c
int misc_register(struct miscdevice *misc)
{
    dev_t dev;
    int err = 0;
    bool is_dynamic = (misc->minor == MISC_DYNAMIC_MINOR);
    INIT_LIST_HEAD(&misc->list);
    mutex_lock(&misc_mtx);
    if (is_dynamic) {
        //minor次设备号如果等于255就自动分配次设备
        int i = find_first_zero_bit(misc_minors, DYNAMIC_MINORS);
        if (i >= DYNAMIC_MINORS) {
            err = -EBUSY;
            goto out;
        }
        misc->minor = DYNAMIC_MINORS - i - 1;
        set_bit(i, misc_minors);
    } else {
        //否则检查次设备号是否已经被占有
        struct miscdevice *c;
        list_for_each_entry(c, &misc_list, list) {
            if (c->minor == misc->minor) {
                err = -EBUSY;
                goto out;
            }
        }
    }
    dev = MKDEV(MISC_MAJOR, misc->minor);//合并主、次设备号
    //建立设备
    misc->this_device =
        device_create_with_groups(misc_class, misc->parent, dev,
                      misc, misc->groups, "%s", misc->name);
    //把这个misc加入到全局misc_list链表
    list_add(&misc->list, &misc_list);
 out:
    mutex_unlock(&misc_mtx);
    return err;
}
```
misc_register 函数只是负责分配设备号，以及把 miscdev 加入链表，真正的核心工作由 device_create_with_groups 函数来完成  
```c
struct device *device_create_with_groups(struct class *class,
                     struct device *parent, dev_t devt,void *drvdata,const struct attribute_group **groups,const char *fmt, ...)
{
    va_list vargs;
    struct device *dev;
    va_start(vargs, fmt);
    dev = device_create_groups_vargs(class, parent, devt, drvdata, groups,fmt, vargs);
    va_end(vargs);
    return dev;
}

struct device *device_create_groups_vargs(struct class *class, struct device *parent, dev_t devt, void *drvdata,const struct attribute_group **groups,const char *fmt, va_list args)
{
    struct device *dev = NULL;
    int retval = -ENODEV;
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);//分配设备结构的内存空间
    device_initialize(dev);//初始化设备结构
    dev->devt = devt;//设置设备号
    dev->class = class;//设置设备类
    dev->parent = parent;//设置设备的父设备
    dev->groups = groups;////设置设备属性
    dev->release = device_create_release;
    dev_set_drvdata(dev, drvdata);//设置miscdev的地址到设备结构中
    retval = kobject_set_name_vargs(&dev->kobj, fmt, args);//把名称设置到设备的kobjext中去
    retval = device_add(dev);//把设备加入到系统中
    if (retval)
        goto error;
    return dev;//返回设备
error:
    put_device(dev);
    return ERR_PTR(retval);
}
```
_在本代码目录中，执行 make 指令，就会产生一个 miscdvrv.ko 内核模块文件，我们把这个模块文件加载到 Linux 系统中就行了。_  
为了看到效果，我们还必须要做另一件事情。 在终端中用 sudo cat /proc/kmsg 指令读取 /proc/kmsg 文件，该文件是内核 prink 函数输出信息的文件  
```
#第一步在终端中执行如下指令
sudo cat /proc/kmsg
#第二步在另一个终端中执行如下指令
make
sudo insmod miscdrv.ko
#不用这个模块了可以用以下指令卸载
sudo rmmod miscdrv.ko
```
insmod 指令是加载一个内核模块，一旦加载成功就会执行 miscdrv_init 函数  
最后/dev 目录可以看到一个 devicesinfo 文件，同时你在 /sys/bus/ 目录下也可以看到一个 devicesinfobus 文件。这就是我们建立的设备和总线的文件节点的名称。  
## 打开，关闭，读写函数
我们的目的是要获取 Linux 中所有总线上的所有设备  
```c
#define to_subsys_private(obj) container_of(obj, struct subsys_private, subsys.kobj)//从kobject上获取subsys_private的地址
//正常情况下，我们是不能获取 bus_kset 地址的，它是所有总线的根，包含了所有总线的 kobject，Linux 为了保护 bus_kset，并没有在 bus_type 结构中直接包含 kobject，而是让总线指向一个 subsys_private 结构，在其中包含了 kobject 结构。
struct kset *ret_buskset(void)
{
    struct subsys_private *p;
    if(busok)
        return NULL;
    if(!devicesinfo_bus.p)
        return NULL;
    p = devicesinfo_bus.p;
    if(!p->subsys.kobj.kset)
        return NULL;
    //返回devicesinfo_bus总线上的kset，正是bus_kset
    return p->subsys.kobj.kset;
}
static int misc_find_match(struct device *dev, void *data)
{
    struct bus_type* b = (struct bus_type*)data;
    printk(KERN_EMERG "%s---->device name is:%s\n", b->name, dev->kobj.name);//打印总线名称和设备名称
    return 0;
}
static ssize_t misc_read (struct file *pfile, char __user *buff, size_t size, loff_t *off)
{
    struct kobject* kobj;
    struct kset* kset;
    struct subsys_private* p;
    kset = ret_buskset();//获取bus_kset的地址
    if(!kset)
        return 0;
    printk(KERN_EMERG "line:%d,%s is call\n",__LINE__,__FUNCTION__);//打印这个函数所在文件的行号和名称
    //扫描所有总线的kobject
    list_for_each_entry(kobj, &kset->list, entry)
    {
        p = to_subsys_private(kobj);
        printk(KERN_EMERG "Bus name is:%s\n",p->bus->name);
        //遍历具体总线上的所有设备
        bus_for_each_dev(p->bus, NULL, p->bus, misc_find_match);
    }
    return 0;
}
```
得到 bus_kset，根据它又能找到所有 subsys_private 结构中的 kobject，接着找到 subsys_private 结构，反向查询到 bus_type 结构的地址。  
调用 Linux 提供的 bus_for_each_dev 函数，就可以遍历一个总线上的所有设备，它每遍历到一个设备，就调用一个函数，这个函数是用参数的方式传给它的，在我们代码中就是 misc_find_match 函数。在调用 misc_find_match 函数时，会把一个设备结构的地址和另一个指针作为参数传递进来。最后就能打印每个设备的名称了。  
## 测试驱动
写一个应用程序，对设备文件进行读写
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define DEV_NAME "/dev/devicesinfo"
int main(void)
{
    char buf[] = {0, 0, 0, 0};
    int fd;
    //打开设备文件
    fd = open(DEV_NAME, O_RDWR);
    if (fd < 0) {
        printf("打开 :%s 失败!\n", DEV_NAME);
    }
    //写数据到内核空间
    write(fd, buf, 4);
    //从内核空间中读取数据
    read(fd, buf, 4);
    //关闭设备,也可以不调用，程序关闭时系统自动调用
    close(fd);
    return 0;
}
```