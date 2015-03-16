#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/gpio.h>
#include <linux/ctype.h>
#include <linux/wakelock.h>
#include <linux/es209ra_proximity.h>

static unsigned long Proximity_BurstDuration;
struct wake_lock proximity_wakelock;

static void Proximity_initialize(void);
static void Proximity_initialize()
{
	gpio_set_value(PROXIMITY_GPIO_LEDON_PIN,PROXIMITY_GPIO_LEDON_DISABLE);
	gpio_set_value(PROXIMITY_GPIO_ENBAR_PIN,PROXIMITY_GPIO_ENBAR_DISABLE);
	gpio_set_value(PROXIMITY_GPIO_ENBAR_PIN,PROXIMITY_GPIO_POWER_OFF);
}

static int Proximity_open(struct inode *inode, struct file *file)
{
	Proximity_initialize();
	return 0;
}

/* release command for PROXIMITY device file */
static int Proximity_close(struct inode *inode, struct file *file)
{
	Proximity_initialize();
	return 0;
}

static ssize_t Proximity_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	return 0;
}

static ssize_t Proximity_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	return 0;
}

static void Proximity_setlock(int lock)
{
	if (lock)
		wake_lock(&proximity_wakelock);
	else
		wake_unlock(&proximity_wakelock);
}

static int Proximity_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	unsigned int data = 0;
	unsigned long time = 0;

	if(_IOC_TYPE(cmd) != PROXIMITY_IOC_MAGIC)
	{
		return -ENOTTY;
	}
	if(_IOC_NR(cmd) > PROXIMITY_IOC_MAXNR)
	{
		return -ENOTTY;
	}

	if(_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,(void __user*)arg, _IOC_SIZE(cmd));
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
	if(err)
	{
		return -EFAULT;
	}

	switch(cmd)
	{
	case PROXIMITY_SET_POWER_STATE:
		if(copy_from_user((unsigned char*)&data, (unsigned char*)arg, 1)!=0)
		{
			return -EFAULT;
		}
		gpio_set_value(PROXIMITY_GPIO_POWER_PIN,
				(data==PROXIMITY_GPIO_POWER_ON)?PROXIMITY_GPIO_POWER_ON:PROXIMITY_GPIO_POWER_OFF);
		return err;

	case PROXIMITY_GET_POWER_STATE:
		data = gpio_get_value(PROXIMITY_GPIO_POWER_PIN);
		if(copy_to_user((unsigned char*)arg, (unsigned char*)&data, 1)!=0)
		{
			return -EFAULT;
		}
		return err;

	case PROXIMITY_SET_DEVICE_MODE:
		if(copy_from_user((unsigned char*)&data, (unsigned char*)arg, 1)!=0)
		{
			return -EFAULT;
		}
		gpio_set_value(PROXIMITY_GPIO_ENBAR_PIN,
				(data==PROXIMITY_GPIO_ENBAR_ENABLE)?PROXIMITY_GPIO_ENBAR_ENABLE:PROXIMITY_GPIO_ENBAR_DISABLE);
		return err;

	case PROXIMITY_GET_DEVICE_MODE:
		data = gpio_get_value(PROXIMITY_GPIO_ENBAR_PIN);
		if(copy_to_user((unsigned char*)arg, (unsigned char*)&data, 1)!=0)
		{
			return -EFAULT;
		}
		return err;

	case PROXIMITY_SET_LED_MODE:
		if(copy_from_user((unsigned char*)&data, (unsigned char*)arg, 1)!=0)
		{
			return -EFAULT;
		}
		gpio_set_value(PROXIMITY_GPIO_LEDON_PIN,
				(data==PROXIMITY_GPIO_LEDON_ENABLE)?PROXIMITY_GPIO_LEDON_ENABLE:PROXIMITY_GPIO_LEDON_DISABLE);
		return err;

	case PROXIMITY_GET_LED_MODE:
		data = gpio_get_value(PROXIMITY_GPIO_LEDON_PIN);
		if(copy_to_user((unsigned char*)arg, (unsigned char*)&data, 1)!=0)
		{
			return -EFAULT;
		}
		return err;

	case PROXIMITY_GET_DETECTION_STATE:
		data = gpio_get_value(PROXIMITY_GPIO_DOUT_PIN);
		if(copy_to_user((unsigned char*)arg, (unsigned char*)&data, 1)!=0)
		{
			return -EFAULT;
		}
		return err;

	case PROXIMITY_SET_BURST_ON_TIME:
		if(copy_from_user((unsigned long*)&time, (unsigned long*)arg, sizeof(arg))!=0)
		{
			return -EFAULT;
		}
		if((time < PROXIMITY_BURST_ON_TIME_MIN) || (time > PROXIMITY_BURST_ON_TIME_MAX)) {
			return -EFAULT;
		}
		Proximity_BurstDuration = time;
		return err;

	case PROXIMITY_GET_BURST_ON_TIME:
		if(copy_to_user((unsigned long*)arg, (unsigned long*)&Proximity_BurstDuration, sizeof(arg))!=0)
		{
			return -EFAULT;
		}
		return err;

	case PROXIMITY_DO_SENSING:
		gpio_set_value(PROXIMITY_GPIO_POWER_PIN,PROXIMITY_GPIO_POWER_ON);
		udelay(90);
		gpio_set_value(PROXIMITY_GPIO_ENBAR_PIN,PROXIMITY_GPIO_ENBAR_ENABLE);
		udelay(18);
		gpio_set_value(PROXIMITY_GPIO_LEDON_PIN,PROXIMITY_GPIO_LEDON_ENABLE);
		udelay(Proximity_BurstDuration);
		data = gpio_get_value(PROXIMITY_GPIO_DOUT_PIN);
		gpio_set_value(PROXIMITY_GPIO_LEDON_PIN,PROXIMITY_GPIO_LEDON_DISABLE);
		gpio_set_value(PROXIMITY_GPIO_ENBAR_PIN,PROXIMITY_GPIO_ENBAR_DISABLE);
		gpio_set_value(PROXIMITY_GPIO_POWER_PIN,PROXIMITY_GPIO_POWER_OFF);
		if(copy_to_user((unsigned char*)arg, (unsigned char*)&data, 1)!=0)
		{
			return -EFAULT;
		}
		return err;

	case PROXIMITY_SET_LOCK:
		Proximity_setlock(true);
		return err;

	case PROXIMITY_SET_UNLOCK:
		Proximity_setlock(false);
		return err;

	default:
		return 0;
	}
}

