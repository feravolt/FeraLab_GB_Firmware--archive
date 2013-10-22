/*
 * linux/arch/arm/include/asm/neon.h
 *
 * Copyright (C) 2013 Linaro Ltd <ard.biesheuvel@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/hwcap.h>

#define cpu_has_neon()		(!!(elf_hwcap & HWCAP_NEON))

#ifdef __ARM_NEON__
#define kernel_neon_begin() \
	BUILD_BUG_ON_MSG(1, "kernel_neon_begin() called from NEON code")

#else
void kernel_neon_begin(void);
#endif
void kernel_neon_end(void);

