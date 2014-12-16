#ifndef __GSL_PM4TYPES_H
#define __GSL_PM4TYPES_H
#define PM4_PKT_MASK	0xc0000000
#define PM4_TYPE0_PKT	((unsigned int)0 << 30)
#define PM4_TYPE1_PKT	((unsigned int)1 << 30)
#define PM4_TYPE2_PKT	((unsigned int)2 << 30)
#define PM4_TYPE3_PKT	((unsigned int)3 << 30)
#define PM4_ME_INIT		0x48
#define PM4_NOP			0x10
#define PM4_INDIRECT_BUFFER	0x3f
#define PM4_INDIRECT_BUFFER_PFD	0x37
#define PM4_WAIT_FOR_IDLE	0x26
#define PM4_WAIT_REG_MEM	0x3c
#define PM4_WAIT_REG_EQ		0x52
#define PM4_WAT_REG_GTE		0x53
#define PM4_WAIT_UNTIL_READ	0x5c
#define PM4_WAIT_IB_PFD_COMPLETE 0x5d
#define PM4_REG_RMW		0x21
#define PM4_REG_TO_MEM		0x3e
#define PM4_MEM_WRITE		0x3d
#define PM4_MEM_WRITE_CNTR	0x4f
#define PM4_COND_EXEC		0x44
#define PM4_COND_WRITE		0x45
#define PM4_EVENT_WRITE		0x46
#define PM4_EVENT_WRITE_SHD	0x58
#define PM4_EVENT_WRITE_CFL	0x59
#define PM4_EVENT_WRITE_ZPD	0x5b
#define PM4_DRAW_INDX		0x22
#define PM4_DRAW_INDX_2		0x36
#define PM4_DRAW_INDX_BIN	0x34
#define PM4_DRAW_INDX_2_BIN	0x35
#define PM4_VIZ_QUERY		0x23
#define PM4_SET_STATE		0x25
#define PM4_SET_CONSTANT	0x2d
#define PM4_IM_LOAD		0x27
#define PM4_IM_LOAD_IMMEDIATE	0x2b
#define PM4_LOAD_CONSTANT_CONTEXT 0x2e
#define PM4_INVALIDATE_STATE	0x3b
#define PM4_SET_SHADER_BASES	0x4A
#define PM4_SET_BIN_MASK	0x50
#define PM4_SET_BIN_SELECT	0x51
#define PM4_CONTEXT_UPDATE	0x5e
#define PM4_INTERRUPT		0x40
#define PM4_IM_STORE            0x2c
#define PM4_SET_BIN_BASE_OFFSET 0x4B
#define PM4_SET_PROTECTED_MODE  0x5f
#define pm4_type0_packet(regindx, cnt) \
	(PM4_TYPE0_PKT | (((cnt)-1) << 16) | ((regindx) & 0x7FFF))

#define pm4_type0_packet_for_sameregister(regindx, cnt) \
	((PM4_TYPE0_PKT | (((cnt)-1) << 16) | ((1 << 15) | \
		((regindx) & 0x7FFF)))

#define pm4_type1_packet(reg0, reg1) \
	 (PM4_TYPE1_PKT | ((reg1) << 12) | (reg0))

#define pm4_type3_packet(opcode, cnt) \
	 (PM4_TYPE3_PKT | (((cnt)-1) << 16) | (((opcode) & 0xFF) << 8))

#define pm4_predicated_type3_packet(opcode, cnt) \
	 (PM4_TYPE3_PKT | (((cnt)-1) << 16) | (((opcode) & 0xFF) << 8) | 0x1)

#define pm4_nop_packet(cnt) \
	 (PM4_TYPE3_PKT | (((cnt)-1) << 16) | (PM4_NOP << 8))

#define PM4_HDR_ME_INIT	pm4_type3_packet(PM4_ME_INIT, 18)
#define PM4_HDR_INDIRECT_BUFFER_PFD pm4_type3_packet(PM4_INDIRECT_BUFFER_PFD, 2)
#define PM4_HDR_INDIRECT_BUFFER	pm4_type3_packet(PM4_INDIRECT_BUFFER, 2)
#endif
