#ifndef _A_DEBUG_H_
#define _A_DEBUG_H_

#include <a_types.h>
#include <a_osapi.h>

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/debug_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/debug_wince.h"
#endif

#ifndef UNDER_CE
#define DBG_INFO        0x00000001
#define DBG_ERROR       0x00000002
#define DBG_WARNING     0x00000004
#define DBG_SDIO        0x00000008
#define DBG_HIF         0x00000010
#define DBG_HTC         0x00000020
#define DBG_WMI         0x00000040
#define DBG_WMI2        0x00000080
#define DBG_DRIVER      0x00000100

#define DBG_DEFAULTS    (DBG_ERROR|DBG_WARNING)
#endif

#ifdef  __linux__
#if !defined(LINUX_EMULATION)
#include "../os/linux/include/debug_linux.h"
#endif
#endif

#ifdef REXOS
#include "../os/rexos/include/common/debug_rexos.h"
#endif

#endif
