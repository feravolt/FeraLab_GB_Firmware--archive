#ifndef _A_DRV_H_
#define _A_DRV_H_

#ifdef  __linux__
#if !defined(LINUX_EMULATION)
#include "../os/linux/include/athdrv_linux.h"
#endif
#endif

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/athdrv_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/athdrv_wince.h"
#endif

#ifdef REXOS
#include "../os/rexos/include/common/athdrv_rexos.h"
#endif

#endif /* _ADRV_H_ */
