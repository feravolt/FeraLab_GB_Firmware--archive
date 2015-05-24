#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#include "a_debug.h"
#include "htc_api.h"
#include "common_drv.h"


#define NO_VO_SERVICE 1

#ifdef NO_VO_SERVICE
#define DATA_SVCS_USED 3
#else
#define DATA_SVCS_USED 4
#endif

static void RedistributeCredits(COMMON_CREDIT_STATE_INFO *pCredInfo,
                                HTC_ENDPOINT_CREDIT_DIST *pEPDistList);

static void SeekCredits(COMMON_CREDIT_STATE_INFO *pCredInfo,
                        HTC_ENDPOINT_CREDIT_DIST *pEPDistList);

/* reduce an ep's credits back to a set limit */
static INLINE void ReduceCredits(COMMON_CREDIT_STATE_INFO *pCredInfo,
                                HTC_ENDPOINT_CREDIT_DIST  *pEpDist,
                                int                       Limit)
{
    int credits;

        /* set the new limit */
    pEpDist->TxCreditsAssigned = Limit;

    if (pEpDist->TxCredits <= Limit) {
        return;
    }

        /* figure out how much to take away */
    credits = pEpDist->TxCredits - Limit;
        /* take them away */
    pEpDist->TxCredits -= credits;
    pCredInfo->CurrentFreeCredits += credits;
}

/* give an endpoint some credits from the free credit pool */
#define GiveCredits(pCredInfo,pEpDist,credits)      \
{                                                   \
    (pEpDist)->TxCredits += (credits);              \
    (pEpDist)->TxCreditsAssigned += (credits);      \
    (pCredInfo)->CurrentFreeCredits -= (credits);   \
}

static void ar6000_credit_init(void                     *Context,
                               HTC_ENDPOINT_CREDIT_DIST *pEPList,
                               int                      TotalCredits)
{
    HTC_ENDPOINT_CREDIT_DIST *pCurEpDist;
    int                      count;
    COMMON_CREDIT_STATE_INFO *pCredInfo = (COMMON_CREDIT_STATE_INFO *)Context;

    pCredInfo->CurrentFreeCredits = TotalCredits;
    pCredInfo->TotalAvailableCredits = TotalCredits;

    pCurEpDist = pEPList;
    while (pCurEpDist != NULL) {

            /* set minimums for each endpoint */
        pCurEpDist->TxCreditsMin = pCurEpDist->TxCreditsPerMaxMsg;

        if (pCurEpDist->ServiceID == WMI_CONTROL_SVC) {
                /* give control service some credits */
            GiveCredits(pCredInfo,pCurEpDist,pCurEpDist->TxCreditsMin);
                /* control service is always marked active, it never goes inactive EVER */
            SET_EP_ACTIVE(pCurEpDist);
        } else if (pCurEpDist->ServiceID == WMI_DATA_BK_SVC) {
                /* this is the lowest priority data endpoint, save this off for easy access */
            pCredInfo->pLowestPriEpDist = pCurEpDist;
        }
        pCurEpDist = pCurEpDist->pNext;
    }

    if (pCredInfo->CurrentFreeCredits <= 0) {
        AR_DEBUG_PRINTF(ATH_LOG_INF, ("Not enough credits (%d) to do credit distributions \n", TotalCredits));
        A_ASSERT(FALSE);
        return;
    }
    pCurEpDist = pEPList;
    while (pCurEpDist != NULL) {
        if (pCurEpDist->ServiceID == WMI_CONTROL_SVC) {
            pCurEpDist->TxCreditsNorm = pCurEpDist->TxCreditsPerMaxMsg;
        } else {
            count = (pCredInfo->CurrentFreeCredits/pCurEpDist->TxCreditsPerMaxMsg) * pCurEpDist->TxCreditsPerMaxMsg;
            count = (count * 3) >> 2;
            count = max(count,pCurEpDist->TxCreditsPerMaxMsg);
            pCurEpDist->TxCreditsNorm = count;

        }
        pCurEpDist = pCurEpDist->pNext;
    }

}

static void ar6000_credit_distribute(void                     *Context,
                                     HTC_ENDPOINT_CREDIT_DIST *pEPDistList,
                                     HTC_CREDIT_DIST_REASON   Reason)
{
    HTC_ENDPOINT_CREDIT_DIST *pCurEpDist;
    COMMON_CREDIT_STATE_INFO *pCredInfo = (COMMON_CREDIT_STATE_INFO *)Context;

    switch (Reason) {
        case HTC_CREDIT_DIST_SEND_COMPLETE :
            pCurEpDist = pEPDistList;
            while (pCurEpDist != NULL) {

                if (pCurEpDist->TxCreditsToDist > 0) {
                        /* return the credits back to the endpoint */
                    pCurEpDist->TxCredits += pCurEpDist->TxCreditsToDist;
                        /* always zero out when we are done */
                    pCurEpDist->TxCreditsToDist = 0;

                    if (pCurEpDist->TxCredits > pCurEpDist->TxCreditsAssigned) {
                            /* reduce to the assigned limit, previous credit reductions
                             * could have caused the limit to change */
                        ReduceCredits(pCredInfo, pCurEpDist, pCurEpDist->TxCreditsAssigned);
                    }

                    if (pCurEpDist->TxCredits > pCurEpDist->TxCreditsNorm) {
                            /* oversubscribed endpoints need to reduce back to normal */
                        ReduceCredits(pCredInfo, pCurEpDist, pCurEpDist->TxCreditsNorm);
                    }
                
                    if (!IS_EP_ACTIVE(pCurEpDist)) {
                            /* endpoint is inactive, now check for messages waiting for credits */
                        if (pCurEpDist->TxQueueDepth == 0) {
                                /* EP is inactive and there are no pending messages, 
                                 * reduce credits back to zero to recover credits */
                            ReduceCredits(pCredInfo, pCurEpDist, 0);
                        }
                    }
                }

                pCurEpDist = pCurEpDist->pNext;
            }

            break;

        case HTC_CREDIT_DIST_ACTIVITY_CHANGE :
            RedistributeCredits(pCredInfo,pEPDistList);
            break;
        case HTC_CREDIT_DIST_SEEK_CREDITS :
            SeekCredits(pCredInfo,pEPDistList);
            break;
        case HTC_DUMP_CREDIT_STATE :
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Credit Distribution, total : %d, free : %d\n",
            								pCredInfo->TotalAvailableCredits, pCredInfo->CurrentFreeCredits));
            break;
        default:
            break;

    }

        /* sanity checks done after each distribution action */
    A_ASSERT(pCredInfo->CurrentFreeCredits <= pCredInfo->TotalAvailableCredits);
    A_ASSERT(pCredInfo->CurrentFreeCredits >= 0);

}

