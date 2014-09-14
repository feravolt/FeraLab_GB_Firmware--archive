#ifndef LINUX_BMA150_MODULE_H
#define LINUX_BMA150_MODULE_H

struct bma150_platform_data {
	int (*setup)(struct device *);
	void (*teardown)(struct device *);
};

#endif
