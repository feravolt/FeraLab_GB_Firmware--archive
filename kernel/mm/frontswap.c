#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sysctl.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/proc_fs.h>
#include <linux/security.h>
#include <linux/capability.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/frontswap.h>
#include <linux/swapfile.h>


static struct frontswap_ops frontswap_ops;

int frontswap_enabled;
EXPORT_SYMBOL(frontswap_enabled);

static unsigned long frontswap_gets;
static unsigned long frontswap_succ_puts;
static unsigned long frontswap_failed_puts;
static unsigned long frontswap_flushes;

struct frontswap_ops frontswap_register_ops(struct frontswap_ops *ops)
{
	struct frontswap_ops old = frontswap_ops;

	frontswap_ops = *ops;
	frontswap_enabled = 1;
	return old;
}
EXPORT_SYMBOL(frontswap_register_ops);

void frontswap_init(unsigned type)
{
	if (frontswap_enabled)
		(*frontswap_ops.init)(type);
}
EXPORT_SYMBOL(frontswap_init);

int __frontswap_put_page(struct page *page)
{
	int ret = -1, dup = 0;
	swp_entry_t entry = { .val = page_private(page), };
	int type = swp_type(entry);
	struct swap_info_struct *sis = swap_info[type];
	pgoff_t offset = swp_offset(entry);

	BUG_ON(!PageLocked(page));
	if (frontswap_test(sis, offset))
		dup = 1;
	ret = (*frontswap_ops.put_page)(type, offset, page);
	if (ret == 0) {
		frontswap_set(sis, offset);
		frontswap_succ_puts++;
		if (!dup)
			sis->frontswap_pages++;
	} else if (dup) {
		frontswap_clear(sis, offset);
		sis->frontswap_pages--;
		frontswap_failed_puts++;
	} else
		frontswap_failed_puts++;
	return ret;
}

int __frontswap_get_page(struct page *page)
{
	int ret = -1;
	swp_entry_t entry = { .val = page_private(page), };
	int type = swp_type(entry);
	struct swap_info_struct *sis = swap_info[type];
	pgoff_t offset = swp_offset(entry);

	BUG_ON(!PageLocked(page));
	if (frontswap_test(sis, offset))
		ret = (*frontswap_ops.get_page)(type, offset, page);
	if (ret == 0)
		frontswap_gets++;
	return ret;
}

void __frontswap_flush_page(unsigned type, pgoff_t offset)
{
	struct swap_info_struct *sis = swap_info[type];

	if (frontswap_test(sis, offset)) {
		(*frontswap_ops.flush_page)(type, offset);
		sis->frontswap_pages--;
		frontswap_clear(sis, offset);
		frontswap_flushes++;
	}
}

void __frontswap_flush_area(unsigned type)
{
	struct swap_info_struct *sis = swap_info[type];

	(*frontswap_ops.flush_area)(type);
	sis->frontswap_pages = 0;
	memset(sis->frontswap_map, 0, sis->max / sizeof(long));
}

void frontswap_shrink(unsigned long target_pages)
{
	int wrapped = 0;
	bool locked = false;

	for (wrapped = 0; wrapped <= 3; wrapped++) {

		struct swap_info_struct *si = NULL;
		unsigned long total_pages = 0, total_pages_to_unuse;
		unsigned long pages = 0, unuse_pages = 0;
		int type;
		spin_lock(&swap_lock);
		locked = true;
		total_pages = 0;
		for (type = swap_list.head; type >= 0; type = si->next) {
			si = swap_info[type];
			total_pages += si->frontswap_pages;
		}
		if (total_pages <= target_pages)
			goto out;
		total_pages_to_unuse = total_pages - target_pages;
		for (type = swap_list.head; type >= 0; type = si->next) {
			si = swap_info[type];
			if (total_pages_to_unuse < si->frontswap_pages)
				pages = unuse_pages = total_pages_to_unuse;
			else {
				pages = si->frontswap_pages;
				unuse_pages = 0; /* unuse all */
			}
			if (security_vm_enough_memory_kern(pages))
				continue;
			vm_unacct_memory(pages);
			break;
		}
		if (type < 0)
			goto out;
		locked = false;
		spin_unlock(&swap_lock);
		try_to_unuse(type, true, unuse_pages);
	}

out:
	if (locked)
		spin_unlock(&swap_lock);
	return;
}
EXPORT_SYMBOL(frontswap_shrink);

unsigned long frontswap_curr_pages(void)
{
	int type;
	unsigned long totalpages = 0;
	struct swap_info_struct *si = NULL;

	spin_lock(&swap_lock);
	for (type = swap_list.head; type >= 0; type = si->next) {
		si = swap_info[type];
		totalpages += si->frontswap_pages;
	}
	spin_unlock(&swap_lock);
	return totalpages;
}
EXPORT_SYMBOL(frontswap_curr_pages);

#ifdef CONFIG_SYSFS

#define FRONTSWAP_ATTR_RO(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_RO(_name)
#define FRONTSWAP_ATTR(_name) \
	static struct kobj_attribute _name##_attr = \
		__ATTR(_name, 0644, _name##_show, _name##_store)

static ssize_t curr_pages_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", frontswap_curr_pages());
}

static ssize_t curr_pages_store(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	unsigned long target_pages;
	int err;

	err = strict_strtoul(buf, 10, &target_pages);
	if (err)
		return -EINVAL;

	frontswap_shrink(target_pages);

	return count;
}
FRONTSWAP_ATTR(curr_pages);

static ssize_t succ_puts_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", frontswap_succ_puts);
}
FRONTSWAP_ATTR_RO(succ_puts);

static ssize_t failed_puts_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", frontswap_failed_puts);
}
FRONTSWAP_ATTR_RO(failed_puts);

static ssize_t gets_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", frontswap_gets);
}
FRONTSWAP_ATTR_RO(gets);

static ssize_t flushes_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", frontswap_flushes);
}
FRONTSWAP_ATTR_RO(flushes);

static struct attribute *frontswap_attrs[] = {
	&curr_pages_attr.attr,
	&succ_puts_attr.attr,
	&failed_puts_attr.attr,
	&gets_attr.attr,
	&flushes_attr.attr,
	NULL,
};

static struct attribute_group frontswap_attr_group = {
	.attrs = frontswap_attrs,
	.name = "frontswap",
};

#endif /* CONFIG_SYSFS */

static int __init init_frontswap(void)
{
#ifdef CONFIG_SYSFS
	int err;

	err = sysfs_create_group(mm_kobj, &frontswap_attr_group);
#endif /* CONFIG_SYSFS */
	return 0;
}

static void __exit exit_frontswap(void)
{
	frontswap_shrink(0UL);
}

module_init(init_frontswap);
module_exit(exit_frontswap);
 
