#include "kgsl_log.h"
#include "kgsl_device.h"
#include "kgsl.h"

#define KGSL_LOG_LEVEL_DEFAULT 3
#define KGSL_LOG_LEVEL_MAX     7
unsigned int kgsl_drv_log = KGSL_LOG_LEVEL_DEFAULT;
unsigned int kgsl_cmd_log = KGSL_LOG_LEVEL_DEFAULT;
unsigned int kgsl_ctxt_log = KGSL_LOG_LEVEL_DEFAULT;
unsigned int kgsl_mem_log = KGSL_LOG_LEVEL_DEFAULT;

#ifdef CONFIG_MSM_KGSL_MMU
unsigned int kgsl_cache_enable;
#endif
