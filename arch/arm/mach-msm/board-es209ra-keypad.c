#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#define ES209RA_GPIO_POWER_KEY 36

static unsigned int es209ra_row_gpios[] = { 33, 34, 35 };
static unsigned int es209ra_col_gpios[] = { 38, 39, 40, 41, 42 };

#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(es209ra_col_gpios) + (col))

#ifdef CONFIG_ES209RA_LB
static const unsigned short es209ra_keymap[ARRAY_SIZE(es209ra_col_gpios) * ARRAY_SIZE(es209ra_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,		/* volume up(115) */
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,		/* volume down(114) */
	[KEYMAP_INDEX(0, 2)] = KEY_CAMERA,			/* camera(212) */
	[KEYMAP_INDEX(0, 3)] = KEY_EMAIL,			/* AF(215) */
	[KEYMAP_INDEX(0, 4)] = KEY_COFFEE,			/* lock(152) */
	[KEYMAP_INDEX(1, 0)] = KEY_SEND,			/* send(231) */
	[KEYMAP_INDEX(1, 1)] = KEY_KBDILLUMDOWN,	/* menu(229) */
	[KEYMAP_INDEX(1, 2)] = KEY_HOME,			/* home(102) */
	[KEYMAP_INDEX(1, 3)] = KEY_BACK,			/* back(158) */
	[KEYMAP_INDEX(1, 4)] = KEY_END,				/* end(107) */
	[KEYMAP_INDEX(2, 0)] = KEY_UP,				/* up(103) */
	[KEYMAP_INDEX(2, 1)] = KEY_DOWN,			/* down(108) */
	[KEYMAP_INDEX(2, 2)] = KEY_LEFT,			/* left(105) */
	[KEYMAP_INDEX(2, 3)] = KEY_RIGHT,			/* right(106) */
	[KEYMAP_INDEX(2, 4)] = KEY_REPLY			/* click(232) */
};
#else

static const unsigned short es209ra_keymap[ARRAY_SIZE(es209ra_col_gpios) * ARRAY_SIZE(es209ra_row_gpios)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,		/* volume up(115) */
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,		/* volume down(114) */
	[KEYMAP_INDEX(0, 2)] = KEY_CAMERA,			/* camera(212) */
	[KEYMAP_INDEX(0, 3)] = KEY_EMAIL,			/* AF(215) */
	[KEYMAP_INDEX(1, 1)] = KEY_KBDILLUMDOWN,	/* menu(229) */
	[KEYMAP_INDEX(1, 2)] = KEY_HOME,			/* home(102) */
	[KEYMAP_INDEX(1, 3)] = KEY_BACK,			/* back(158) */
};
#endif

static struct gpio_event_matrix_info es209ra_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= es209ra_keymap,
	.output_gpios	= es209ra_row_gpios,
	.input_gpios	= es209ra_col_gpios,
	.noutputs	= ARRAY_SIZE(es209ra_row_gpios),
	.ninputs	= ARRAY_SIZE(es209ra_col_gpios),	
	.settle_time.tv.nsec = 500 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
        .flags          = GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS
};

static struct gpio_event_direct_entry es209ra_keypad_powerkey_map[] = {
	{ ES209RA_GPIO_POWER_KEY, KEY_POWER }
};

static struct gpio_event_input_info es209ra_keypad_powerkey_info = {
	.info.func = gpio_event_input_func,
	.flags = 0,
	.type = EV_KEY,
	.keymap = es209ra_keypad_powerkey_map,
	.keymap_size = ARRAY_SIZE(es209ra_keypad_powerkey_map)
};

static struct gpio_event_info *es209ra_keypad_info[] = {
	&es209ra_matrix_info.info,
	&es209ra_keypad_powerkey_info.info
};

static struct gpio_event_platform_data es209ra_keypad_data = {
	.name		= "es209ra_keypad",
	.info		= es209ra_keypad_info,
	.info_count	= ARRAY_SIZE(es209ra_keypad_info)
};

struct platform_device es209ra_keypad_device = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &es209ra_keypad_data,
	},
};

