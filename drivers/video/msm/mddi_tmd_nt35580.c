/* FeraVolt */
#include <linux/kernel.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/msm_rpcrouter.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/nt35580.h>
#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"

static void nt35580_lcd_set_disply_on(struct work_struct *ignored);
static DECLARE_WORK(workq_display_on, nt35580_lcd_set_disply_on);

enum mddi_nt35580_lcd_state {
	LCD_STATE_OFF,
	LCD_STATE_WAIT_DISPLAY_ON,
	LCD_STATE_DMA_START,
	LCD_STATE_DMA_COMPLETE,
	LCD_STATE_ON
};

enum mddi_nt35580_client_id {
	CLIENT_ID_UNINITIALIZED,
	CLIENT_ID_TMD_PANEL_OLD_DRIC,
	CLIENT_ID_TMD_PANEL_NEW_DRIC,
	CLIENT_ID_SHARP_PANEL,
	CLIENT_ID_INVALID
};

static enum mddi_nt35580_lcd_state lcd_state;
static enum mddi_nt35580_client_id client_id;

#define write_client_reg(__X, __Y) \
	mddi_queue_register_write(__X, __Y, TRUE, 0);

static uint32 read_client_reg(uint32 addr)
{
	uint32 val;
	mddi_queue_register_read(addr, &val, TRUE, 0);
	return val;
}

struct reg_data {
	uint32 reg_addr;
	uint32 reg_val;
};

static void write_client_reg_table(struct reg_data *table, uint32 size)
{
	int i = 0;
	for (i = 0; i < size; i++)
		write_client_reg(table[i].reg_addr, table[i].reg_val);
}

static struct reg_data exit_sleep_1_tmd_panel_old_dric[] = {
	{0x6E4F, 0x0005},
	{0xF280, 0x0055},
	{0xF281, 0x00AA},
	{0xF282, 0x0066},
	{0xF38E, 0x0020},
	{0x0180, 0x0000},
	{0x0380, 0x0000},
	{0x0480, 0x0001},
	{0x0580, 0x002C},
	{0x0680, 0x0023},
	{0x2080, 0x0004},
	{0x2280, 0x0009},
	{0x2480, 0x0048},
	{0x2580, 0x0038},
	{0x2780, 0x0070},
	{0x2A80, 0x0000},
	{0x2B80, 0x0037},
	{0x2C80, 0x0055},
	{0x2D80, 0x0000},
	{0xD080, 0x0008},
	{0xD180, 0x0015},
	{0xD280, 0x0005},
	{0xD380, 0x0000},
	{0xD480, 0x0062},
	{0xD580, 0x0001},
	{0xD680, 0x005B},
	{0xD780, 0x0001},
	{0xD880, 0x00DE},
	{0xD980, 0x000E},
	{0xDB80, 0x0000},
};

static struct reg_data exit_sleep_1_tmd_panel_new_dric[] = {
	{0x6E4F, 0x0005},
	{0xF280, 0x0055},
	{0xF281, 0x00AA},
	{0xF282, 0x0066},
	{0xF38E, 0x0020},
	{0x0180, 0x0002},
	{0x0380, 0x0000},
	{0x0480, 0x0001},
	{0x0580, 0x002C},
	{0x0680, 0x0023},
	{0x2080, 0x0000},
	{0x2280, 0x0000},
	{0x2480, 0x0050},
	{0x2580, 0x006B},
	{0x2780, 0x0064},
	{0x2A80, 0x0000},
	{0x2B80, 0x00B1},
	{0x2C80, 0x0000},
	{0x2D80, 0x0000},
	{0xD080, 0x0008},
	{0xD180, 0x0016},
	{0xD280, 0x0005},
	{0xD380, 0x0000},
	{0xD480, 0x0062},
	{0xD580, 0x0001},
	{0xD680, 0x005B},
	{0xD780, 0x0001},
	{0xD880, 0x00DE},
	{0xD980, 0x000E},
	{0xDB80, 0x0000},
};

static struct reg_data exit_sleep_1_sharp_panel[] = {
	{0x0180, 0x0002},
	{0x2080, 0x0043},
	{0x2C80, 0x0000},
	{0xD080, 0x000F},
	{0xD680, 0x0055},
};

static struct reg_data exit_sleep_2_tmd_panel_old_dric[] = {
	{0x3500, 0x0000},
	{0x4400, 0x0000},
	{0x4401, 0x0000},
	{0x3600, 0x0001},
	{0xDA80, 0x0040},
	{0x2100, 0x0000},
};

