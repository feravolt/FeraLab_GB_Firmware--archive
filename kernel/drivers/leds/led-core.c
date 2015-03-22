#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/leds.h>
#include "leds.h"

DECLARE_RWSEM(leds_list_lock);
EXPORT_SYMBOL_GPL(leds_list_lock);
LIST_HEAD(leds_list);
EXPORT_SYMBOL_GPL(leds_list);
