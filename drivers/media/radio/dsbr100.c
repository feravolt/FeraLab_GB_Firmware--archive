

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <linux/usb.h>


#include <linux/version.h>	

#define DRIVER_VERSION "v0.46"
#define RADIO_VERSION KERNEL_VERSION(0, 4, 6)

#define DRIVER_AUTHOR "Markus Demleitner <msdemlei@tucana.harvard.edu>"
#define DRIVER_DESC "D-Link DSB-R100 USB FM radio driver"

#define DSB100_VENDOR 0x04b4
#define DSB100_PRODUCT 0x1002


#define DSB100_TUNE 1
#define DSB100_ONOFF 2

#define TB_LEN 16


#define FREQ_MIN  87.5
#define FREQ_MAX 108.0
#define FREQ_MUL 16000


#define STARTED	0
#define STOPPED	1

#define videodev_to_radio(d) container_of(d, struct dsbr100_device, videodev)

static int usb_dsbr100_probe(struct usb_interface *intf,
			     const struct usb_device_id *id);
static void usb_dsbr100_disconnect(struct usb_interface *intf);
static int usb_dsbr100_suspend(struct usb_interface *intf,
						pm_message_t message);
static int usb_dsbr100_resume(struct usb_interface *intf);

static int radio_nr = -1;
module_param(radio_nr, int, 0);


struct dsbr100_device {
	struct usb_device *usbdev;
	struct video_device videodev;
	struct v4l2_device v4l2_dev;

	u8 *transfer_buffer;
	struct mutex lock;	
	int curfreq;
	int stereo;
	int removed;
	int status;
};

static struct usb_device_id usb_dsbr100_device_table [] = {
	{ USB_DEVICE(DSB100_VENDOR, DSB100_PRODUCT) },
	{ }						
};

MODULE_DEVICE_TABLE (usb, usb_dsbr100_device_table);


static struct usb_driver usb_dsbr100_driver = {
	.name			= "dsbr100",
	.probe			= usb_dsbr100_probe,
	.disconnect		= usb_dsbr100_disconnect,
	.id_table		= usb_dsbr100_device_table,
	.suspend		= usb_dsbr100_suspend,
	.resume			= usb_dsbr100_resume,
	.reset_resume		= usb_dsbr100_resume,
	.supports_autosuspend	= 0,
};




static int dsbr100_start(struct dsbr100_device *radio)
{
	int retval;
	int request;

	mutex_lock(&radio->lock);

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		USB_REQ_GET_STATUS,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
		0x00, 0xC7, radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		request = USB_REQ_GET_STATUS;
		goto usb_control_msg_failed;
	}

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		DSB100_ONOFF,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
		0x01, 0x00, radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		request = DSB100_ONOFF;
		goto usb_control_msg_failed;
	}

	radio->status = STARTED;
	mutex_unlock(&radio->lock);
	return (radio->transfer_buffer)[0];

usb_control_msg_failed:
	mutex_unlock(&radio->lock);
	dev_err(&radio->usbdev->dev,
		"%s - usb_control_msg returned %i, request %i\n",
			__func__, retval, request);
	return retval;

}


static int dsbr100_stop(struct dsbr100_device *radio)
{
	int retval;
	int request;

	mutex_lock(&radio->lock);

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		USB_REQ_GET_STATUS,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
		0x16, 0x1C, radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		request = USB_REQ_GET_STATUS;
		goto usb_control_msg_failed;
	}

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		DSB100_ONOFF,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
		0x00, 0x00, radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		request = DSB100_ONOFF;
		goto usb_control_msg_failed;
	}

	radio->status = STOPPED;
	mutex_unlock(&radio->lock);
	return (radio->transfer_buffer)[0];

usb_control_msg_failed:
	mutex_unlock(&radio->lock);
	dev_err(&radio->usbdev->dev,
		"%s - usb_control_msg returned %i, request %i\n",
			__func__, retval, request);
	return retval;

}


static int dsbr100_setfreq(struct dsbr100_device *radio)
{
	int retval;
	int request;
	int freq = (radio->curfreq / 16 * 80) / 1000 + 856;

	mutex_lock(&radio->lock);

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		DSB100_TUNE,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
		(freq >> 8) & 0x00ff, freq & 0xff,
		radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		request = DSB100_TUNE;
		goto usb_control_msg_failed;
	}

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		USB_REQ_GET_STATUS,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
		0x96, 0xB7, radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		request = USB_REQ_GET_STATUS;
		goto usb_control_msg_failed;
	}

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		USB_REQ_GET_STATUS,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE |  USB_DIR_IN,
		0x00, 0x24, radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		request = USB_REQ_GET_STATUS;
		goto usb_control_msg_failed;
	}

	radio->stereo = !((radio->transfer_buffer)[0] & 0x01);
	mutex_unlock(&radio->lock);
	return (radio->transfer_buffer)[0];

