#ifndef _AR6KAP_COMMON_H_
#define _AR6KAP_COMMON_H_

typedef struct {
    A_UINT8                 mac[ATH_MAC_LEN];
    A_UINT8                 aid;
} station_t;

typedef struct {
    station_t sta[AP_MAX_NUM_STA];
} ap_get_sta_t;

#endif /* _AR6KAP_COMMON_H_ */
