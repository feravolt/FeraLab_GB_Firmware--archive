

#include <linux/module.h>	
#include <linux/init.h>		
#include <linux/ioport.h>	
#include <linux/version.h>      
#include <linux/videodev2.h>	
#include <linux/io.h>		
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

MODULE_AUTHOR("Dr. Henrik Seidel");
MODULE_DESCRIPTION("A driver for the Typhoon radio card (a.k.a. EcoRadio).");
MODULE_LICENSE("GPL");

#ifndef CONFIG_RADIO_TYPHOON_PORT
#define CONFIG_RADIO_TYPHOON_PORT -1
#endif

#ifndef CONFIG_RADIO_TYPHOON_MUTEFREQ
#define CONFIG_RADIO_TYPHOON_MUTEFREQ 0
#endif

static int io = CONFIG_RADIO_TYPHOON_PORT;
static int radio_nr = -1;

module_param(io, int, 0);
MODULE_PARM_DESC(io, "I/O address of the Typhoon card (0x316 or 0x336)");

module_param(radio_nr, int, 0);

static unsigned long mutefreq = CONFIG_RADIO_TYPHOON_MUTEFREQ;
module_param(mutefreq, ulong, 0);
MODULE_PARM_DESC(mutefreq, "Frequency used when muting the card (in kHz)");

#define RADIO_VERSION KERNEL_VERSION(0, 1, 1)

#define BANNER "Typhoon Radio Card driver v0.1.1\n"

struct typhoon {
	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	int io;
	int curvol;
	int muted;
	unsigned long curfreq;
	unsigned long mutefreq;
	struct mutex lock;
};

static struct typhoon typhoon_card;

static void typhoon_setvol_generic(struct typhoon *dev, int vol)
{
	mutex_lock(&dev->lock);
	vol >>= 14;				
	vol &= 3;
	outb_p(vol / 2, dev->io);		
	outb_p(vol % 2, dev->io + 2);	
	mutex_unlock(&dev->lock);
}

static int typhoon_setfreq_generic(struct typhoon *dev,
				   unsigned long frequency)
{
	unsigned long outval;
	unsigned long x;

	

	mutex_lock(&dev->lock);
	x = frequency / 160;
	outval = (x * x + 2500) / 5000;
	outval = (outval * x + 5000) / 10000;
	outval -= (10 * x * x + 10433) / 20866;
	outval += 4 * x - 11505;

	outb_p((outval >> 8) & 0x01, dev->io + 4);
	outb_p(outval >> 9, dev->io + 6);
	outb_p(outval & 0xff, dev->io + 8);
	mutex_unlock(&dev->lock);

	return 0;
}

static int typhoon_setfreq(struct typhoon *dev, unsigned long frequency)
{
	typhoon_setfreq_generic(dev, frequency);
	dev->curfreq = frequency;
	return 0;
}

static void typhoon_mute(struct typhoon *dev)
{
	if (dev->muted == 1)
		return;
	typhoon_setvol_generic(dev, 0);
	typhoon_setfreq_generic(dev, dev->mutefreq);
	dev->muted = 1;
}

static void typhoon_unmute(struct typhoon *dev)
{
	if (dev->muted == 0)
		return;
	typhoon_setfreq_generic(dev, dev->curfreq);
	typhoon_setvol_generic(dev, dev->curvol);
	dev->muted = 0;
}

static int typhoon_setvol(struct typhoon *dev, int vol)
{
	if (dev->muted && vol != 0) {	
		dev->curvol = vol;
		typhoon_unmute(dev);
		return 0;
	}
	if (vol == dev->curvol)		
		return 0;

	if (vol == 0) {			
		typhoon_mute(dev);
		dev->curvol = vol;
		return 0;
	}
	typhoon_setvol_generic(dev, vol);
	dev->curvol = vol;
	return 0;
}

