#ifndef _AR6000_API_H_
#define _AR6000_API_H_

#ifdef  __linux__
#if !defined(LINUX_EMULATION)
#include "../os/linux/include/ar6xapi_linux.h"
#endif
#endif

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/ar6xapi_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/ar6xapi_wince.h"
#endif

#ifdef REXOS
#include "../os/rexos/include/common/ar6xapi_rexos.h"
#endif

#endif /* _AR6000_API_H */

