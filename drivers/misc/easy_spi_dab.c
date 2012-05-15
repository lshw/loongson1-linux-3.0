#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <asm/ioctl.h>

#define	CMD_DAB_ON	_IO('E', 0x11)
#define	CMD_DAB_OFF	_IO('E', 0x12)

#define	BUF_LENGTH	2048
#define	SINGLE_LENGTH	8
#define	BUF_COUNT	(BUF_LENGTH / SINGLE_LENGTH)

struct easy_dab_ring
{
	int ring_head;	//for recorde read pos
	int ring_tail;	//for recorde write pos
	int state;
	unsigned char *ring_buf;
	struct mutex easy_dab_mutex;
	struct completion easy_dab_completion;
	struct work_struct easy_work;
};

static struct easy_dab_ring *ring = NULL;
static struct spi_device *easy_dab_spi = NULL;
static int __devinit easy_dab_probe(struct spi_device *spi);

static int easy_dab_open(struct inode *inode, struct file *file)
{
	schedule_work (&ring->easy_work);
	ring->state = 1;
	return 0;
}

static int easy_dab_close(struct inode *inode, struct file *file)
{
	cancel_work_sync (&ring->easy_work);
	ring->state = 0;
	return 0;
}

static long easy_dab_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
		case	CMD_DAB_ON:
			if (ring->state == 1)
				break;
			schedule_work (&ring->easy_work);
			ring->state = 1;
			break;

		case	CMD_DAB_OFF:
			if (ring->state == 0)
				break;
			cancel_work_sync (&ring->easy_work);
			ring->state = 0;
			break;
	}
	return 0;
}

static int easy_dab_read (struct file *file, char __user *buf, size_t count, loff_t *ptr)
{
	int count1 = 0;
	int write_pos;
	int read_pos;

	wait_for_completion (&ring->easy_dab_completion);
	init_completion(&ring->easy_dab_completion);
	
	mutex_lock(&ring->easy_dab_mutex);
	write_pos = ring->ring_tail;
	read_pos = ring->ring_head;
	mutex_unlock(&ring->easy_dab_mutex);

	count1 = write_pos - read_pos;
	if (count1 > 0)
	{
		count1 = (count1 > count) ? count : count1;
		if (copy_to_user (buf, (ring->ring_buf + read_pos), count1))
		{
			printk ("Copy data to userspace error !\n");
			return -EFAULT;
		}
		ring->ring_head = read_pos + count1;
	}
	else if (count1 < 0)
	{
		count1 = BUF_LENGTH - read_pos - 1;
		if (count1 != 0)
			if (copy_to_user (buf, (ring->ring_buf + read_pos), count1))
			{
				printk ("Copy data to userspace error !\n");
				return -EFAULT;
			}

		buf += count1;
		count1 += (write_pos + 1);
		if (copy_to_user (buf, ring->ring_buf, (write_pos + 1)))
		{
			printk ("Copy data to userspace error !\n");
			return -EFAULT;
		}

		ring->ring_head = write_pos;
	}

	return count1;
}

static void easy_get_data_work(struct work_struct *work)
{
	unsigned char *ptr;
	int retval = 0;
	ptr = ring->ring_buf;
	while (1)
	{
		retval = spi_write_then_read(easy_dab_spi, NULL, 0, ptr, SINGLE_LENGTH);
		if (retval < 0)
		{
			printk ("easy_dab get data error %d \n", retval);
			continue;
		}
		
		mutex_lock(&ring->easy_dab_mutex);
		ring->ring_tail = (ring->ring_tail + SINGLE_LENGTH) % BUF_LENGTH;
		ptr = ring->ring_buf + ring->ring_tail;
		mutex_unlock(&ring->easy_dab_mutex);
		complete(&ring->easy_dab_completion);
	}
}

static const struct file_operations easy_dab_ops = {
	.owner		= THIS_MODULE,
	.open		= easy_dab_open,
	.release	= easy_dab_close,
	.read		= easy_dab_read,
	.unlocked_ioctl	= easy_dab_ioctl,
};

static struct miscdevice easy_dab_miscdev = {
	MISC_DYNAMIC_MINOR,
	"easy_dab",
	&easy_dab_ops,
};

static struct spi_driver easy_dab_driver = {
	.driver = {
		.name	= "easy_dab",	
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe	= easy_dab_probe,
};

static int __devinit easy_dab_probe(struct spi_device *spi)
{
	ring = kzalloc (sizeof *ring, GFP_KERNEL); 
	if (!ring)
		return -ENOMEM;
	ring->ring_buf = kmalloc (BUF_LENGTH, GFP_KERNEL);
	if (!ring->ring_buf)
	{
		kfree (ring);
		return -ENOMEM;
	}	
	ring->ring_head = ring->ring_tail = 0;

	mutex_init(&ring->easy_dab_mutex);
	init_completion(&ring->easy_dab_completion);
	INIT_WORK(&ring->easy_work, easy_get_data_work);

	if (misc_register (&easy_dab_miscdev))
	{
		printk ("register easy_dab misc device error !\n");
		return -EBUSY;
	}
	easy_dab_spi = spi;
	return 0;
}


static int __init easy_dab_init(void)
{
	return spi_register_driver(&easy_dab_driver);
}

static void __exit easy_dab_exit(void)
{
	spi_unregister_driver(&easy_dab_driver);
}

module_init(easy_dab_init);
module_exit(easy_dab_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("linxiyuan");
MODULE_DESCRIPTION("loongson-gz for easynetting of spi-dab audio driver");