static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *v)
{
	strlcpy(v->driver, "radio-typhoon", sizeof(v->driver));
	strlcpy(v->card, "Typhoon Radio", sizeof(v->card));
	strlcpy(v->bus_info, "ISA", sizeof(v->bus_info));
	v->version = RADIO_VERSION;
	v->capabilities = V4L2_CAP_TUNER | V4L2_CAP_RADIO;
	return 0;
}

static int vidioc_g_tuner(struct file *file, void *priv,
					struct v4l2_tuner *v)
{
	if (v->index > 0)
		return -EINVAL;

	strlcpy(v->name, "FM", sizeof(v->name));
	v->type = V4L2_TUNER_RADIO;
	v->rangelow = 87.5 * 16000;
	v->rangehigh = 108 * 16000;
	v->rxsubchans = V4L2_TUNER_SUB_MONO;
	v->capability = V4L2_TUNER_CAP_LOW;
	v->audmode = V4L2_TUNER_MODE_MONO;
	v->signal = 0xFFFF;     
	return 0;
}

static int vidioc_s_tuner(struct file *file, void *priv,
					struct v4l2_tuner *v)
{
	return v->index ? -EINVAL : 0;
}

static int vidioc_g_frequency(struct file *file, void *priv,
					struct v4l2_frequency *f)
{
	struct typhoon *dev = video_drvdata(file);

	f->type = V4L2_TUNER_RADIO;
	f->frequency = dev->curfreq;
	return 0;
}

static int vidioc_s_frequency(struct file *file, void *priv,
					struct v4l2_frequency *f)
{
	struct typhoon *dev = video_drvdata(file);

	dev->curfreq = f->frequency;
	typhoon_setfreq(dev, dev->curfreq);
	return 0;
}

static int vidioc_queryctrl(struct file *file, void *priv,
					struct v4l2_queryctrl *qc)
{
	switch (qc->id) {
	case V4L2_CID_AUDIO_MUTE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 1);
	case V4L2_CID_AUDIO_VOLUME:
		return v4l2_ctrl_query_fill(qc, 0, 65535, 16384, 65535);
	}
	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
					struct v4l2_control *ctrl)
{
	struct typhoon *dev = video_drvdata(file);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		ctrl->value = dev->muted;
		return 0;
	case V4L2_CID_AUDIO_VOLUME:
		ctrl->value = dev->curvol;
		return 0;
	}
	return -EINVAL;
}

static int vidioc_s_ctrl (struct file *file, void *priv,
					struct v4l2_control *ctrl)
{
	struct typhoon *dev = video_drvdata(file);

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		if (ctrl->value)
			typhoon_mute(dev);
		else
			typhoon_unmute(dev);
		return 0;
	case V4L2_CID_AUDIO_VOLUME:
		typhoon_setvol(dev, ctrl->value);
		return 0;
	}
	return -EINVAL;
}

