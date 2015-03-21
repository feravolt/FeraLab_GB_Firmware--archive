#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/bitmap.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include "kgsl_mmu.h"
#include "kgsl_drawctxt.h"
#include "kgsl.h"
#include "yamato_reg.h"
#include "kgsl_device.h"
#include "kgsl_yamato.h"

#define GSL_PT_PAGE_BITS_MASK	0x00000007
#define GSL_PT_PAGE_ADDR_MASK	PAGE_MASK

#define GSL_MMU_INT_MASK \
	(MH_INTERRUPT_MASK__AXI_READ_ERROR | \
	 MH_INTERRUPT_MASK__AXI_WRITE_ERROR)


static ssize_t
sysfs_show_ptpool_entries(struct kobject *kobj,
			  struct kobj_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.entries);
}

static ssize_t
sysfs_show_ptpool_min(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.static_entries);
}

static ssize_t
sysfs_show_ptpool_chunks(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.chunks);
}

static ssize_t
sysfs_show_ptpool_ptsize(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", kgsl_driver.ptpool.ptsize);
}

static struct kobj_attribute attr_ptpool_entries = {
	.attr = { .name = "ptpool_entries", .mode = 0444 },
	.show = sysfs_show_ptpool_entries,
	.store = NULL,
};

static struct kobj_attribute attr_ptpool_min = {
	.attr = { .name = "ptpool_min", .mode = 0444 },
	.show = sysfs_show_ptpool_min,
	.store = NULL,
};

static struct kobj_attribute attr_ptpool_chunks = {
	.attr = { .name = "ptpool_chunks", .mode = 0444 },
	.show = sysfs_show_ptpool_chunks,
	.store = NULL,
};

static struct kobj_attribute attr_ptpool_ptsize = {
	.attr = { .name = "ptpool_ptsize", .mode = 0444 },
	.show = sysfs_show_ptpool_ptsize,
	.store = NULL,
};

static struct attribute *ptpool_attrs[] = {
	&attr_ptpool_entries.attr,
	&attr_ptpool_min.attr,
	&attr_ptpool_chunks.attr,
	&attr_ptpool_ptsize.attr,
	NULL,
};

static struct attribute_group ptpool_attr_group = {
	.attrs = ptpool_attrs,
};

static int
_kgsl_ptpool_add_entries(struct kgsl_ptpool *pool, int count, int dynamic)
{
	struct kgsl_ptpool_chunk *chunk;
	size_t size = ALIGN(count * pool->ptsize, PAGE_SIZE);

	BUG_ON(count == 0);

	if (get_order(size) >= MAX_ORDER) {
		return -EINVAL;
	}

	chunk = kzalloc(sizeof(*chunk), GFP_KERNEL);
	if (chunk == NULL) {
		return -ENOMEM;
	}

	chunk->size = size;
	chunk->count = count;
	chunk->dynamic = dynamic;

	chunk->data = dma_alloc_coherent(NULL, size,
					 &chunk->phys, GFP_KERNEL);

	if (chunk->data == NULL) {
		goto err;
	}

	chunk->bitmap = kzalloc(BITS_TO_LONGS(count) * 4, GFP_KERNEL);

	if (chunk->bitmap == NULL) {
		goto err_dma;
	}

	list_add_tail(&chunk->list, &pool->list);

	pool->chunks++;
	pool->entries += count;

	if (!dynamic)
		pool->static_entries += count;

	return 0;

err_dma:
	dma_free_coherent(NULL, chunk->size, chunk->data, chunk->phys);
err:
	kfree(chunk);
	return -ENOMEM;
}

static void *
_kgsl_ptpool_get_entry(struct kgsl_ptpool *pool, unsigned int *physaddr)
{
	struct kgsl_ptpool_chunk *chunk;

	list_for_each_entry(chunk, &pool->list, list) {
		int bit = find_first_zero_bit(chunk->bitmap, chunk->count);

		if (bit >= chunk->count)
			continue;

		set_bit(bit, chunk->bitmap);
		*physaddr = chunk->phys + (bit * pool->ptsize);

		return chunk->data + (bit * pool->ptsize);
	}

	return NULL;
}

/**
 * kgsl_ptpool_add
 * @pool:  A pointer to a ptpool structure
 * @entries: Number of entries to add
 *
 * Add static entries to the pagetable pool.
 */

