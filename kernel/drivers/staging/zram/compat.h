#ifndef _RAMZSWAP_COMPAT_H_
#define _RAMZSWAP_COMPAT_H_
#define CONFIG_SWAP_FREE_NOTIFY
#define blk_queue_physical_block_size(q, size) \
	blk_queue_hardsect_size(q, size)
#define blk_queue_logical_block_size(q, size)
#endif