static const struct file_operations Proximity_fops = {
	.owner = THIS_MODULE,
	.open = Proximity_open,
	.release = Proximity_close,
	.read = Proximity_read,
	.write = Proximity_write,
	.ioctl = Proximity_ioctl,
};

static struct miscdevice Proximity_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "es209ra_proximity",
	.fops = &Proximity_fops,
};

static int __init Proximity_init(void)
{
	int res;

	res = misc_register(&Proximity_device);
	if (res) {
		printk(KERN_ERR "PROXIMITY: Couldn't misc_register, error=%d\n", res);
		goto err;
	}

	res = gpio_request(PROXIMITY_GPIO_POWER_PIN, "POWER");
	if (res) {
		printk(KERN_ERR
		       "PROXIMITY: Couldn't gpio_request(POWER), error=%d\n", res);
		goto err_request_power;
	}
	res = gpio_request(PROXIMITY_GPIO_ENBAR_PIN, "ENBAR");
	if (res) {
		printk(KERN_ERR
		       "PROXIMITY: Couldn't gpio_request(ENBAR), error=%d\n", res);
		goto err_request_enbar;
	}
	res = gpio_request(PROXIMITY_GPIO_LEDON_PIN, "LEDON");
	if (res) {
		printk(KERN_ERR
		       "PROXIMITY: Couldn't gpio_request(LEDON), error=%d\n", res);
		goto err_request_ledon;
	}
	res = gpio_request(PROXIMITY_GPIO_DOUT_PIN, "DOUT");
	if (res) {
		printk(KERN_ERR
		       "PROXIMITY: Couldn't gpio_request(DOUT), error=%d\n", res);
		goto err_request_dout;
	}

	gpio_direction_output(PROXIMITY_GPIO_POWER_PIN, PROXIMITY_GPIO_POWER_OFF);
	gpio_direction_output(PROXIMITY_GPIO_ENBAR_PIN, PROXIMITY_GPIO_ENBAR_DISABLE);
	gpio_direction_output(PROXIMITY_GPIO_LEDON_PIN, PROXIMITY_GPIO_LEDON_DISABLE);
	gpio_direction_input(PROXIMITY_GPIO_DOUT_PIN);
	Proximity_BurstDuration = PROXIMITY_BURST_ON_TIME_DEFAULT;
	wake_lock_init(&proximity_wakelock, WAKE_LOCK_SUSPEND, "proximitylock");
	printk(KERN_INFO "PROXIMITY driver installation succeeded\n");
	return 0;


err_request_dout:
	gpio_free(PROXIMITY_GPIO_LEDON_PIN);
err_request_ledon:
	gpio_free(PROXIMITY_GPIO_ENBAR_PIN);
err_request_enbar:
	gpio_free(PROXIMITY_GPIO_POWER_PIN);
err_request_power:
	misc_deregister(&Proximity_device);
err:
	printk(KERN_ERR "%s: PROXIMITY driver installation failed\n", __FILE__);
	return res;
}

static void __exit Proximity_exit(void)
{
	gpio_free(PROXIMITY_GPIO_POWER_PIN);
	gpio_free(PROXIMITY_GPIO_ENBAR_PIN);
	gpio_free(PROXIMITY_GPIO_LEDON_PIN);
	gpio_free(PROXIMITY_GPIO_DOUT_PIN);
	misc_deregister(&Proximity_device);
	wake_lock_destroy(&proximity_wakelock);
	printk(KERN_INFO "PROXIMITY driver removed\n");
}

MODULE_AUTHOR("SEMC");
MODULE_DESCRIPTION("Proximity sensor driver");
MODULE_LICENSE("GPL");
module_init(Proximity_init);
module_exit(Proximity_exit);