int
kgsl_ptpool_add(struct kgsl_ptpool *pool, int count)
{
	int ret = 0;
	BUG_ON(count == 0);

	mutex_lock(&pool->lock);

	/* Only 4MB can be allocated in one chunk, so larger allocations
	   need to be split into multiple sections */

	while (count) {
		int entries = ((count * pool->ptsize) > SZ_4M) ?
			SZ_4M / pool->ptsize : count;

		/* Add the entries as static, i.e. they don't ever stand
		   a chance of being removed */

		ret =  _kgsl_ptpool_add_entries(pool, entries, 0);
		if (ret)
			break;

		count -= entries;
	}

	mutex_unlock(&pool->lock);
	return ret;
}

/**
 * kgsl_ptpool_alloc
 * @pool:  A pointer to a ptpool structure
 * @addr: A pointer to store the physical address of the chunk
 *
 * Allocate a pagetable from the pool.  Returns the virtual address
 * of the pagetable, the physical address is returned in physaddr
 */

void *kgsl_ptpool_alloc(struct kgsl_ptpool *pool, unsigned int *physaddr)
{
	void *addr = NULL;
	int ret;

	mutex_lock(&pool->lock);
	addr = _kgsl_ptpool_get_entry(pool, physaddr);
	if (addr)
		goto done;

	/* Add a chunk for 1 more pagetable and mark it as dynamic */
	ret = _kgsl_ptpool_add_entries(pool, 1, 1);

	if (ret)
		goto done;

	addr = _kgsl_ptpool_get_entry(pool, physaddr);
done:
	mutex_unlock(&pool->lock);
	return addr;
}

static inline void _kgsl_ptpool_rm_chunk(struct kgsl_ptpool_chunk *chunk)
{
	list_del(&chunk->list);

	if (chunk->data)
		dma_free_coherent(NULL, chunk->size, chunk->data,
			chunk->phys);
	kfree(chunk->bitmap);
	kfree(chunk);
}

/**
 * kgsl_ptpool_free
 * @pool:  A pointer to a ptpool structure
 * @addr: A pointer to the virtual address to free
 *
 * Free a pagetable allocated from the pool
 */

void kgsl_ptpool_free(struct kgsl_ptpool *pool, void *addr)
{
	struct kgsl_ptpool_chunk *chunk, *tmp;

	if (pool == NULL || addr == NULL)
		return;

	mutex_lock(&pool->lock);
	list_for_each_entry_safe(chunk, tmp, &pool->list, list)  {
		if (addr >=  chunk->data &&
		    addr < chunk->data + chunk->size) {
			int bit = ((unsigned long) (addr - chunk->data)) /
				pool->ptsize;

			clear_bit(bit, chunk->bitmap);
			memset(addr, 0, pool->ptsize);

			if (chunk->dynamic &&
				bitmap_empty(chunk->bitmap, chunk->count))
				_kgsl_ptpool_rm_chunk(chunk);

			break;
		}
	}

	mutex_unlock(&pool->lock);
}

void kgsl_ptpool_destroy(struct kgsl_ptpool *pool)
{
	struct kgsl_ptpool_chunk *chunk, *tmp;

	if (pool == NULL)
		return;

	mutex_lock(&pool->lock);
	list_for_each_entry_safe(chunk, tmp, &pool->list, list)
		_kgsl_ptpool_rm_chunk(chunk);
	mutex_unlock(&pool->lock);

	memset(pool, 0, sizeof(*pool));
}

/**
 * kgsl_ptpool_init
 * @pool:  A pointer to a ptpool structure to initialize
 * @ptsize: The size of each pagetable entry
 * @entries:  The number of inital entries to add to the pool
 *
 * Initalize a pool and allocate an initial chunk of entries.
 */

int kgsl_ptpool_init(struct kgsl_ptpool *pool, int ptsize, int entries)
{
	int ret = 0;
	BUG_ON(ptsize == 0);

	pool->ptsize = ptsize;
	mutex_init(&pool->lock);
	INIT_LIST_HEAD(&pool->list);

	if (entries) {
		ret = kgsl_ptpool_add(pool, entries);
		if (ret)
			return ret;
	}
	return 0;
}
 
