#ifndef MSM_FB_PANEL_H
#define MSM_FB_PANEL_H
#include "msm_fb_def.h"
#include <mach/board.h>

struct msm_fb_data_type;
typedef void (*msm_fb_vsync_handler_type) (void *arg);

typedef struct panel_id_s {
	uint16 id;
	uint16 type;
} panel_id_type;

#define NO_PANEL       0xffff
#define MDDI_PANEL     1
#define EBI2_PANEL     2
#define LCDC_PANEL     3
#define EXT_MDDI_PANEL 4
#define TV_PANEL       5
#define HDMI_PANEL     6
#define DTV_PANEL      7

typedef enum {
	DISPLAY_LCD = 0,
	DISPLAY_LCDC,
	DISPLAY_TV,
	DISPLAY_EXT_MDDI,
} DISP_TARGET;

typedef enum {
	DISPLAY_1 = 0,
	DISPLAY_2,
	MAX_PHYS_TARGET_NUM,
} DISP_TARGET_PHYS;

struct lcd_panel_info {
	__u32 vsync_enable;
	__u32 refx100;
	__u32 v_back_porch;
	__u32 v_front_porch;
	__u32 v_pulse_width;
	__u32 hw_vsync_mode;
	__u32 vsync_notifier_period;
};

struct lcdc_panel_info {
	__u32 h_back_porch;
	__u32 h_front_porch;
	__u32 h_pulse_width;
	__u32 v_back_porch;
	__u32 v_front_porch;
	__u32 v_pulse_width;
	__u32 border_clr;
	__u32 underflow_clr;
	__u32 hsync_skew;
};

struct mddi_panel_info {
	__u32 vdopkt;
};

struct msm_panel_info {
	__u32 xres;
	__u32 yres;
	__u32 bpp;
	__u32 type;
	__u32 wait_cycle;
	DISP_TARGET_PHYS pdest;
	__u32 bl_max;
	__u32 bl_min;
	__u32 fb_num;
	__u32 clk_rate;
	__u32 clk_min;
	__u32 clk_max;
	__u32 frame_count;
	__u32 width;
	__u32 height;
        __u32 frame_rate;

	union {
		struct mddi_panel_info mddi;
	};

	union {
		struct lcd_panel_info lcd;
		struct lcdc_panel_info lcdc;
	};
};

struct msm_fb_panel_data {
	struct msm_panel_info panel_info;
	struct panel_data_ext *panel_ext;
	void (*set_rect) (int x, int y, int xres, int yres);
	void (*set_vsync_notifier) (msm_fb_vsync_handler_type, void *arg);
	void (*set_backlight) (struct msm_fb_data_type *);
	int (*on) (struct platform_device *pdev);
	int (*off) (struct platform_device *pdev);
	struct platform_device *next;
};

struct platform_device *msm_fb_device_alloc(struct msm_fb_panel_data *pdata, u32 type, u32 id);
int panel_next_on(struct platform_device *pdev);
int panel_next_off(struct platform_device *pdev);
int lcdc_device_register(struct msm_panel_info *pinfo);
int mddi_toshiba_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

#endif

