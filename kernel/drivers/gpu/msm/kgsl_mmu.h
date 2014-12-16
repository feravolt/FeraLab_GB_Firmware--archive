#ifndef __GSL_MMU_H
#define __GSL_MMU_H
#include <linux/types.h>
#include <linux/msm_kgsl.h>
#include "kgsl_sharedmem.h"

#define KGSL_MMU_GLOBAL_PT 0
#define GSL_PT_SUPER_PTE 8
#define GSL_PT_PAGE_WV		0x00000001
#define GSL_PT_PAGE_RV		0x00000002
#define GSL_PT_PAGE_DIRTY	0x00000004
#define KGSL_MMUFLAGS_TLBFLUSH         0x10000000
#define KGSL_MMUFLAGS_PTUPDATE         0x20000000
#define ADDR_MH_ARBITER_CONFIG           0x0A40
#define ADDR_MH_INTERRUPT_CLEAR          0x0A44
#define ADDR_MH_INTERRUPT_MASK           0x0A42
#define ADDR_MH_INTERRUPT_STATUS         0x0A43
#define ADDR_MH_AXI_ERROR                0x0A45
#define ADDR_MH_AXI_HALT_CONTROL         0x0A50
#define ADDR_MH_CLNT_INTF_CTRL_CONFIG1   0x0A54
#define ADDR_MH_CLNT_INTF_CTRL_CONFIG2   0x0A55
#define ADDR_MH_MMU_CONFIG               0x0040
#define ADDR_MH_MMU_INVALIDATE           0x0045
#define ADDR_MH_MMU_MPU_BASE             0x0046
#define ADDR_MH_MMU_MPU_END              0x0047
#define ADDR_MH_MMU_PT_BASE              0x0042
#define ADDR_MH_MMU_TRAN_ERROR           0x0044
#define ADDR_MH_MMU_VA_RANGE             0x0041
#define ADDR_VGC_MH_READ_ADDR            0x0510
#define ADDR_VGC_MH_DATA_ADDR            0x0518
#define ADDR_MH_MMU_PAGE_FAULT           0x0043
#define ADDR_VGC_COMMANDSTREAM           0x0000
#define ADDR_VGC_IRQENABLE               0x0438
#define ADDR_VGC_IRQSTATUS               0x0418
#define ADDR_VGC_IRQ_ACTIVE_CNT          0x04E0
#define ADDR_VGC_MMUCOMMANDSTREAM        0x03FC
#define ADDR_VGV3_CONTROL                0x0070
#define ADDR_VGV3_LAST                   0x007F
#define ADDR_VGV3_MODE                   0x0071
#define ADDR_VGV3_NEXTADDR               0x0075
#define ADDR_VGV3_NEXTCMD                0x0076
#define ADDR_VGV3_WRITEADDR              0x0072
#define GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS     (sizeof(unsigned char) * 8)
#define GSL_TLBFLUSH_FILTER_GET(superpte)			     \
	      (*((unsigned char *)				    \
	      (((unsigned int)pagetable->tlbflushfilter.base)    \
	      + (superpte / GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS))))
#define GSL_TLBFLUSH_FILTER_SETDIRTY(superpte)				\
	      (GSL_TLBFLUSH_FILTER_GET((superpte)) |= 1 <<	    \
	      (superpte % GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS))
#define GSL_TLBFLUSH_FILTER_ISDIRTY(superpte)			 \
	      (GSL_TLBFLUSH_FILTER_GET((superpte)) &		  \
	      (1 << (superpte % GSL_TLBFLUSH_FILTER_ENTRY_NUMBITS)))
#define GSL_TLBFLUSH_FILTER_RESET() memset(pagetable->tlbflushfilter.base,\
				      0, pagetable->tlbflushfilter.size)


extern unsigned int kgsl_cache_enable;
struct kgsl_device;
struct kgsl_ptstats {
	int64_t  maps;
	int64_t  unmaps;
	int64_t  superpteallocs;
	int64_t  superptefrees;
	int64_t  ptswitches;
	int64_t  tlbflushes[KGSL_DEVICE_MAX];
};

struct kgsl_tlbflushfilter {
	unsigned int *base;
	unsigned int size;
};

struct kgsl_pagetable {
	spinlock_t lock;
	unsigned int   refcnt;
	struct kgsl_memdesc  base;
	uint32_t      va_base;
	unsigned int   va_range;
	unsigned int   last_superpte;
	unsigned int   max_entries;
	struct gen_pool *pool;
	struct list_head list;
	unsigned int name;
	struct kgsl_tlbflushfilter tlbflushfilter;
	unsigned int tlb_flags;
};

struct kgsl_mmu_reg {

	uint32_t config;
	uint32_t mpu_base;
	uint32_t mpu_end;
	uint32_t va_range;
	uint32_t pt_page;
	uint32_t page_fault;
	uint32_t tran_error;
	uint32_t invalidate;
	uint32_t interrupt_mask;
	uint32_t interrupt_status;
	uint32_t interrupt_clear;
};

struct kgsl_mmu {
	unsigned int     refcnt;
	uint32_t      flags;
	struct kgsl_device     *device;
	unsigned int     config;
	uint32_t        mpu_base;
	int              mpu_range;
	uint32_t        va_base;
	unsigned int     va_range;
	struct kgsl_memdesc    dummyspace;
	/* current page table object being used by device mmu */
	struct kgsl_pagetable  *defaultpagetable;
	struct kgsl_pagetable  *hwpagetable;
};


static inline int
kgsl_mmu_isenabled(struct kgsl_mmu *mmu)
{
	return ((mmu)->flags & KGSL_FLAGS_STARTED) ? 1 : 0;
}


int kgsl_mmu_init(struct kgsl_device *device);

int kgsl_mmu_start(struct kgsl_device *device);

int kgsl_mmu_stop(struct kgsl_device *device);

int kgsl_mmu_close(struct kgsl_device *device);

struct kgsl_pagetable *kgsl_mmu_getpagetable(struct kgsl_mmu *mmu,
					     unsigned long name);

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable);

int kgsl_mmu_setstate(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable);

static inline unsigned int kgsl_pt_get_flags(struct kgsl_pagetable *pt,
					     enum kgsl_deviceid id)
{
	unsigned int result = 0;
	spin_lock(&pt->lock);
	if (pt->tlb_flags && (1<<id)) {
		result = KGSL_MMUFLAGS_TLBFLUSH;
		pt->tlb_flags &= ~(1<<id);
	}
	spin_unlock(&pt->lock);
	return result;
}

int kgsl_mmu_map(struct kgsl_pagetable *pagetable,
		 unsigned int address,
		 int range,
		 unsigned int protflags,
		 unsigned int *gpuaddr,
		 unsigned int flags);

int kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
					unsigned int gpuaddr, int range);

unsigned int kgsl_virtaddr_to_physaddr(unsigned int virtaddr);

int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc, unsigned int protflags,
			unsigned int flags);

int kgsl_mmu_querystats(struct kgsl_pagetable *pagetable,
			struct kgsl_ptstats *stats);

void kgsl_mh_intrcallback(struct kgsl_device *device);

#endif