static const struct kgsl_mmu_reg mmu_reg[KGSL_DEVICE_MAX] = {
	{
		.config = REG_MH_MMU_CONFIG,
		.mpu_base = REG_MH_MMU_MPU_BASE,
		.mpu_end = REG_MH_MMU_MPU_END,
		.va_range = REG_MH_MMU_VA_RANGE,
		.pt_page = REG_MH_MMU_PT_BASE,
		.page_fault = REG_MH_MMU_PAGE_FAULT,
		.tran_error = REG_MH_MMU_TRAN_ERROR,
		.invalidate = REG_MH_MMU_INVALIDATE,
		.interrupt_mask = REG_MH_INTERRUPT_MASK,
		.interrupt_status = REG_MH_INTERRUPT_STATUS,
		.interrupt_clear = REG_MH_INTERRUPT_CLEAR
	},
	{
		.config = ADDR_MH_MMU_CONFIG,
		.mpu_base = ADDR_MH_MMU_MPU_BASE,
		.mpu_end = ADDR_MH_MMU_MPU_END,
		.va_range = ADDR_MH_MMU_VA_RANGE,
		.pt_page = ADDR_MH_MMU_PT_BASE,
		.page_fault = ADDR_MH_MMU_PAGE_FAULT,
		.tran_error = ADDR_MH_MMU_TRAN_ERROR,
		.invalidate = ADDR_MH_MMU_INVALIDATE,
		.interrupt_mask = ADDR_MH_INTERRUPT_MASK,
		.interrupt_status = ADDR_MH_INTERRUPT_STATUS,
		.interrupt_clear = ADDR_MH_INTERRUPT_CLEAR
	},
	{
		.config = ADDR_MH_MMU_CONFIG,
		.mpu_base = ADDR_MH_MMU_MPU_BASE,
		.mpu_end = ADDR_MH_MMU_MPU_END,
		.va_range = ADDR_MH_MMU_VA_RANGE,
		.pt_page = ADDR_MH_MMU_PT_BASE,
		.page_fault = ADDR_MH_MMU_PAGE_FAULT,
		.tran_error = ADDR_MH_MMU_TRAN_ERROR,
		.invalidate = ADDR_MH_MMU_INVALIDATE,
		.interrupt_mask = ADDR_MH_INTERRUPT_MASK,
		.interrupt_status = ADDR_MH_INTERRUPT_STATUS,
		.interrupt_clear = ADDR_MH_INTERRUPT_CLEAR
	}
};

static inline uint32_t
kgsl_pt_entry_get(struct kgsl_pagetable *pt, uint32_t va)
{
	return (va - pt->va_base) >> PAGE_SHIFT;
}

static inline void
kgsl_pt_map_set(struct kgsl_pagetable *pt, uint32_t pte, uint32_t val)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	writel(val, &baseptr[pte]);
}

static inline uint32_t
kgsl_pt_map_getaddr(struct kgsl_pagetable *pt, uint32_t pte)
{
	uint32_t *baseptr = (uint32_t *)pt->base.hostptr;
	return readl(&baseptr[pte]) & GSL_PT_PAGE_ADDR_MASK;
}

void kgsl_mh_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;
	unsigned int reg;

	kgsl_regread_isr(device, mmu_reg[device->id].interrupt_status, &status);

	if (status & MH_INTERRUPT_MASK__MMU_PAGE_FAULT) {
		kgsl_regread_isr(device, mmu_reg[device->id].page_fault, &reg);
	}

	kgsl_regwrite_isr(device, mmu_reg[device->id].interrupt_clear, status);
}

static struct kgsl_pagetable *kgsl_mmu_createpagetableobject(
				struct kgsl_mmu *mmu,
				unsigned int name)
{
	int status = 0;
	struct kgsl_pagetable *pagetable = NULL;

	pagetable = kzalloc(sizeof(struct kgsl_pagetable), GFP_KERNEL);
	if (pagetable == NULL) {
		return NULL;
	}

	pagetable->refcnt = 1;
	spin_lock_init(&pagetable->lock);
	pagetable->tlb_flags = 0;
	pagetable->name = name;
	pagetable->va_base = mmu->va_base;
	pagetable->va_range = mmu->va_range;
	pagetable->last_superpte = 0;
	pagetable->max_entries = KGSL_PAGETABLE_ENTRIES(mmu->va_range);

	pagetable->tlbflushfilter.size = (mmu->va_range /
				(PAGE_SIZE * GSL_PT_SUPER_PTE * 8)) + 1;
	pagetable->tlbflushfilter.base = (unsigned int *)
			kzalloc(pagetable->tlbflushfilter.size, GFP_KERNEL);
	if (!pagetable->tlbflushfilter.base) {
		goto err_alloc;
	}
	GSL_TLBFLUSH_FILTER_RESET();

	pagetable->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (pagetable->pool == NULL) {
		goto err_flushfilter;
	}

	if (gen_pool_add(pagetable->pool, pagetable->va_base,
				pagetable->va_range, -1)) {
		goto err_pool;
	}

	pagetable->base.hostptr = kgsl_ptpool_alloc(&kgsl_driver.ptpool, &pagetable->base.physaddr);

	if (pagetable->base.hostptr == NULL)
		goto err_pool;

	pagetable->base.gpuaddr = pagetable->base.physaddr;

	status = kgsl_setup_pt(pagetable);
	if (status)
		goto err_free_sharedmem;

	list_add(&pagetable->list, &kgsl_driver.pagetable_list);
	return pagetable;

err_free_sharedmem:
	kgsl_ptpool_free(&kgsl_driver.ptpool, &pagetable->base.hostptr);
err_pool:
	gen_pool_destroy(pagetable->pool);
err_flushfilter:
	kfree(pagetable->tlbflushfilter.base);
err_alloc:
	kfree(pagetable);

	return NULL;
}

