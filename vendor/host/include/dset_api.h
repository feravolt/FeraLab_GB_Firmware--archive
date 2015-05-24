#ifndef _DSET_API_H_
#define _DSET_API_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef CONFIG_HOST_DSET_SUPPORT
#define CONFIG_HOST_DSET_SUPPORT 1
#endif

A_STATUS wmi_dset_open_reply(struct wmi_t *wmip,
                             A_UINT32 status,
                             A_UINT32 access_cookie,
                             A_UINT32 size,
                             A_UINT32 version,
                             A_UINT32 targ_handle,
                             A_UINT32 targ_reply_fn,
                             A_UINT32 targ_reply_arg);

A_STATUS wmi_dset_data_reply(struct wmi_t *wmip,
                             A_UINT32 status,
                             A_UINT8 *host_buf,
                             A_UINT32 length,
                             A_UINT32 targ_buf,
                             A_UINT32 targ_reply_fn,
                             A_UINT32 targ_reply_arg);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _DSET_API_H_ */