static struct reg_data exit_sleep_2_tmd_panel_new_dric[] = {
	{0x4080, 0x0020},
	{0x4180, 0x0027},
	{0x4280, 0x0042},
	{0x4380, 0x0070},
	{0x4480, 0x0010},
	{0x4580, 0x0037},
	{0x4680, 0x0062},
	{0x4780, 0x008C},
	{0x4880, 0x001E},
	{0x4980, 0x0025},
	{0x4A80, 0x00D4},
	{0x4B80, 0x001E},
	{0x4C80, 0x0041},
	{0x4D80, 0x006C},
	{0x4E80, 0x00BC},
	{0x4F80, 0x00DB},
	{0x5080, 0x0077},
	{0x5180, 0x0073},
	{0x5880, 0x0020},
	{0x5980, 0x0020},
	{0x5A80, 0x0038},
	{0x5B80, 0x0057},
	{0x5C80, 0x0014},
	{0x5D80, 0x003E},
	{0x5E80, 0x0061},
	{0x5F80, 0x0040},
	{0x6080, 0x001A},
	{0x6180, 0x0020},
	{0x6280, 0x0089},
	{0x6380, 0x001D},
	{0x6480, 0x0048},
	{0x6580, 0x006A},
	{0x6680, 0x00A6},
	{0x6780, 0x00D9},
	{0x6880, 0x0078},
	{0x6980, 0x007F},
	{0x7080, 0x0020},
	{0x7180, 0x003A},
	{0x7280, 0x0055},
	{0x7380, 0x007C},
	{0x7480, 0x001A},
	{0x7580, 0x0041},
	{0x7680, 0x0063},
	{0x7780, 0x009B},
	{0x7880, 0x0019},
	{0x7980, 0x0024},
	{0x7A80, 0x00DB},
	{0x7B80, 0x001C},
	{0x7C80, 0x003D},
	{0x7D80, 0x006A},
	{0x7E80, 0x00BB},
	{0x7F80, 0x00DB},
	{0x8080, 0x0077},
	{0x8180, 0x0073},
	{0x8880, 0x0020},
	{0x8980, 0x0022},
	{0x8A80, 0x0038},
	{0x8B80, 0x0058},
	{0x8C80, 0x0016},
	{0x8D80, 0x0041},
	{0x8E80, 0x0061},
	{0x8F80, 0x003A},
	{0x9080, 0x0019},
	{0x9180, 0x0022},
	{0x9280, 0x007B},
	{0x9380, 0x001B},
	{0x9480, 0x003E},
	{0x9580, 0x0063},
	{0x9680, 0x009A},
	{0x9780, 0x00CA},
	{0x9880, 0x0064},
	{0x9980, 0x007F},
	{0xA080, 0x0020},
	{0xA180, 0x003A},
	{0xA280, 0x0060},
	{0xA380, 0x008C},
	{0xA480, 0x0018},
	{0xA580, 0x0045},
	{0xA680, 0x0066},
	{0xA780, 0x00B1},
	{0xA880, 0x001B},
	{0xA980, 0x0026},
	{0xAA80, 0x00E5},
	{0xAB80, 0x001C},
	{0xAC80, 0x0047},
	{0xAD80, 0x0069},
	{0xAE80, 0x00BC},
	{0xAF80, 0x00DB},
	{0xB080, 0x0077},
	{0xB180, 0x0073},
	{0xB880, 0x0020},
	{0xB980, 0x0021},
	{0xBA80, 0x0038},
	{0xBB80, 0x0057},
	{0xBC80, 0x0017},
	{0xBD80, 0x003B},
	{0xBE80, 0x0063},
	{0xBF80, 0x002F},
	{0xC080, 0x001B},
	{0xC180, 0x0026},
	{0xC280, 0x0063},
	{0xC380, 0x001A},
	{0xC480, 0x0036},
	{0xC580, 0x0067},
	{0xC680, 0x0087},
	{0xC780, 0x00B4},
	{0xC880, 0x0064},
	{0xC980, 0x007F},
	{0x3500, 0x0000},
	{0x4400, 0x0000},
	{0x4401, 0x0000},
	{0x3600, 0x0001},
	{0xDA80, 0x0040},
	{0x2100, 0x0000},
};