static void kgsl_mmu_destroypagetable(struct kgsl_pagetable *pagetable)
{
	list_del(&pagetable->list);
	kgsl_cleanup_pt(pagetable);
	kgsl_ptpool_free(&kgsl_driver.ptpool, pagetable->base.hostptr);

	if (pagetable->pool) {
		gen_pool_destroy(pagetable->pool);
		pagetable->pool = NULL;
	}

	if (pagetable->tlbflushfilter.base) {
		pagetable->tlbflushfilter.size = 0;
		kfree(pagetable->tlbflushfilter.base);
		pagetable->tlbflushfilter.base = NULL;
	}

	kfree(pagetable);
}

struct kgsl_pagetable *kgsl_mmu_getpagetable(struct kgsl_mmu *mmu,
					     unsigned long name)
{
	struct kgsl_pagetable *pt;

	if (mmu == NULL)
		return NULL;

	mutex_lock(&kgsl_driver.pt_mutex);

	list_for_each_entry(pt,	&kgsl_driver.pagetable_list, list) {
		if (pt->name == name) {
			spin_lock(&pt->lock);
			pt->refcnt++;
			spin_unlock(&pt->lock);
			mutex_unlock(&kgsl_driver.pt_mutex);
			return pt;
		}
	}

	pt = kgsl_mmu_createpagetableobject(mmu, name);
	mutex_unlock(&kgsl_driver.pt_mutex);

	return pt;
}

void kgsl_mmu_putpagetable(struct kgsl_pagetable *pagetable)
{
	bool dead;
	if (pagetable == NULL)
		return;

	mutex_lock(&kgsl_driver.pt_mutex);

	spin_lock(&pagetable->lock);
	dead = (--pagetable->refcnt) == 0;
	spin_unlock(&pagetable->lock);

	if (dead)
		kgsl_mmu_destroypagetable(pagetable);

	mutex_unlock(&kgsl_driver.pt_mutex);
}

int kgsl_mmu_setstate(struct kgsl_device *device,
				struct kgsl_pagetable *pagetable)
{
	int status = 0;
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		if (mmu->hwpagetable != pagetable) {
			mmu->hwpagetable = pagetable;
			spin_lock(&mmu->hwpagetable->lock);
			mmu->hwpagetable->tlb_flags &= ~(1<<device->id);
			spin_unlock(&mmu->hwpagetable->lock);

			status = kgsl_setstate(mmu->device,
				KGSL_MMUFLAGS_TLBFLUSH |
				KGSL_MMUFLAGS_PTUPDATE);

		}
	}

	return status;
}

int kgsl_mmu_init(struct kgsl_device *device)
{
	int status;
	struct kgsl_mmu *mmu = &device->mmu;
	mmu->device = device;

	if ((mmu->config & 0x1) == 0) {
		return 0;
	}

	BUG_ON(mmu->mpu_base & (PAGE_SIZE - 1));
	BUG_ON((mmu->mpu_base + mmu->mpu_range) & (PAGE_SIZE - 1));

	if ((mmu->config & ~0x1) > 0) {
		BUG_ON(mmu->va_range & ((1 << 16) - 1));
		status = kgsl_sharedmem_alloc_coherent(&mmu->dummyspace, 64);
		if (status != 0) {
			goto error;
		}

		kgsl_sharedmem_set(&mmu->dummyspace, 0, 0,
				   mmu->dummyspace.size);

	}

	return 0;

error:
	return status;
}

