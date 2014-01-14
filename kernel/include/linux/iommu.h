#ifndef __LINUX_IOMMU_H
#define __LINUX_IOMMU_H

#include <linux/scatterlist.h>

#define IOMMU_READ        (1)
#define IOMMU_WRITE       (2)
#define IOMMU_CACHE       (4)

struct device;

struct iommu_domain {
        void *priv;
};

#define IOMMU_CAP_CACHE_COHERENCY        0x1
#define IOMMU_CAP_INTR_REMAP             0x2

struct iommu_ops {
        int (*domain_init)(struct iommu_domain *domain, int flags);
        void (*domain_destroy)(struct iommu_domain *domain);
        int (*attach_dev)(struct iommu_domain *domain, struct device *dev);
        void (*detach_dev)(struct iommu_domain *domain, struct device *dev);
        int (*map)(struct iommu_domain *domain, unsigned long iova,
                 phys_addr_t paddr, int gfp_order, int prot);
        int (*unmap)(struct iommu_domain *domain, unsigned long iova,
                 int gfp_order);
        int (*map_range)(struct iommu_domain *domain, unsigned int iova,
                 struct scatterlist *sg, unsigned int len, int prot);
        int (*unmap_range)(struct iommu_domain *domain, unsigned int iova,
                 unsigned int len);
        phys_addr_t (*iova_to_phys)(struct iommu_domain *domain,
                                 unsigned long iova);
        int (*domain_has_cap)(struct iommu_domain *domain,
                         unsigned long cap);
};

#ifdef CONFIG_IOMMU_API

extern void register_iommu(struct iommu_ops *ops);
extern bool iommu_found(void);
extern struct iommu_domain *iommu_domain_alloc(int flags);
extern void iommu_domain_free(struct iommu_domain *domain);
extern int iommu_attach_device(struct iommu_domain *domain,
                         struct device *dev);
extern void iommu_detach_device(struct iommu_domain *domain,
                                struct device *dev);
extern int iommu_map(struct iommu_domain *domain, unsigned long iova,
                 phys_addr_t paddr, int gfp_order, int prot);
extern int iommu_unmap(struct iommu_domain *domain, unsigned long iova,
                 int gfp_order);
extern int iommu_map_range(struct iommu_domain *domain, unsigned int iova,
                 struct scatterlist *sg, unsigned int len, int prot);
extern int iommu_unmap_range(struct iommu_domain *domain, unsigned int iova,
                 unsigned int len);
extern phys_addr_t iommu_iova_to_phys(struct iommu_domain *domain,
                                 unsigned long iova);
extern int iommu_domain_has_cap(struct iommu_domain *domain,
                                unsigned long cap);

#else /* CONFIG_IOMMU_API */

static inline void register_iommu(struct iommu_ops *ops)
{
}

static inline bool iommu_found(void)
{
        return false;
}

static inline struct iommu_domain *iommu_domain_alloc(int flags)
{
        return NULL;
}

static inline void iommu_domain_free(struct iommu_domain *domain)
{
}

static inline int iommu_attach_device(struct iommu_domain *domain,
                                 struct device *dev)
{
        return -ENODEV;
}

static inline void iommu_detach_device(struct iommu_domain *domain,
                                 struct device *dev)
{
}

static inline int iommu_map(struct iommu_domain *domain, unsigned long iova,
                         phys_addr_t paddr, int gfp_order, int prot)
{
        return -ENODEV;
}

static inline int iommu_unmap(struct iommu_domain *domain, unsigned long iova,
                         int gfp_order)
{
        return -ENODEV;
}

static inline int iommu_map_range(struct iommu_domain *domain,
                                 unsigned int iova, struct scatterlist *sg,
                                 unsigned int len, int prot)
{
        return -ENODEV;
}

static inline int iommu_unmap_range(struct iommu_domain *domain,
                                 unsigned int iova, unsigned int len)
{
        return -ENODEV;
}

static inline phys_addr_t iommu_iova_to_phys(struct iommu_domain *domain,
                                         unsigned long iova)
{
        return 0;
}

static inline int domain_has_cap(struct iommu_domain *domain,
                                 unsigned long cap)
{
        return 0;
}

#endif /* CONFIG_IOMMU_API */
#endif /* __LINUX_IOMMU_H */

