KBUILD_AFLAGS	:=$(KBUILD_AFLAGS:-msoft-float=-Wa,-mfpu=neon -mfloat-abi=softfp -ftree-vectorize -funsafe-math-optimizations)
LDFLAGS		+=--no-warn-mismatch

obj-y			+= vfp.o
vfp-$(CONFIG_VFP)	+= vfpmodule.o entry.o vfphw.o vfpsingle.o vfpdouble.o
