#ifndef _MISCDRV_H
#define _MISCDRV_H


#define HOST_INTEREST_ITEM_ADDRESS(target, item)    \
(((target) == TARGET_TYPE_AR6001) ?     \
   AR6001_HOST_INTEREST_ITEM_ADDRESS(item) :    \
   AR6002_HOST_INTEREST_ITEM_ADDRESS(item))

A_UINT32 ar6kRev2Array[][128]   = {
                                    {0xFFFF, 0xFFFF},      // No Patches
                               };

#define CFG_REV2_ITEMS                0     // no patches so far
#define AR6K_RESET_ADDR               0x4000
#define AR6K_RESET_VAL                0x100

#define EEPROM_SZ                     768
#define EEPROM_WAIT_LIMIT             4

#endif

