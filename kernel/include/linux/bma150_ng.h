#ifndef __LINUX_BMA150_NG_H
#define __LINUX_BMA150_NG_H

struct bma150_platform_data {
	int (*power)(bool);
	int (*gpio_setup)(bool request);
	int rate_msec;
};
#endif
