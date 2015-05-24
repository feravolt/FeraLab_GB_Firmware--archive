#ifndef _A_TYPES_H_
#define _A_TYPES_H_

#ifdef  __linux__
#if !defined(LINUX_EMULATION)
#include "../os/linux/include/athtypes_linux.h"
#endif
#endif

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/athtypes_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/athtypes_wince.h"
#endif

#ifdef REXOS
#include "../os/rexos/include/common/athtypes_rexos.h"
#endif

#endif /* _ATHTYPES_H_ */
