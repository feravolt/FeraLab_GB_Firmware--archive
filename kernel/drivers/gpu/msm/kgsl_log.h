#ifndef _GSL_LOG_H
#define _GSL_LOG_H

#include <linux/bug.h>
#include <linux/types.h>
#include <linux/msm_kgsl.h>
#include <linux/device.h>

extern unsigned int kgsl_drv_log;
extern unsigned int kgsl_cmd_log;
extern unsigned int kgsl_ctxt_log;
extern unsigned int kgsl_mem_log;

struct device *kgsl_driver_getdevnode(void);

#define KGSL_LOG_VDBG(lvl, fmt, args...) \
	do { \
		if ((lvl) >= 7)  \
			dev_vdbg(kgsl_driver_getdevnode(), "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_DBG(lvl, fmt, args...) \
	do { \
		if ((lvl) >= 7)  \
			dev_dbg(kgsl_driver_getdevnode(), "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_INFO(lvl, fmt, args...) \
	do { \
		if ((lvl) >= 6)  \
			dev_info(kgsl_driver_getdevnode(), "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_WARN(lvl, fmt, args...) \
	do { \
		if ((lvl) >= 4)  \
			dev_warn(kgsl_driver_getdevnode(), "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_ERR(lvl, fmt, args...) \
	do { \
		if ((lvl) >= 3)  \
			dev_err(kgsl_driver_getdevnode(), "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_LOG_FATAL(lvl, fmt, args...) \
	do { \
		if ((lvl) >= 2) \
			dev_crit(kgsl_driver_getdevnode(), "|%s| " fmt, \
					__func__, ##args);\
	} while (0)

#define KGSL_DRV_VDBG(fmt, args...) KGSL_LOG_VDBG(kgsl_drv_log, fmt, ##args)
#define KGSL_DRV_DBG(fmt, args...)  KGSL_LOG_DBG(kgsl_drv_log, fmt, ##args)
#define KGSL_DRV_INFO(fmt, args...) KGSL_LOG_INFO(kgsl_drv_log, fmt, ##args)
#define KGSL_DRV_WARN(fmt, args...) KGSL_LOG_WARN(kgsl_drv_log, fmt, ##args)
#define KGSL_DRV_ERR(fmt, args...)  KGSL_LOG_ERR(kgsl_drv_log, fmt, ##args)
#define KGSL_DRV_FATAL(fmt, args...) KGSL_LOG_FATAL(kgsl_drv_log, fmt, ##args)
#define KGSL_CMD_VDBG(fmt, args...) KGSL_LOG_VDBG(kgsl_cmd_log, fmt, ##args)
#define KGSL_CMD_DBG(fmt, args...)  KGSL_LOG_DBG(kgsl_cmd_log, fmt, ##args)
#define KGSL_CMD_INFO(fmt, args...) KGSL_LOG_INFO(kgsl_cmd_log, fmt, ##args)
#define KGSL_CMD_WARN(fmt, args...) KGSL_LOG_WARN(kgsl_cmd_log, fmt, ##args)
#define KGSL_CMD_ERR(fmt, args...)  KGSL_LOG_ERR(kgsl_cmd_log, fmt, ##args)
#define KGSL_CMD_FATAL(fmt, args...) KGSL_LOG_FATAL(kgsl_cmd_log, fmt, ##args)
#define KGSL_CTXT_VDBG(fmt, args...) KGSL_LOG_VDBG(kgsl_ctxt_log, fmt, ##args)
#define KGSL_CTXT_DBG(fmt, args...)  KGSL_LOG_DBG(kgsl_ctxt_log, fmt, ##args)
#define KGSL_CTXT_INFO(fmt, args...) KGSL_LOG_INFO(kgsl_ctxt_log, fmt, ##args)
#define KGSL_CTXT_WARN(fmt, args...) KGSL_LOG_WARN(kgsl_ctxt_log, fmt, ##args)
#define KGSL_CTXT_ERR(fmt, args...)  KGSL_LOG_ERR(kgsl_ctxt_log, fmt, ##args)
#define KGSL_CTXT_FATAL(fmt, args...) KGSL_LOG_FATAL(kgsl_ctxt_log, fmt, ##args)
#define KGSL_MEM_VDBG(fmt, args...) KGSL_LOG_VDBG(kgsl_mem_log, fmt, ##args)
#define KGSL_MEM_DBG(fmt, args...)  KGSL_LOG_DBG(kgsl_mem_log, fmt, ##args)
#define KGSL_MEM_INFO(fmt, args...) KGSL_LOG_INFO(kgsl_mem_log, fmt, ##args)
#define KGSL_MEM_WARN(fmt, args...) KGSL_LOG_WARN(kgsl_mem_log, fmt, ##args)
#define KGSL_MEM_ERR(fmt, args...)  KGSL_LOG_ERR(kgsl_mem_log, fmt, ##args)
#define KGSL_MEM_FATAL(fmt, args...) KGSL_LOG_FATAL(kgsl_mem_log, fmt, ##args)
#endif