usb_control_msg_failed:
	radio->stereo = -1;
	mutex_unlock(&radio->lock);
	dev_err(&radio->usbdev->dev,
		"%s - usb_control_msg returned %i, request %i\n",
			__func__, retval, request);
	return retval;
}


static void dsbr100_getstat(struct dsbr100_device *radio)
{
	int retval;

	mutex_lock(&radio->lock);

	retval = usb_control_msg(radio->usbdev,
		usb_rcvctrlpipe(radio->usbdev, 0),
		USB_REQ_GET_STATUS,
		USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_DIR_IN,
		0x00 , 0x24, radio->transfer_buffer, 8, 300);

	if (retval < 0) {
		radio->stereo = -1;
		dev_err(&radio->usbdev->dev,
			"%s - usb_control_msg returned %i, request %i\n",
				__func__, retval, USB_REQ_GET_STATUS);
	} else {
		radio->stereo = !(radio->transfer_buffer[0] & 0x01);
	}

	mutex_unlock(&radio->lock);
}




static void usb_dsbr100_disconnect(struct usb_interface *intf)
{
	struct dsbr100_device *radio = usb_get_intfdata(intf);

	usb_set_intfdata (intf, NULL);

	mutex_lock(&radio->lock);
	radio->removed = 1;
	mutex_unlock(&radio->lock);

	video_unregister_device(&radio->videodev);
	v4l2_device_disconnect(&radio->v4l2_dev);
}


static int vidioc_querycap(struct file *file, void *priv,
					struct v4l2_capability *v)
{
	struct dsbr100_device *radio = video_drvdata(file);

	strlcpy(v->driver, "dsbr100", sizeof(v->driver));
	strlcpy(v->card, "D-Link R-100 USB FM Radio", sizeof(v->card));
	usb_make_path(radio->usbdev, v->bus_info, sizeof(v->bus_info));
	v->version = RADIO_VERSION;
	v->capabilities = V4L2_CAP_TUNER;
	return 0;
}

static int vidioc_g_tuner(struct file *file, void *priv,
				struct v4l2_tuner *v)
{
	struct dsbr100_device *radio = video_drvdata(file);

	
	if (radio->removed)
		return -EIO;

	if (v->index > 0)
		return -EINVAL;

	dsbr100_getstat(radio);
	strcpy(v->name, "FM");
	v->type = V4L2_TUNER_RADIO;
	v->rangelow = FREQ_MIN * FREQ_MUL;
	v->rangehigh = FREQ_MAX * FREQ_MUL;
	v->rxsubchans = V4L2_TUNER_SUB_MONO | V4L2_TUNER_SUB_STEREO;
	v->capability = V4L2_TUNER_CAP_LOW;
	if(radio->stereo)
		v->audmode = V4L2_TUNER_MODE_STEREO;
	else
		v->audmode = V4L2_TUNER_MODE_MONO;
	v->signal = 0xffff;     
	return 0;
}

static int vidioc_s_tuner(struct file *file, void *priv,
				struct v4l2_tuner *v)
{
	struct dsbr100_device *radio = video_drvdata(file);

	
	if (radio->removed)
		return -EIO;

	if (v->index > 0)
		return -EINVAL;

	return 0;
}

static int vidioc_s_frequency(struct file *file, void *priv,
				struct v4l2_frequency *f)
{
	struct dsbr100_device *radio = video_drvdata(file);
	int retval;

	
	if (radio->removed)
		return -EIO;

	mutex_lock(&radio->lock);
	radio->curfreq = f->frequency;
	mutex_unlock(&radio->lock);

	retval = dsbr100_setfreq(radio);
	if (retval < 0)
		dev_warn(&radio->usbdev->dev, "Set frequency failed\n");
	return 0;
}

static int vidioc_g_frequency(struct file *file, void *priv,
				struct v4l2_frequency *f)
{
	struct dsbr100_device *radio = video_drvdata(file);

	
	if (radio->removed)
		return -EIO;

	f->type = V4L2_TUNER_RADIO;
	f->frequency = radio->curfreq;
	return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
				struct v4l2_queryctrl *qc)
{
	switch (qc->id) {
	case V4L2_CID_AUDIO_MUTE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct dsbr100_device *radio = video_drvdata(file);

	
	if (radio->removed)
		return -EIO;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = radio->status;
		return 0;
	}
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
				struct v4l2_control *ctrl)
{
	struct dsbr100_device *radio = video_drvdata(file);
	int retval;

	
	if (radio->removed)
		return -EIO;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		if (ctrl->value) {
			retval = dsbr100_stop(radio);
			if (retval < 0) {
				dev_warn(&radio->usbdev->dev,
					 "Radio did not respond properly\n");
				return -EBUSY;
			}
		} else {
			retval = dsbr100_start(radio);
			if (retval < 0) {
				dev_warn(&radio->usbdev->dev,
					 "Radio did not respond properly\n");
				return -EBUSY;
			}
		}
		return 0;
	}
	return -EINVAL;
}

static int vidioc_g_audio(struct file *file, void *priv,
				struct v4l2_audio *a)
{
	if (a->index > 1)
		return -EINVAL;

