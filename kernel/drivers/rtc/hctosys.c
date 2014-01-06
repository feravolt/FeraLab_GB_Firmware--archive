#include <linux/rtc.h>

int rtc_hctosys(void)
{
	int err;
	struct rtc_time tm;
	struct rtc_device *rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

	if (rtc == NULL) {
		return -ENODEV;
	}

	err = rtc_read_time(rtc, &tm);
	if (err == 0) {
		err = rtc_valid_tm(&tm);
		if (err == 0) {
			struct timespec tv;

			tv.tv_nsec = NSEC_PER_SEC >> 1;

			rtc_tm_to_time(&tm, &tv.tv_sec);

			do_settimeofday(&tv);

			dev_info(rtc->dev.parent,
				"setting system clock to "
				"%d-%02d-%02d %02d:%02d:%02d UTC (%u)\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec,
				(unsigned int) tv.tv_sec);
		}
		else
			dev_err(rtc->dev.parent,
				"hctosys: invalid date/time\n");
	}
	else
		dev_err(rtc->dev.parent,
			"hctosys: unable to read the hardware clock\n");

	rtc_class_close(rtc);

	return 0;
}

late_initcall(rtc_hctosys);

