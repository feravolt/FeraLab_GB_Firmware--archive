#ifndef __linux_video_edid_h__
#define __linux_video_edid_h__
struct edid_info {
	unsigned char dummy[128];
};
extern struct edid_info edid_info;
#endif
