#include <asm/hwcap.h>

#define cpu_has_neon()		(!!(elf_hwcap & HWCAP_NEON))

#ifdef __ARM_NEON__

#define kernel_neon_begin() \
	BUILD_BUG_ON_MSG(1, "kernel_neon_begin() called from NEON code")

#else
void kernel_neon_begin(void);
#endif
void kernel_neon_end(void);
 