static struct reg_data exit_sleep_2_sharp_panel[] = {
	{0x3500, 0x0002},
	{0x3600, 0x0000},
	{0x3601, 0x0001},
	{0x3A00, 0x0077},
	{0xDD80, 0x0005},
};

static struct reg_data set_disply_on_tmd_panel_old_dric[] = {
	{0x2C80, 0x0022},
};

static struct reg_data set_disply_on_tmd_panel_new_dric[] = {
	{0x2C80, 0x0022},
	{0x2080, 0x0040},
	{0x2B80, 0x00BA},
	{0x2280, 0x000C},
};

#define NV_RPC_PROG		0x3000000e
#define NV_RPC_VERS		0x00040001
#define NV_CMD_REMOTE_PROC	9
#define NV_READ			0
#define NV_OEMHW_LCD_VSYNC_I	60007
#define MIN_NV	13389 /* ref100=7468 */
#define MAX_NV	18181 /* ref100=5500 */
#define DEF_NV	16766 /* ref100=5964 */

struct rpc_request_nv_cmd {
	struct rpc_request_hdr hdr;
	uint32_t cmd;
	uint32_t item;
	uint32_t more_data;
	uint32_t desc;
};

struct rpc_reply_nv_cmd {
	struct rpc_reply_hdr hdr;
	uint32_t result;
	uint32_t more_data;
	uint32_t desc;
};

struct nv_oemhw_lcd_vsync {
	uint32_t vsync_usec;
};

static void nt35580_lcd_power_on(struct platform_device *pdev)
{
	struct msm_fb_panel_data *panel;
	panel = (struct msm_fb_panel_data *)pdev->dev.platform_data;

	if (panel && panel->panel_ext->power_on)
		panel->panel_ext->power_on();
}

static void nt35580_lcd_driver_initialization(void)
{
	write_client_reg(0x2A00, 0x0000);
	write_client_reg(0x2A01, 0x0000);
	write_client_reg(0x2A02, 0x0001);
	write_client_reg(0x2A03, 0x00DF);
	write_client_reg(0x2B00, 0x0000);
	write_client_reg(0x2B01, 0x0000);
	write_client_reg(0x2B02, 0x0003);
	write_client_reg(0x2B03, 0x0055);
	write_client_reg(0x2D00, 0x0000);
	write_client_reg(0x2D01, 0x0000);
	write_client_reg(0x2D02, 0x0000);
	write_client_reg(0x2D03, 0x0000);
}

static void nt35580_lcd_set_client_id(void)
{
	int ret = 0;
	ret = read_client_reg(0x1080);

	if (0x58 == ret) {
		client_id = CLIENT_ID_TMD_PANEL_NEW_DRIC;
	} else if (0x55 == ret) {
		ret = read_client_reg(0x1280);
		if (0x00 == ret)
			client_id = CLIENT_ID_TMD_PANEL_OLD_DRIC;
		else if (0x01 == ret)
			client_id = CLIENT_ID_SHARP_PANEL;
		else
			client_id = CLIENT_ID_INVALID;
	} else {
		client_id = CLIENT_ID_INVALID;
	}
}

static void nt35580_lcd_exit_sleep(void)
{
	write_client_reg(0x1100, 0x0000);
	msleep(100);

	if (CLIENT_ID_UNINITIALIZED == client_id)
		nt35580_lcd_set_client_id();

	switch (client_id) {
	case CLIENT_ID_TMD_PANEL_OLD_DRIC:
		write_client_reg_table(exit_sleep_1_tmd_panel_old_dric,
			ARRAY_SIZE(exit_sleep_1_tmd_panel_old_dric));
		break;
	case CLIENT_ID_TMD_PANEL_NEW_DRIC:
		write_client_reg_table(exit_sleep_1_tmd_panel_new_dric,
			ARRAY_SIZE(exit_sleep_1_tmd_panel_new_dric));
		break;
	case CLIENT_ID_SHARP_PANEL:
		write_client_reg_table(exit_sleep_1_sharp_panel,
			ARRAY_SIZE(exit_sleep_1_sharp_panel));
		break;
	default:
		break;
	}

	msleep(9);
	switch (client_id) {
	case CLIENT_ID_TMD_PANEL_OLD_DRIC:
		write_client_reg_table(exit_sleep_2_tmd_panel_old_dric,
			ARRAY_SIZE(exit_sleep_2_tmd_panel_old_dric));
		break;
	case CLIENT_ID_TMD_PANEL_NEW_DRIC:
		write_client_reg_table(exit_sleep_2_tmd_panel_new_dric,
			ARRAY_SIZE(exit_sleep_2_tmd_panel_new_dric));
		break;
	case CLIENT_ID_SHARP_PANEL:
		write_client_reg_table(exit_sleep_2_sharp_panel,
			ARRAY_SIZE(exit_sleep_2_sharp_panel));
		break;
	default:
		break;
	}
}

