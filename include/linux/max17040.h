/* FeraVolt */
#ifndef __MAX17040_H
#define __MAX17040_H
#include <linux/types.h>

struct max17040_load_result {
	u8	max;
	u8	min;
};

struct max17040_rcomp_data {
	int	init;
	int	temp_co_hot;
	int	temp_co_cold;
	int	temp_div;
};

struct max17040_voltage_value {
	int	max;
	int	over_voltage;
	int	dead;
};

struct max17040_device_data {
	u8				model[4][16];
	u8				lock[2];
	u8				unlock[2];
	u8				greatest_ocv[2];
	struct max17040_load_result	load_result;
	struct max17040_rcomp_data	rcomp;
	struct max17040_voltage_value	voltage;
};

struct max17040_i2c_platform_data {
	struct max17040_device_data *data;
};

#endif