int kgsl_mmu_start(struct kgsl_device *device)
{
	int status;
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		return 0;
	}

	if ((mmu->config & 0x1) == 0) {
		return 0;
	}

	mmu->flags |= KGSL_FLAGS_STARTED;

	kgsl_regwrite(device, mmu_reg[device->id].config, mmu->config);
	kgsl_regwrite(device, mmu_reg[device->id].interrupt_mask,
				GSL_MMU_INT_MASK);
	kgsl_idle(device,  KGSL_TIMEOUT_DEFAULT);

	kgsl_regwrite(device, mmu_reg[device->id].mpu_base, mmu->mpu_base);
	kgsl_regwrite(device, mmu_reg[device->id].mpu_end,
			mmu->mpu_base + mmu->mpu_range);

	kgsl_regwrite(device, mmu_reg[device->id].interrupt_mask,
			GSL_MMU_INT_MASK | MH_INTERRUPT_MASK__MMU_PAGE_FAULT);

	if ((mmu->config & ~0x1) > 0) {

		kgsl_sharedmem_set(&mmu->dummyspace, 0, 0,
				   mmu->dummyspace.size);
		kgsl_regwrite(device, mmu_reg[device->id].tran_error,
						mmu->dummyspace.physaddr + 32);

		BUG_ON(mmu->defaultpagetable == NULL);
		mmu->hwpagetable = mmu->defaultpagetable;

		kgsl_regwrite(device, mmu_reg[device->id].pt_page,
			      mmu->hwpagetable->base.gpuaddr);
		kgsl_regwrite(device, mmu_reg[device->id].va_range,
			      (mmu->hwpagetable->va_base |
			      (mmu->hwpagetable->va_range >> 16)));
		status = kgsl_setstate(device, KGSL_MMUFLAGS_TLBFLUSH);
		if (status) {
			goto error;
		}
	}

	return 0;
error:
	kgsl_regwrite(device, mmu_reg[device->id].interrupt_mask, 0);
	kgsl_regwrite(device, mmu_reg[device->id].config, 0x00000000);
	return status;
}

unsigned int kgsl_virtaddr_to_physaddr(unsigned int virtaddr)
{
	unsigned int physaddr = 0;
	pgd_t *pgd_ptr = NULL;
	pmd_t *pmd_ptr = NULL;
	pte_t *pte_ptr = NULL, pte;

	pgd_ptr = pgd_offset(current->mm, virtaddr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)) {
		return 0;
	}

	pmd_ptr = pmd_offset(pgd_ptr, virtaddr);
	if (pmd_none(*pmd_ptr) || pmd_bad(*pmd_ptr)) {
		return 0;
	}

	pte_ptr = pte_offset_map(pmd_ptr, virtaddr);
	if (!pte_ptr) {
		return 0;
	}
	pte = *pte_ptr;
	physaddr = pte_pfn(pte);
	pte_unmap(pte_ptr);
	physaddr <<= PAGE_SHIFT;
	return physaddr;
}

