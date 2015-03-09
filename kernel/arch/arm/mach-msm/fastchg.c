#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/fastchg.h>
#include <linux/slab.h>

int force_fast_charge;

/* sysfs interface */
static ssize_t force_fast_charge_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
return sprintf(buf, "%d\n", force_fast_charge);
}

static ssize_t force_fast_charge_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
sscanf(buf, "%du", &force_fast_charge);
return count;
}

static struct kobj_attribute force_fast_charge_attribute =
__ATTR(force_fast_charge, 0666, force_fast_charge_show, force_fast_charge_store);

static struct attribute *attrs[] = {
&force_fast_charge_attribute.attr,
NULL,
};

static struct attribute_group attr_group = {
.attrs = attrs,
};

static struct kobject *force_fast_charge_kobj;

static int __init force_fast_charge_init(void)
{
	int retval;

	force_fast_charge = 0;

        force_fast_charge_kobj = kobject_create_and_add("fast_charge", kernel_kobj);
        if (!force_fast_charge_kobj) {
                return -ENOMEM;
        }
        retval = sysfs_create_group(force_fast_charge_kobj, &attr_group);
        if (retval)
                kobject_put(force_fast_charge_kobj);
        return retval;
}
/* end sysfs interface */

static void __exit force_fast_charge_exit(void)
{
	kobject_put(force_fast_charge_kobj);
}

module_init(force_fast_charge_init);
module_exit(force_fast_charge_exit);
