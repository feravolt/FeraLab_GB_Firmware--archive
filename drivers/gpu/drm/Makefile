ccflags-y := -Iinclude/drm

drm-y       :=	drm_auth.o drm_bufs.o drm_cache.o \
		drm_context.o drm_dma.o drm_drawable.o \
		drm_drv.o drm_fops.o drm_gem.o drm_ioctl.o drm_irq.o \
		drm_lock.o drm_memory.o drm_proc.o drm_stub.o drm_vm.o \
		drm_agpsupport.o drm_scatter.o ati_pcigart.o drm_pci.o \
		drm_sysfs.o drm_hashtab.o drm_sman.o drm_mm.o drm_platform.o \
		drm_crtc.o drm_crtc_helper.o drm_modes.o drm_edid.o

drm-$(CONFIG_COMPAT) += drm_ioc32.o
obj-$(CONFIG_DRM)	+= drm.o