/* redistribute credits based on activity change */
static void RedistributeCredits(COMMON_CREDIT_STATE_INFO *pCredInfo,
                                HTC_ENDPOINT_CREDIT_DIST *pEPDistList)
{
    HTC_ENDPOINT_CREDIT_DIST *pCurEpDist = pEPDistList;

        /* walk through the list and remove credits from inactive endpoints */
    while (pCurEpDist != NULL) {

        if (pCurEpDist->ServiceID != WMI_CONTROL_SVC) {
            if (!IS_EP_ACTIVE(pCurEpDist)) {
                if (pCurEpDist->TxQueueDepth == 0) {
                        /* EP is inactive and there are no pending messages, reduce credits back to zero */
                    ReduceCredits(pCredInfo, pCurEpDist, 0);
                } else {
                    ReduceCredits(pCredInfo, pCurEpDist, pCurEpDist->TxCreditsMin);
                }
            }
        }
        pCurEpDist = pCurEpDist->pNext;
    }

}

static void SeekCredits(COMMON_CREDIT_STATE_INFO *pCredInfo,
                        HTC_ENDPOINT_CREDIT_DIST *pEPDist)
{
    HTC_ENDPOINT_CREDIT_DIST *pCurEpDist;
    int                      credits = 0;
    int                      need;

    do {

        if (pEPDist->ServiceID == WMI_CONTROL_SVC) {
            break;
        }

        if (pEPDist->ServiceID == WMI_DATA_VI_SVC) {
            if (pEPDist->TxCreditsAssigned >= pEPDist->TxCreditsNorm) {
                 /* limit VI service from oversubscribing */
                 break;
            }
        }
 
        if (pEPDist->ServiceID == WMI_DATA_VO_SVC) {
            if (pEPDist->TxCreditsAssigned >= pEPDist->TxCreditsNorm) {
                 /* limit VO service from oversubscribing */
                break;
            }
        }
        credits = min(pCredInfo->CurrentFreeCredits,pEPDist->TxCreditsSeek);

        if (credits >= pEPDist->TxCreditsSeek) {
                /* we found some to fullfill the seek request */
            break;
        }

        pCurEpDist = pCredInfo->pLowestPriEpDist;
        while (pCurEpDist != pEPDist) {
                /* calculate how many we need so far */
            need = pEPDist->TxCreditsSeek - pCredInfo->CurrentFreeCredits;

            if ((pCurEpDist->TxCreditsAssigned - need) >= pCurEpDist->TxCreditsMin) {
                ReduceCredits(pCredInfo,
                              pCurEpDist,
                              pCurEpDist->TxCreditsAssigned - need);

                if (pCredInfo->CurrentFreeCredits >= pEPDist->TxCreditsSeek) {
                        /* we have enough */
                    break;
                }
            }

            pCurEpDist = pCurEpDist->pPrev;
        }

            /* return what we can get */
        credits = min(pCredInfo->CurrentFreeCredits,pEPDist->TxCreditsSeek);

    } while (FALSE);

        /* did we find some credits? */
    if (credits) {
            /* give what we can */
        GiveCredits(pCredInfo, pEPDist, credits);
    }

}

/* initialize and setup credit distribution */
A_STATUS ar6000_setup_credit_dist(HTC_HANDLE HTCHandle, COMMON_CREDIT_STATE_INFO *pCredInfo)
{
    HTC_SERVICE_ID servicepriority[5];

    A_MEMZERO(pCredInfo,sizeof(COMMON_CREDIT_STATE_INFO));

    servicepriority[0] = WMI_CONTROL_SVC;  /* highest */
    servicepriority[1] = WMI_DATA_VO_SVC;
    servicepriority[2] = WMI_DATA_VI_SVC;
    servicepriority[3] = WMI_DATA_BE_SVC;
    servicepriority[4] = WMI_DATA_BK_SVC; /* lowest */

        /* set callbacks and priority list */
    HTCSetCreditDistribution(HTCHandle,
                             pCredInfo,
                             ar6000_credit_distribute,
                             ar6000_credit_init,
                             servicepriority,
                             5);

    return A_OK;
}

