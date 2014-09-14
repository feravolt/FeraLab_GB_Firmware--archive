#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/mach/flash.h>
#include <linux/io.h>
#include <asm/setup.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <mach/msm_iomap.h>
#include <mach/board.h>
#include "smd_private.h"

#define ATAG_MSM_PARTITION 0x4d534D70 /* MSMp */

struct msm_ptbl_entry {
	char name[16];
	__u32 offset;
	__u32 size;
	__u32 flags;
};

#define MSM_MAX_PARTITIONS 8

static struct mtd_partition msm_nand_partitions[MSM_MAX_PARTITIONS];
static char msm_nand_names[MSM_MAX_PARTITIONS * 16];
extern struct flash_platform_data msm_nand_data;
static int __init parse_tag_msm_partition(const struct tag *tag)
{
	struct mtd_partition *ptn = msm_nand_partitions;
	char *name = msm_nand_names;
	struct msm_ptbl_entry *entry = (void *) &tag->u;
	unsigned count, n;

	count = (tag->hdr.size - 2) /
		(sizeof(struct msm_ptbl_entry) / sizeof(__u32));

	if (count > MSM_MAX_PARTITIONS)
		count = MSM_MAX_PARTITIONS;

	for (n = 0; n < count; n++) {
			memcpy(name, entry->name, 15);
			name[15] = 0;
			ptn->name = name;
			ptn->offset = entry->offset;
			ptn->size = entry->size;
			printk(KERN_INFO "Partition (from atag) %s "
				"-- Offset:%llx Size:%llx\n",
				ptn->name, ptn->offset, ptn->size);
			name += 16;
			entry++;
			ptn++;
	}

	 ptn = &msm_nand_partitions[count];
	 ptn->name ="boot";
	 ptn->offset = 0x00000275;
	 ptn->size = 0x00000062;
	 printk("Boot mtd partition '%s' created @%llx (%llu)\n", ptn->name, ptn->offset, ptn->size);
	 count++;

	msm_nand_data.nr_parts = count;
	msm_nand_data.parts = msm_nand_partitions;
	return 0;
}

__tagtable(ATAG_MSM_PARTITION, parse_tag_msm_partition);

#define FLASH_PART_MAGIC1     0x55EE73AA
#define FLASH_PART_MAGIC2     0xE35EBDDB
#define FLASH_PARTITION_VERSION   0x3
#define LINUX_FS_PARTITION_NAME  "0:EFS2APPS"

struct flash_partition_entry {
	char name[16];
	u32 offset;	/* Offset in blocks from beginning of device */
	u32 length;	/* Length of the partition in blocks */
	u8 attrib1;
	u8 attrib2;
	u8 attrib3;
	u8 which_flash;	/* Numeric ID (first = 0, second = 1) */
};
struct flash_partition_table {
	u32 magic1;
	u32 magic2;
	u32 version;
	u32 numparts;
	struct flash_partition_entry part_entry[16];
};


static int get_nand_partitions(void)
{
	struct flash_partition_table *partition_table;
	struct flash_partition_entry *part_entry;
	struct mtd_partition *ptn = msm_nand_partitions;
	char *name = msm_nand_names;
	int part;

	if (msm_nand_data.nr_parts)
		return 0;

	partition_table = (struct flash_partition_table *)
	    smem_alloc(SMEM_AARM_PARTITION_TABLE,
		       sizeof(struct flash_partition_table));

	if (!partition_table) {
		printk(KERN_WARNING "%s: no flash partition table in shared "
		       "memory\n", __func__);
		return -ENOENT;
	}

	if ((partition_table->magic1 != (u32) FLASH_PART_MAGIC1) ||
	    (partition_table->magic2 != (u32) FLASH_PART_MAGIC2) ||
	    (partition_table->version != (u32) FLASH_PARTITION_VERSION)) {
		printk(KERN_WARNING "%s: version mismatch -- magic1=%#x, "
		       "magic2=%#x, version=%#x\n", __func__,
		       partition_table->magic1,
		       partition_table->magic2,
		       partition_table->version);
		return -EFAULT;
	}

	msm_nand_data.nr_parts = 0;
	for (part = 0; part < partition_table->numparts; part++) {
		part_entry = &partition_table->part_entry[part];

		if (strcmp(part_entry->name, LINUX_FS_PARTITION_NAME) == 0) {
			strcpy(name, part_entry->name);
			ptn->name = name;
			ptn->offset = part_entry->offset;
			if (part_entry->length == (u32)-1)
				ptn->size = 0;
			else
				ptn->size = part_entry->length;

			msm_nand_data.nr_parts = 1;
			msm_nand_data.parts = msm_nand_partitions;
			printk(KERN_INFO "Partition(from smem) %s "
					"-- Offset:%llx Size:%llx\n",
					ptn->name, ptn->offset, ptn->size);

			return 0;
		}
	}

	printk(KERN_WARNING "%s: no partition table found!", __func__);
	return -ENODEV;
}

device_initcall(get_nand_partitions);
