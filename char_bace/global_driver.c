#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define GLOBALMEM_SIZE 0X1000 //4k
#define MEM_CLEAR 0X1
#define GLOBALMEM_MAJOR 250 // main dev num
#define DEVICE_NUM 10
static int globalmem_major = GLOBALMEM_MAJOR;
static struct class *globalmem_class;
dev_t devno ;
/*device truct*/
struct globalmem_dev{
	struct cdev cdev;
	unsigned char mem[GLOBALMEM_SIZE];
};

struct globalmem_dev *globalmem_devp;

int globalmem_open(struct inode *inode, struct file *filep)
{
	//filep->private_data = globalmem_devp; //设备结构体指针赋值给文件私有数据
	struct globalmem_dev *dev = container_of(inode->i_cdev, struct globalmem_dev, cdev);
	filep->private_data = dev;
	return 0;
}
int globalmem_release(struct inode *inode, struct file *filep)
{
	return 0;
}
/*ioctl*/
static int globalmem_ioctl(struct inode *inode, struct file *filep, unsigned int cmd, unsigned long arg)
{
	//struct globalmem_dev *dev = filep->private_data;
	struct globalmem_dev *dev = container_of(inode->i_cdev, struct globalmem_dev, cdev);
	filep->private_data = dev;
	switch (cmd){
		case MEM_CLEAR:
			memset(dev->mem, 0, GLOBALMEM_SIZE);
			printk(KERN_INFO"globalmem is set to zero \n");
			break;
		default:
		return EINVAL;
		};
	return 0;
}

/*read fun*/
static ssize_t globalmem_read (struct file *filep, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;

	struct globalmem_dev  *dev = filep->private_data;

	if(p > GLOBALMEM_SIZE)
	{
		return 0;
	}
	else
	{
		if(count > GLOBALMEM_SIZE-p)
		count = GLOBALMEM_SIZE-p;
	}
	/*to user space*/
	if(copy_to_user(buf, (void*)(dev->mem+p), count))
	{
		return -EFAULT;
	}
	else
	{
		*ppos += count;
		printk(KERN_INFO"read %ud byte(s) form %lu\n",count, p);
	}
	return count;
	 
}
/*write fun*/
static ssize_t globalmem_write(struct file *filep, const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	
	struct globalmem_dev  *dev = filep->private_data;

	if(p > GLOBALMEM_SIZE)
		return 0;
	if(count >GLOBALMEM_SIZE-p)
	count = GLOBALMEM_SIZE-p;

	if(copy_from_user(dev->mem+p, buf, count))
	{
		return -EFAULT;
	}
	else
	{
		*ppos+=count;
		printk(KERN_INFO"written %u bype(s) from %lu\n",count, p);
	}
	return count;
}

/*file operate*/
static const struct file_operations globalmem_fops = {
	.owner = THIS_MODULE,
	.read = globalmem_read,
	.write = globalmem_write,
	.ioctl = globalmem_ioctl,
	//.unlocked_ioctl = globalmem_ioctl,
	.open = globalmem_open,
	.release = globalmem_release,
};

static void globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
	int err;
	unsigned char dev_name_chr[20] = {0};
	
	devno = MKDEV(globalmem_major, index);
	cdev_init(&dev->cdev,&globalmem_fops);
	dev->cdev.owner = THIS_MODULE;

	err = cdev_add(&dev->cdev,devno,1);
	if(err)
	printk(KERN_NOTICE"Error %d adding globalmem %d", err, index);
	/*devices create*/
	// 最后1个参数字符串，就是我们将来要在/dev目录下创建的设备文件的名字
	// 所以我们这里要的文件名是/dev/test
	sprintf(dev_name_chr,"globalmem_dev%d",index);
	device_create(globalmem_class, NULL, devno, NULL, dev_name_chr);
	printk(KERN_INFO"device_create %s:%d\n", dev_name_chr,MINOR(devno));
}


int globalmem_init(void)
{
	int result;
	int i;
	
	 /**/
	 if(globalmem_major)
 	{
 		devno = MKDEV(globalmem_major,0);
 		result = register_chrdev_region(devno, DEVICE_NUM, "globalmem");
 	}
	 else
 	{
	 	result = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem");
	 	globalmem_major = MAJOR(devno);
 	}
	 	
	if(result < 0)
		return result;
	/*apply for mem*/
	globalmem_devp = kmalloc(sizeof(struct globalmem_dev)*DEVICE_NUM, GFP_KERNEL);
	if(!globalmem_devp){
		result = -ENOMEM;
		printk(KERN_INFO"kmalloc error ...\n");
		goto fail_malloc;
		}
	memset(globalmem_devp,0,sizeof(struct globalmem_dev)*DEVICE_NUM);
	/*class create*/
	// 注册字符设备驱动完成后，添加设备类的操作，以让内核帮我们发信息
	// 给udev，让udev自动创建和删除设备文件
	globalmem_class = class_create(THIS_MODULE, "globalmem_class");
	if (IS_ERR(globalmem_class))
	return -EINVAL;
	
	for(i = 0; i < DEVICE_NUM; i ++)
	globalmem_setup_cdev(globalmem_devp+i,i);
	return 0;
	
fail_malloc:
	devno = MKDEV(globalmem_major,0);
	unregister_chrdev_region(devno, DEVICE_NUM);
	kfree(globalmem_devp);
	printk(KERN_INFO"fail_malloc...\n");
	return result;
}
void globalmem_exit(void)
{
	int i;
	printk(KERN_INFO"exit start");
	unregister_chrdev_region(MKDEV(globalmem_major,0),DEVICE_NUM);
	for(i = 0; i < DEVICE_NUM; i ++)
	{
		device_destroy(globalmem_class, MKDEV(globalmem_major,i));
		cdev_del(&(globalmem_devp+i)->cdev);
	}
	kfree(globalmem_devp);
	class_destroy(globalmem_class);
	printk(KERN_INFO"globalmem_exit\n");
} 

module_init(globalmem_init);
module_exit(globalmem_exit);
MODULE_LICENSE("GPL");
