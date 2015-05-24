#ifndef _A_CONFIG_H_
#define _A_CONFIG_H_

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/config_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/config_wince.h"
#endif

#ifdef  __linux__
#if !defined(LINUX_EMULATION)
#include "../os/linux/include/config_linux.h"
#endif
#endif

#ifdef REXOS
#include "../os/rexos/include/common/config_rexos.h"
#endif

#endif
