#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/irq.h>
#include <asm/cacheflush.h>
#include <asm/system.h>

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)  	((fb)->var.xres * (fb)->var.yres * 3)

static int total_pixel = 1;
static int memset16_rgb8888(void *_ptr, unsigned short val, unsigned count, struct fb_info *fb)
{
	unsigned short *ptr = _ptr;
	unsigned short red;
	unsigned short green;
	unsigned short blue;
	int need_align = (fb->fix.line_length >> 2) - fb->var.xres;
	int align_amount = need_align << 1;
	int pad = 0;

	red = (val & 0xF800) >> 8;
	green = (val & 0x7E0) >> 3;
	blue = (val & 0x1F) << 3;
	count >>= 1;
	while (count--) {
		*ptr++ = (green << 8) | red;
		*ptr++ = blue;
	if (need_align) {
		if (!(total_pixel % fb->var.xres)) {
			ptr += align_amount;
			pad++;
		}
	}
	total_pixel++;
	}
	return pad * align_amount;
}

int load_565rle_image(char *filename)
{
	struct fb_info *info;
	int fd, err = 0;
	unsigned count, max;
	unsigned short *data, *bits, *ptr;
	struct module *owner;
	int pad;

	info = registered_fb[0];
	if (!info) {
		return -ENODEV;
	}

	owner = info->fbops->owner;
	if (!try_module_get(owner))
		return -ENODEV;
	if (info->fbops->fb_open && info->fbops->fb_open(info, 0)) {
		module_put(owner);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		return -ENOENT;
	}

	count = (unsigned)sys_lseek(fd, (off_t)0, 2);
	if (count == 0) {
		err = -EIO;
		goto err_logo_close_file;
	}

	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if ((unsigned)sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	if (info->screen_base) {
		bits = (unsigned short *)(info->screen_base);
		while (count > 3) {
			unsigned n = ptr[0];
			if (n > max)
				break;
			pad = memset16_rgb8888(bits, ptr[1], n << 1, info);
			bits += n << 1;
			bits += pad;
			max -= n;
			ptr += 2;
			count -= 4;
		}
	}

flush_cache_all();
outer_flush_all();
err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}

EXPORT_SYMBOL(load_565rle_image);