int
kgsl_mmu_map(struct kgsl_pagetable *pagetable,
				unsigned int address,
				int range,
				unsigned int protflags,
				unsigned int *gpuaddr,
				unsigned int flags)
{
	int numpages;
	unsigned int pte, ptefirst, ptelast, physaddr;
	int flushtlb, alloc_size;
	unsigned int align = flags & KGSL_MEMFLAGS_ALIGN_MASK;

	BUG_ON(protflags & ~(GSL_PT_PAGE_RV | GSL_PT_PAGE_WV));
	BUG_ON(protflags == 0);
	BUG_ON(range <= 0);

	if (align != KGSL_MEMFLAGS_ALIGN8K && align != KGSL_MEMFLAGS_ALIGN4K) {
		return -EINVAL;
	}

	if (!IS_ALIGNED(address, PAGE_SIZE) || range & ~PAGE_MASK) {
		return -EINVAL;
	}
	alloc_size = range;
	if (align == KGSL_MEMFLAGS_ALIGN8K)
		alloc_size += PAGE_SIZE;

	*gpuaddr = gen_pool_alloc(pagetable->pool, alloc_size);
	if (*gpuaddr == 0) {
		return -ENOMEM;
	}

	if (align == KGSL_MEMFLAGS_ALIGN8K) {
		if (*gpuaddr & ((1 << 13) - 1)) {
			gen_pool_free(pagetable->pool, *gpuaddr, PAGE_SIZE);
			*gpuaddr = *gpuaddr + PAGE_SIZE;
		} else
			gen_pool_free(pagetable->pool, *gpuaddr + range, PAGE_SIZE);
	}

	numpages = (range >> PAGE_SHIFT);

	ptefirst = kgsl_pt_entry_get(pagetable, *gpuaddr);
	ptelast = ptefirst + numpages;

	pte = ptefirst;
	flushtlb = 0;
	if ((ptefirst & (GSL_PT_SUPER_PTE - 1)) != 0 ||
		((ptelast + 1) & (GSL_PT_SUPER_PTE-1)) != 0)
		flushtlb = 1;

	spin_lock(&pagetable->lock);
	for (pte = ptefirst; pte < ptelast; pte++) {
		if ((pte & (GSL_PT_SUPER_PTE-1)) == 0)
			if (GSL_TLBFLUSH_FILTER_ISDIRTY(pte / GSL_PT_SUPER_PTE))
				flushtlb = 1;
		if (flags & KGSL_MEMFLAGS_CONPHYS)
			physaddr = address;
		else if (flags & KGSL_MEMFLAGS_VMALLOC_MEM) {
			physaddr = vmalloc_to_pfn((void *)address);
			physaddr <<= PAGE_SHIFT;
		} else if (flags & KGSL_MEMFLAGS_HOSTADDR)
			physaddr = kgsl_virtaddr_to_physaddr(address);
		else
			physaddr = 0;

		if (physaddr) {
			kgsl_pt_map_set(pagetable, pte, physaddr | protflags);
		} else {
			spin_unlock(&pagetable->lock);
			kgsl_mmu_unmap(pagetable, *gpuaddr, range);
			return -EFAULT;
		}
		address += PAGE_SIZE;
	}

	mb();
	dsb();

	if (flushtlb) {
		pagetable->tlb_flags = UINT_MAX;
		GSL_TLBFLUSH_FILTER_RESET();
	}
	spin_unlock(&pagetable->lock);
	return 0;
}

int
kgsl_mmu_unmap(struct kgsl_pagetable *pagetable, unsigned int gpuaddr,
		int range)
{
	unsigned int numpages;
	unsigned int pte, ptefirst, ptelast, superpte;
	BUG_ON(range <= 0);

	numpages = (range >> PAGE_SHIFT);
	if (range & (PAGE_SIZE - 1))
		numpages++;

	ptefirst = kgsl_pt_entry_get(pagetable, gpuaddr);
	ptelast = ptefirst + numpages;
	spin_lock(&pagetable->lock);
	superpte = ptefirst - (ptefirst & (GSL_PT_SUPER_PTE-1));
	GSL_TLBFLUSH_FILTER_SETDIRTY(superpte / GSL_PT_SUPER_PTE);
	for (pte = ptefirst; pte < ptelast; pte++) {
		kgsl_pt_map_set(pagetable, pte, GSL_PT_PAGE_DIRTY);
		superpte = pte - (pte & (GSL_PT_SUPER_PTE - 1));
		if (pte == superpte)
			GSL_TLBFLUSH_FILTER_SETDIRTY(superpte /
				GSL_PT_SUPER_PTE);
	}

	mb();
	dsb();
	spin_unlock(&pagetable->lock);
	gen_pool_free(pagetable->pool, gpuaddr, range);
	return 0;
}

int kgsl_mmu_map_global(struct kgsl_pagetable *pagetable,
			struct kgsl_memdesc *memdesc, unsigned int protflags,
			unsigned int flags)
{
	int result = -EINVAL;
	unsigned int gpuaddr = 0;

	if (memdesc == NULL)
		goto error;

	result = kgsl_mmu_map(pagetable, memdesc->physaddr, memdesc->size,
				protflags, &gpuaddr, flags);
	if (result)
		goto error;

	if (memdesc->gpuaddr == 0)
		memdesc->gpuaddr = gpuaddr;

	else if (memdesc->gpuaddr != gpuaddr) {
		goto error_unmap;
	}
	return result;
error_unmap:
	kgsl_mmu_unmap(pagetable, gpuaddr, memdesc->size);
error:
	return result;
}

int kgsl_mmu_stop(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->flags & KGSL_FLAGS_STARTED) {
		kgsl_regwrite(device, mmu_reg[device->id].interrupt_mask, 0);
		kgsl_regwrite(device, mmu_reg[device->id].config, 0x00000000);
		mmu->flags &= ~KGSL_FLAGS_STARTED;
	}

	return 0;
}

int kgsl_mmu_close(struct kgsl_device *device)
{
	struct kgsl_mmu *mmu = &device->mmu;

	if (mmu->dummyspace.gpuaddr)
		kgsl_sharedmem_free(&mmu->dummyspace);
	return 0;
}