static void nt35580_lcd_set_disply_on(struct work_struct *ignored)
{
	write_client_reg(0x2900, 0x0000);
	msleep(18);

	switch (client_id) {
	case CLIENT_ID_TMD_PANEL_OLD_DRIC:
		write_client_reg_table(set_disply_on_tmd_panel_old_dric,
			ARRAY_SIZE(set_disply_on_tmd_panel_old_dric));
		break;
	case CLIENT_ID_TMD_PANEL_NEW_DRIC:
		write_client_reg_table(set_disply_on_tmd_panel_new_dric,
			ARRAY_SIZE(set_disply_on_tmd_panel_new_dric));
		break;
	case CLIENT_ID_SHARP_PANEL:
		break;
	default:
		break;
	}
	lcd_state = LCD_STATE_ON;
}

static void nt35580_lcd_set_disply_off(void)
{
	write_client_reg(0x2800, 0x0000);
	msleep(60);
}

static void nt35580_lcd_sleep_set(void)
{
	write_client_reg(0x1000, 0x0000);
	msleep(90);
}

static void nt35580_lcd_power_off(struct platform_device *pdev)
{
	struct msm_fb_panel_data *panel;
	panel = (struct msm_fb_panel_data *)pdev->dev.platform_data;

	if (panel && panel->panel_ext->power_off)
		panel->panel_ext->power_off();
}

void mddi_nt35580_lcd_display_on(void)
{
	switch (lcd_state) {
	case LCD_STATE_WAIT_DISPLAY_ON:
		lcd_state = LCD_STATE_DMA_START;
		break;
	case LCD_STATE_DMA_START:
		lcd_state = LCD_STATE_DMA_COMPLETE;
		schedule_work(&workq_display_on);
		break;
	default:
		break;
	}
}

static int mddi_nt35580_lcd_lcd_on(struct platform_device *pdev)
{
	switch (lcd_state) {
	case LCD_STATE_OFF:
		nt35580_lcd_power_on(pdev);
		nt35580_lcd_driver_initialization();
		nt35580_lcd_exit_sleep();
		lcd_state = LCD_STATE_WAIT_DISPLAY_ON;
		break;
	default:
		break;
	}
	return 0;
}

static int mddi_nt35580_lcd_lcd_off(struct platform_device *pdev)
{

	switch (lcd_state) {
	case LCD_STATE_ON:
		nt35580_lcd_set_disply_off();
	case LCD_STATE_WAIT_DISPLAY_ON:
	case LCD_STATE_DMA_START:
	case LCD_STATE_DMA_COMPLETE:
		nt35580_lcd_sleep_set();
		nt35580_lcd_power_off(pdev);
		lcd_state = LCD_STATE_OFF;
		break;
	default:
		break;
	}

	return 0;
}

static int __init mddi_nt35580_lcd_lcd_probe(struct platform_device *pdev)
{
	struct msm_fb_panel_data *panel_data;

	if (!pdev) {
		return -1;
	}
	if (!pdev->dev.platform_data) {
		return -1;
	}
	panel_data = (struct msm_fb_panel_data *)pdev->dev.platform_data;
	panel_data->on = mddi_nt35580_lcd_lcd_on;
	panel_data->off = mddi_nt35580_lcd_lcd_off;
	panel_data->panel_info.lcd.refx100 = 7468;
	panel_data->panel_info.width = 51;
	panel_data->panel_info.height = 89;
	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = {
	.probe = mddi_nt35580_lcd_lcd_probe,
	.driver = {
		.name = "mddi_tmd_wvga",
	},
};

static int __init mddi_nt35580_lcd_lcd_init(void)
{
	int ret;
	client_id = CLIENT_ID_UNINITIALIZED;
	lcd_state = LCD_STATE_OFF;
	ret = platform_driver_register(&this_driver);
	return ret;
}

module_init(mddi_nt35580_lcd_lcd_init);