	strcpy(a->name, "Radio");
	a->capability = V4L2_AUDCAP_STEREO;
	return 0;
}

static int vidioc_g_input(struct file *filp, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int vidioc_s_input(struct file *filp, void *priv, unsigned int i)
{
	if (i != 0)
		return -EINVAL;
	return 0;
}

static int vidioc_s_audio(struct file *file, void *priv,
					struct v4l2_audio *a)
{
	if (a->index != 0)
		return -EINVAL;
	return 0;
}


static int usb_dsbr100_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct dsbr100_device *radio = usb_get_intfdata(intf);
	int retval;

	if (radio->status == STARTED) {
		retval = dsbr100_stop(radio);
		if (retval < 0)
			dev_warn(&intf->dev, "dsbr100_stop failed\n");

		

		mutex_lock(&radio->lock);
		radio->status = STARTED;
		mutex_unlock(&radio->lock);
	}

	dev_info(&intf->dev, "going into suspend..\n");

	return 0;
}


static int usb_dsbr100_resume(struct usb_interface *intf)
{
	struct dsbr100_device *radio = usb_get_intfdata(intf);
	int retval;

	if (radio->status == STARTED) {
		retval = dsbr100_start(radio);
		if (retval < 0)
			dev_warn(&intf->dev, "dsbr100_start failed\n");
	}

	dev_info(&intf->dev, "coming out of suspend..\n");

	return 0;
}


static void usb_dsbr100_video_device_release(struct video_device *videodev)
{
	struct dsbr100_device *radio = videodev_to_radio(videodev);

	v4l2_device_unregister(&radio->v4l2_dev);
	kfree(radio->transfer_buffer);
	kfree(radio);
}


static const struct v4l2_file_operations usb_dsbr100_fops = {
	.owner		= THIS_MODULE,
	.ioctl		= video_ioctl2,
};

static const struct v4l2_ioctl_ops usb_dsbr100_ioctl_ops = {
	.vidioc_querycap    = vidioc_querycap,
	.vidioc_g_tuner     = vidioc_g_tuner,
	.vidioc_s_tuner     = vidioc_s_tuner,
	.vidioc_g_frequency = vidioc_g_frequency,
	.vidioc_s_frequency = vidioc_s_frequency,
	.vidioc_queryctrl   = vidioc_queryctrl,
	.vidioc_g_ctrl      = vidioc_g_ctrl,
	.vidioc_s_ctrl      = vidioc_s_ctrl,
	.vidioc_g_audio     = vidioc_g_audio,
	.vidioc_s_audio     = vidioc_s_audio,
	.vidioc_g_input     = vidioc_g_input,
	.vidioc_s_input     = vidioc_s_input,
};


static int usb_dsbr100_probe(struct usb_interface *intf,
				const struct usb_device_id *id)
{
	struct dsbr100_device *radio;
	struct v4l2_device *v4l2_dev;
	int retval;

	radio = kzalloc(sizeof(struct dsbr100_device), GFP_KERNEL);

	if (!radio)
		return -ENOMEM;

	radio->transfer_buffer = kmalloc(TB_LEN, GFP_KERNEL);

	if (!(radio->transfer_buffer)) {
		kfree(radio);
		return -ENOMEM;
	}

	v4l2_dev = &radio->v4l2_dev;

	retval = v4l2_device_register(&intf->dev, v4l2_dev);
	if (retval < 0) {
		v4l2_err(v4l2_dev, "couldn't register v4l2_device\n");
		kfree(radio->transfer_buffer);
		kfree(radio);
		return retval;
	}

	strlcpy(radio->videodev.name, v4l2_dev->name, sizeof(radio->videodev.name));
	radio->videodev.v4l2_dev = v4l2_dev;
	radio->videodev.fops = &usb_dsbr100_fops;
	radio->videodev.ioctl_ops = &usb_dsbr100_ioctl_ops;
	radio->videodev.release = usb_dsbr100_video_device_release;

	mutex_init(&radio->lock);

	radio->removed = 0;
	radio->usbdev = interface_to_usbdev(intf);
	radio->curfreq = FREQ_MIN * FREQ_MUL;
	radio->status = STOPPED;

	video_set_drvdata(&radio->videodev, radio);

	retval = video_register_device(&radio->videodev, VFL_TYPE_RADIO, radio_nr);
	if (retval < 0) {
		v4l2_err(v4l2_dev, "couldn't register video device\n");
		v4l2_device_unregister(v4l2_dev);
		kfree(radio->transfer_buffer);
		kfree(radio);
		return -EIO;
	}
	usb_set_intfdata(intf, radio);
	return 0;
}

static int __init dsbr100_init(void)
{
	int retval = usb_register(&usb_dsbr100_driver);
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");
	return retval;
}

static void __exit dsbr100_exit(void)
{
	usb_deregister(&usb_dsbr100_driver);
}

module_init (dsbr100_init);
module_exit (dsbr100_exit);

MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");
