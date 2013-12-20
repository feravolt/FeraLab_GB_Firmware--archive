#if !defined(__MDDI_DISPLAY_H__)
#define __MDDI_DISPLAY_H__

#define DBG(klevel, level, string, args...) \
do { \
 if (dbg_lvl >= level) \
 printk(klevel string, ##args); \
} while(0);

#define write_reg(__X,__Y) \
do { \
  mddi_queue_register_write(__X,__Y,TRUE,0); \
} while(0);

#define write_reg_16(__X, __Y0, __Y1, __Y2, __Y3, __NBR) \
do { \
 mddi_host_register_write16(__X, __Y0, __Y1, __Y2, __Y3, __NBR, \
	TRUE, NULL, MDDI_HOST_PRIM); \
} while(0);

#define write_reg_xl(__X, __Y, __NBR) \
do { \
 mddi_host_register_write_xl(__X, __Y, __NBR, \
			TRUE, NULL, MDDI_HOST_PRIM); \
} while(0);

#define LEVEL_QUIET 0
#define LEVEL_DEBUG 1
#define LEVEL_TRACE 2
#define LEVEL_PARAM 3
#define DBC_OFF 0
#define DBC_ON  1
#define DBC_MODE_UI    1
#define DBC_MODE_IMAGE 2
#define DBC_MODE_VIDEO 3
#define POWER_OFF 0
#define POWER_ON  1

enum mddi_lcd_state {
	LCD_STATE_OFF,
	LCD_STATE_POWER_ON,
	LCD_STATE_ON,
	LCD_STATE_SLEEP
};

struct panel_ids_t {
	u32 driver_ic_id;
	u32 cell_id;
	u32 module_id;
	u32 revision_id;
};


#endif