static int vidioc_g_input(struct file *filp, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int vidioc_s_input(struct file *filp, void *priv, unsigned int i)
{
	return i ? -EINVAL : 0;
}

static int vidioc_g_audio(struct file *file, void *priv,
					struct v4l2_audio *a)
{
	a->index = 0;
	strlcpy(a->name, "Radio", sizeof(a->name));
	a->capability = V4L2_AUDCAP_STEREO;
	return 0;
}

static int vidioc_s_audio(struct file *file, void *priv,
					struct v4l2_audio *a)
{
	return a->index ? -EINVAL : 0;
}

static int vidioc_log_status(struct file *file, void *priv)
{
	struct typhoon *dev = video_drvdata(file);
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;

	v4l2_info(v4l2_dev, BANNER);
#ifdef MODULE
	v4l2_info(v4l2_dev, "Load type: Driver loaded as a module\n\n");
#else
	v4l2_info(v4l2_dev, "Load type: Driver compiled into kernel\n\n");
#endif
	v4l2_info(v4l2_dev, "frequency = %lu kHz\n", dev->curfreq >> 4);
	v4l2_info(v4l2_dev, "volume = %d\n", dev->curvol);
	v4l2_info(v4l2_dev, "mute = %s\n", dev->muted ?  "on" : "off");
	v4l2_info(v4l2_dev, "io = 0x%x\n", dev->io);
	v4l2_info(v4l2_dev, "mute frequency = %lu kHz\n", dev->mutefreq >> 4);
	return 0;
}

static const struct v4l2_file_operations typhoon_fops = {
	.owner		= THIS_MODULE,
	.ioctl		= video_ioctl2,
};

static const struct v4l2_ioctl_ops typhoon_ioctl_ops = {
	.vidioc_log_status  = vidioc_log_status,
	.vidioc_querycap    = vidioc_querycap,
	.vidioc_g_tuner     = vidioc_g_tuner,
	.vidioc_s_tuner     = vidioc_s_tuner,
	.vidioc_g_audio     = vidioc_g_audio,
	.vidioc_s_audio     = vidioc_s_audio,
	.vidioc_g_input     = vidioc_g_input,
	.vidioc_s_input     = vidioc_s_input,
	.vidioc_g_frequency = vidioc_g_frequency,
	.vidioc_s_frequency = vidioc_s_frequency,
	.vidioc_queryctrl   = vidioc_queryctrl,
	.vidioc_g_ctrl      = vidioc_g_ctrl,
	.vidioc_s_ctrl      = vidioc_s_ctrl,
};

static int __init typhoon_init(void)
{
	struct typhoon *dev = &typhoon_card;
	struct v4l2_device *v4l2_dev = &dev->v4l2_dev;
	int res;

	strlcpy(v4l2_dev->name, "typhoon", sizeof(v4l2_dev->name));
	dev->io = io;
	dev->curfreq = dev->mutefreq = mutefreq;

	if (dev->io == -1) {
		v4l2_err(v4l2_dev, "You must set an I/O address with io=0x316 or io=0x336\n");
		return -EINVAL;
	}

	if (dev->mutefreq < 87000 || dev->mutefreq > 108500) {
		v4l2_err(v4l2_dev, "You must set a frequency (in kHz) used when muting the card,\n");
		v4l2_err(v4l2_dev, "e.g. with \"mutefreq=87500\" (87000 <= mutefreq <= 108500)\n");
		return -EINVAL;
	}

	mutex_init(&dev->lock);
	if (!request_region(dev->io, 8, "typhoon")) {
		v4l2_err(v4l2_dev, "port 0x%x already in use\n",
		       dev->io);
		return -EBUSY;
	}

	res = v4l2_device_register(NULL, v4l2_dev);
	if (res < 0) {
		release_region(dev->io, 8);
		v4l2_err(v4l2_dev, "Could not register v4l2_device\n");
		return res;
	}
	v4l2_info(v4l2_dev, BANNER);

	strlcpy(dev->vdev.name, v4l2_dev->name, sizeof(dev->vdev.name));
	dev->vdev.v4l2_dev = v4l2_dev;
	dev->vdev.fops = &typhoon_fops;
	dev->vdev.ioctl_ops = &typhoon_ioctl_ops;
	dev->vdev.release = video_device_release_empty;
	video_set_drvdata(&dev->vdev, dev);
	if (video_register_device(&dev->vdev, VFL_TYPE_RADIO, radio_nr) < 0) {
		v4l2_device_unregister(&dev->v4l2_dev);
		release_region(dev->io, 8);
		return -EINVAL;
	}
	v4l2_info(v4l2_dev, "port 0x%x.\n", dev->io);
	v4l2_info(v4l2_dev, "mute frequency is %lu kHz.\n", dev->mutefreq);
	dev->mutefreq <<= 4;

	
	typhoon_mute(dev);

	return 0;
}

static void __exit typhoon_exit(void)
{
	struct typhoon *dev = &typhoon_card;

	video_unregister_device(&dev->vdev);
	v4l2_device_unregister(&dev->v4l2_dev);
	release_region(dev->io, 8);
}

module_init(typhoon_init);
module_exit(typhoon_exit);

