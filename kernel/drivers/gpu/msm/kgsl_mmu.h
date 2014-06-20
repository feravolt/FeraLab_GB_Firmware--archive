#ifndef __GSL_MMU_H
#define __GSL_MMU_H
#include <linux/types.h>
#include <linux/msm_kgsl.h>
#include "kgsl_log.h"
#include "kgsl_sharedmem.h"
#define KGSL_MMU_GLOBAL_PT 0
#define GSL_PT_SUPER_PTE 8
#define GSL_PT_PAGE_WV		0x00000001
#define GSL_PT_PAGE_RV		0x00000002
#define GSL_PT_PAGE_DIRTY	0x00000004
#define KGSL_MMUFLAGS_TLBFLUSH         0x10000000
#define KGSL_MMUFLAGS_PTUPDATE         0x20000000
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


#ifdef CONFIG_MSM_KGSL_MMU
extern unsigned int kgsl_cache_enable;
#endif

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
	unsigned int	refcnt;
	struct kgsl_memdesc  base;
	uint32_t	va_base;
	unsigned int	va_range;
	unsigned int	last_superpte;
	unsigned int	max_entries;
	struct gen_pool *pool;
	struct list_head list;
	unsigned int name;
	struct kgsl_tlbflushfilter tlbflushfilter;
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
	unsigned int	refcnt;
	uint32_t	flags;
	struct kgsl_device	*device;
	unsigned int	config;
	uint32_t	mpu_base;
	int		mpu_range;
	uint32_t	va_base;
	unsigned int	va_range;
	struct kgsl_memdesc    dummyspace;
	struct kgsl_pagetable  *defaultpagetable;
	struct kgsl_pagetable  *hwpagetable;
	unsigned int tlb_flags;
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

#ifdef CONFIG_MSM_KGSL_MMU
int kgsl_mmu_map(struct kgsl_pagetable *pagetable,
		 unsigned int address,
		 int range,
		 unsigned int protflags,
		 unsigned int *gpuaddr,
		 unsigned int flags);

int kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
					unsigned int gpuaddr, int range);

unsigned int kgsl_virtaddr_to_physaddr(unsigned int virtaddr);
#else
static inline int kgsl_mmu_map(struct kgsl_pagetable *pagetable,
		 unsigned int address,
		 int range,
		 unsigned int protflags,
		 unsigned int *gpuaddr,
		 unsigned int flags)
{
	*gpuaddr = address;
	return 0;
}

static inline int kgsl_mmu_unmap(struct kgsl_pagetable *pagetable,
					unsigned int gpuaddr, int range)
{ return 0; }
#endif

int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc, unsigned int protflags,
			unsigned int flags);

int kgsl_mmu_querystats(struct kgsl_pagetable *pagetable,
			struct kgsl_ptstats *stats);

void kgsl_mh_intrcallback(struct kgsl_device *device);
#endif
