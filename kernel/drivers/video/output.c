#include <linux/module.h>
#include <linux/video_output.h>
#include <linux/err.h>
#include <linux/ctype.h>

MODULE_DESCRIPTION("Display Output Switcher Lowlevel Control Abstraction");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luming Yu <luming.yu@intel.com>");

static ssize_t video_output_show_state(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	ssize_t ret_size = 0;
	struct output_device *od = to_output_device(dev);
	if (od->props)
		ret_size = sprintf(buf,"%.8x\n",od->props->get_status(od));
	return ret_size;
}

static ssize_t video_output_store_state(struct device *dev,
					struct device_attribute *attr,
					const char *buf,size_t count)
{
	char *endp;
	struct output_device *od = to_output_device(dev);
	int request_state = simple_strtoul(buf,&endp,0);
	size_t size = endp - buf;

	if (*endp && isspace(*endp))
		size++;
	if (size != count)
		return -EINVAL;

	if (od->props) {
		od->request_state = request_state;
		od->props->set_state(od);
	}
	return count;
}

static void video_output_release(struct device *dev)
{
	struct output_device *od = to_output_device(dev);
	kfree(od);
}

static struct device_attribute video_output_attributes[] = {
	__ATTR(state, 0644, video_output_show_state, video_output_store_state),
	__ATTR_NULL,
};


static struct class video_output_class = {
	.name = "video_output",
	.dev_release = video_output_release,
	.dev_attrs = video_output_attributes,
};

struct output_device *video_output_register(const char *name,
	struct device *dev,
	void *devdata,
	struct output_properties *op)
{
	struct output_device *new_dev;
	int ret_code = 0;

	new_dev = kzalloc(sizeof(struct output_device),GFP_KERNEL);
	if (!new_dev) {
		ret_code = -ENOMEM;
		goto error_return;
	}
	new_dev->props = op;
	new_dev->dev.class = &video_output_class;
	new_dev->dev.parent = dev;
	dev_set_name(&new_dev->dev, "%s", name);
	dev_set_drvdata(&new_dev->dev, devdata);
	ret_code = device_register(&new_dev->dev);
	if (ret_code) {
		kfree(new_dev);
		goto error_return;
	}
	return new_dev;

error_return:
	return ERR_PTR(ret_code);
}
EXPORT_SYMBOL(video_output_register);

void video_output_unregister(struct output_device *dev)
{
	if (!dev)
		return;
	device_unregister(&dev->dev);
}
EXPORT_SYMBOL(video_output_unregister);

static void __exit video_output_class_exit(void)
{
	class_unregister(&video_output_class);
}

static int __init video_output_class_init(void)
{
	return class_register(&video_output_class);
}

postcore_initcall(video_output_class_init);
module_exit(video_output_class_exit);
