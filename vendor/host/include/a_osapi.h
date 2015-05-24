#ifndef _A_OSAPI_H_
#define _A_OSAPI_H_

#ifdef  __linux__
#if !defined(LINUX_EMULATION)
#include "../os/linux/include/osapi_linux.h"
#endif
#endif

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/osapi_wince.h"
#include "../os/windows/common/include/netbuf.h"
#if defined __cplusplus || defined __STDC__
extern "C"
#endif
A_UINT32 a_copy_from_user(void *to, const void *from, A_UINT32 n);
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/osapi_wince.h"
#include "../os/wince/include/netbuf.h"
#if defined __cplusplus || defined __STDC__
extern "C"
#endif
A_UINT32 a_copy_from_user(void *to, const void *from, A_UINT32 n);
#endif

#ifdef REXOS
#include "../os/rexos/include/common/osapi_rexos.h"
#endif

#endif /* _OSAPI_H_ */
