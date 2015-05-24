#ifndef _HOST_VERSION_H_
#define _HOST_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <AR6002/AR6K_version.h>

#define ATH_SW_VER_MAJOR      __VER_MAJOR_
#define ATH_SW_VER_MINOR      __VER_MINOR_
#define ATH_SW_VER_PATCH      __VER_PATCH_
#define ATH_SW_VER_BUILD      __BUILD_NUMBER_ 

#ifdef __cplusplus
}
#endif

#endif /* _HOST_VERSION_H_ */
