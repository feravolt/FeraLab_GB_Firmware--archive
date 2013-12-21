






#define DRIVER_AUTHOR "Joonyoung Shim <jy0922.shim@samsung.com>";
#define DRIVER_KERNEL_VERSION KERNEL_VERSION(1, 0, 0)
#define DRIVER_CARD "Silicon Labs Si470x FM Radio Receiver"
#define DRIVER_DESC "I2C radio driver for Si470x FM Radio Receivers"
#define DRIVER_VERSION "1.0.0"


#include <linux/i2c.h>
#include <linux/delay.h>

#include "radio-si470x.h"



static const struct i2c_device_id si470x_i2c_id[] = {
	
	{ "si470x", 0 },
	
	{ }
};
MODULE_DEVICE_TABLE(i2c, si470x_i2c_id);






static int radio_nr = -1;
module_param(radio_nr, int, 0444);
MODULE_PARM_DESC(radio_nr, "Radio Nr");






#define WRITE_REG_NUM		8
#define WRITE_INDEX(i)		(i + 0x02)


#define READ_REG_NUM		RADIO_REGISTER_NUM
#define READ_INDEX(i)		((i + RADIO_REGISTER_NUM - 0x0a) % READ_REG_NUM)






int si470x_get_register(struct si470x_device *radio, int regnr)
{
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
			(void *)buf },
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	radio->registers[regnr] = __be16_to_cpu(buf[READ_INDEX(regnr)]);

	return 0;
}



int si470x_set_register(struct si470x_device *radio, int regnr)
{
	int i;
	u16 buf[WRITE_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, 0, sizeof(u16) * WRITE_REG_NUM,
			(void *)buf },
	};

	for (i = 0; i < WRITE_REG_NUM; i++)
		buf[i] = __cpu_to_be16(radio->registers[WRITE_INDEX(i)]);

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	return 0;
}






static int si470x_get_all_registers(struct si470x_device *radio)
{
	int i;
	u16 buf[READ_REG_NUM];
	struct i2c_msg msgs[1] = {
		{ radio->client->addr, I2C_M_RD, sizeof(u16) * READ_REG_NUM,
			(void *)buf },
	};

	if (i2c_transfer(radio->client->adapter, msgs, 1) != 1)
		return -EIO;

	for (i = 0; i < READ_REG_NUM; i++)
		radio->registers[i] = __be16_to_cpu(buf[READ_INDEX(i)]);

	return 0;
}






int si470x_disconnect_check(struct si470x_device *radio)
{
	return 0;
}






static int si470x_fops_open(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	mutex_lock(&radio->lock);
	radio->users++;

	if (radio->users == 1)
		
		retval = si470x_start(radio);

	mutex_unlock(&radio->lock);

	return retval;
}



static int si470x_fops_release(struct file *file)
{
	struct si470x_device *radio = video_drvdata(file);
	int retval = 0;

	
	if (!radio)
		return -ENODEV;

	mutex_lock(&radio->lock);
	radio->users--;
	if (radio->users == 0)
		
		retval = si470x_stop(radio);

	mutex_unlock(&radio->lock);

	return retval;
}



const struct v4l2_file_operations si470x_fops = {
	.owner		= THIS_MODULE,
	.ioctl		= video_ioctl2,
	.open		= si470x_fops_open,
	.release	= si470x_fops_release,
};






int si470x_vidioc_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	strlcpy(capability->driver, DRIVER_NAME, sizeof(capability->driver));
	strlcpy(capability->card, DRIVER_CARD, sizeof(capability->card));
	capability->version = DRIVER_KERNEL_VERSION;
	capability->capabilities = V4L2_CAP_HW_FREQ_SEEK |
		V4L2_CAP_TUNER | V4L2_CAP_RADIO;

	return 0;
}






static int __devinit si470x_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct si470x_device *radio;
	int retval = 0;
	unsigned char version_warning = 0;

	
	radio = kzalloc(sizeof(struct si470x_device), GFP_KERNEL);
	if (!radio) {
		retval = -ENOMEM;
		goto err_initial;
	}
	radio->users = 0;
	radio->client = client;
	mutex_init(&radio->lock);

	
	radio->videodev = video_device_alloc();
	if (!radio->videodev) {
		retval = -ENOMEM;
		goto err_radio;
	}
	memcpy(radio->videodev, &si470x_viddev_template,
			sizeof(si470x_viddev_template));
	video_set_drvdata(radio->videodev, radio);

	
	radio->registers[POWERCFG] = POWERCFG_ENABLE;
	if (si470x_set_register(radio, POWERCFG) < 0) {
		retval = -EIO;
		goto err_all;
	}
	msleep(110);

	
	if (si470x_get_all_registers(radio) < 0) {
		retval = -EIO;
		goto err_video;
	}
	dev_info(&client->dev, "DeviceID=0x%4.4hx ChipID=0x%4.4hx\n",
			radio->registers[DEVICEID], radio->registers[CHIPID]);
	if ((radio->registers[CHIPID] & CHIPID_FIRMWARE) < RADIO_FW_VERSION) {
		dev_warn(&client->dev,
			"This driver is known to work with "
			"firmware version %hu,\n", RADIO_FW_VERSION);
		dev_warn(&client->dev,
			"but the device has firmware version %hu.\n",
			radio->registers[CHIPID] & CHIPID_FIRMWARE);
		version_warning = 1;
	}

	
	if (version_warning == 1) {
		dev_warn(&client->dev,
			"If you have some trouble using this driver,\n");
		dev_warn(&client->dev,
			"please report to V4L ML at "
			"linux-media@vger.kernel.org\n");
	}

	
	si470x_set_freq(radio, 87.5 * FREQ_MUL); 

	
	retval = video_register_device(radio->videodev, VFL_TYPE_RADIO,
			radio_nr);
	if (retval) {
		dev_warn(&client->dev, "Could not register video device\n");
		goto err_all;
	}
	i2c_set_clientdata(client, radio);

	return 0;
err_all:
err_video:
	video_device_release(radio->videodev);
err_radio:
	kfree(radio);
err_initial:
	return retval;
}



static __devexit int si470x_i2c_remove(struct i2c_client *client)
{
	struct si470x_device *radio = i2c_get_clientdata(client);

	video_unregister_device(radio->videodev);
	kfree(radio);
	i2c_set_clientdata(client, NULL);

	return 0;
}



static struct i2c_driver si470x_i2c_driver = {
	.driver = {
		.name		= "si470x",
		.owner		= THIS_MODULE,
	},
	.probe			= si470x_i2c_probe,
	.remove			= __devexit_p(si470x_i2c_remove),
	.id_table		= si470x_i2c_id,
};






static int __init si470x_i2c_init(void)
{
	printk(KERN_INFO DRIVER_DESC ", Version " DRIVER_VERSION "\n");
	return i2c_add_driver(&si470x_i2c_driver);
}



static void __exit si470x_i2c_exit(void)
{
	i2c_del_driver(&si470x_i2c_driver);
}


module_init(si470x_i2c_init);
module_exit(si470x_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
