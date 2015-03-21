#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>

#include "kgsl_sharedmem.h"
#include "kgsl_device.h"
#include "kgsl.h"

#ifdef CONFIG_OUTER_CACHE
static void _outer_cache_range_op(unsigned long addr, int size,
				  unsigned int flags)
{
	unsigned long end;

	for (end = addr; end < (addr + size); end += PAGE_SIZE) {
		unsigned long physaddr = 0;

		if (flags & KGSL_MEMFLAGS_VMALLOC_MEM)
			physaddr = page_to_phys(vmalloc_to_page((void *) end));
		else if (flags & KGSL_MEMFLAGS_HOSTADDR)
			physaddr = kgsl_virtaddr_to_physaddr(end);
		else if (flags & KGSL_MEMFLAGS_CONPHYS)
			physaddr = __pa(end);

		if (physaddr == 0) {
			return;
		}

		if (flags & KGSL_MEMFLAGS_CACHE_FLUSH)
			outer_flush_range(physaddr, physaddr + PAGE_SIZE);
		else if (flags & KGSL_MEMFLAGS_CACHE_CLEAN)
			outer_clean_range(physaddr, physaddr + PAGE_SIZE);
		else if (flags & KGSL_MEMFLAGS_CACHE_INV)
			outer_inv_range(physaddr, physaddr + PAGE_SIZE);
	}
	mb();
}
#else
static void _outer_cache_range_op(unsigned long addr, int size,
				  unsigned int flags)
{
}
#endif

void kgsl_cache_range_op(unsigned long addr, int size,
			 unsigned int flags)
{
	BUG_ON(addr & (PAGE_SIZE - 1));
	BUG_ON(size & (PAGE_SIZE - 1));

	if (flags & KGSL_MEMFLAGS_CACHE_FLUSH)
		dmac_flush_range((const void *)addr,
				 (const void *)(addr + size));
	else if (flags & KGSL_MEMFLAGS_CACHE_CLEAN)
		dmac_clean_range((const void *)addr,
				 (const void *)(addr + size));
	else if (flags & KGSL_MEMFLAGS_CACHE_INV)
		dmac_inv_range((const void *)addr,
			       (const void *)(addr + size));

	_outer_cache_range_op(addr, size, flags);

}

int
kgsl_sharedmem_vmalloc(struct kgsl_memdesc *memdesc,
		       struct kgsl_pagetable *pagetable, size_t size)
{
	int result;

	size = ALIGN(size, PAGE_SIZE * 2);

	memdesc->hostptr = vmalloc(size);
	if (memdesc->hostptr == NULL) {
		return -ENOMEM;
	}

	memdesc->size = size;
	memdesc->pagetable = pagetable;
	memdesc->priv = KGSL_MEMFLAGS_VMALLOC_MEM | KGSL_MEMFLAGS_CACHE_CLEAN;

	kgsl_cache_range_op((unsigned int) memdesc->hostptr,
			    size, KGSL_MEMFLAGS_CACHE_INV |
			    KGSL_MEMFLAGS_VMALLOC_MEM);

	result = kgsl_mmu_map(pagetable, (unsigned long) memdesc->hostptr,
			      memdesc->size,
			      GSL_PT_PAGE_RV | GSL_PT_PAGE_WV,
			      &memdesc->gpuaddr,
			      KGSL_MEMFLAGS_ALIGN8K |
			      KGSL_MEMFLAGS_VMALLOC_MEM);

	if (result) {
		vfree(memdesc->hostptr);
		memset(memdesc, 0, sizeof(*memdesc));
	}

	return result;
}

void
kgsl_sharedmem_free(struct kgsl_memdesc *memdesc)
{
	BUG_ON(memdesc == NULL);
	if (memdesc->size > 0) {
		if (memdesc->priv & KGSL_MEMFLAGS_VMALLOC_MEM) {
			if (memdesc->gpuaddr)
				kgsl_mmu_unmap(memdesc->pagetable,
					       memdesc->gpuaddr,
					       memdesc->size);

			if (memdesc->hostptr)
				vfree(memdesc->hostptr);
		} else if (memdesc->priv & KGSL_MEMFLAGS_CONPHYS)
			dma_free_coherent(NULL, memdesc->size,
					  memdesc->hostptr,
					  memdesc->physaddr);
		else
			BUG();
	}

	memset(memdesc, 0, sizeof(struct kgsl_memdesc));
}

int
kgsl_sharedmem_readl(const struct kgsl_memdesc *memdesc,
			uint32_t *dst,
			unsigned int offsetbytes)
{
	if (memdesc == NULL || memdesc->hostptr == NULL || dst == NULL) {
		return -EINVAL;
	}
	if (offsetbytes + sizeof(unsigned int) > memdesc->size) {
		return -ERANGE;
	}
	*dst = readl(memdesc->hostptr + offsetbytes);
	return 0;
}

int
kgsl_sharedmem_read(const struct kgsl_memdesc *memdesc, void *dst,
			unsigned int offsetbytes, unsigned int sizebytes)
{
	BUG_ON(sizebytes == sizeof(unsigned int));
	if (memdesc == NULL || memdesc->hostptr == NULL || dst == NULL) {
		return -EINVAL;
	}
	if (offsetbytes + sizebytes > memdesc->size) {
		return -ERANGE;
	}
	memcpy(dst, memdesc->hostptr + offsetbytes, sizebytes);
	return 0;
}

int
kgsl_sharedmem_writel(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			uint32_t src)
{
	if (memdesc == NULL || memdesc->hostptr == NULL) {
		return -EINVAL;
	}
	if (offsetbytes + sizeof(unsigned int) > memdesc->size) {
		return -ERANGE;
	}
	writel(src, memdesc->hostptr + offsetbytes);
	return 0;
}


int
kgsl_sharedmem_write(const struct kgsl_memdesc *memdesc,
			unsigned int offsetbytes,
			void *src, unsigned int sizebytes)
{
	BUG_ON(sizebytes == sizeof(unsigned int));
	if (memdesc == NULL || memdesc->hostptr == NULL) {
		return -EINVAL;
	}
	if (offsetbytes + sizebytes > memdesc->size) {
		return -ERANGE;
	}

	memcpy(memdesc->hostptr + offsetbytes, src, sizebytes);
	return 0;
}

int
kgsl_sharedmem_set(const struct kgsl_memdesc *memdesc, unsigned int offsetbytes,
			unsigned int value, unsigned int sizebytes)
{
	if (memdesc == NULL || memdesc->hostptr == NULL) {
		return -EINVAL;
	}
	if (offsetbytes + sizebytes > memdesc->size) {
		return -ERANGE;
	}
	memset(memdesc->hostptr + offsetbytes, value, sizebytes);
	return 0;
}

