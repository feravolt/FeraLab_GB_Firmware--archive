#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <mach/msm_bus.h>
#include "kgsl.h"
#include "kgsl_yamato.h"
#include "kgsl_pm4types.h"
#include "kgsl_cmdstream.h"
#include "kgsl_drawctxt.h"
#include "yamato_reg.h"

#define GSL_RBBM_INT_MASK \
	 (RBBM_INT_CNTL__RDERR_INT_MASK |  \
	  RBBM_INT_CNTL__DISPLAY_UPDATE_INT_MASK)

#define GSL_SQ_INT_MASK \
	(SQ_INT_CNTL__PS_WATCHDOG_MASK | \
	 SQ_INT_CNTL__VS_WATCHDOG_MASK)

/* Yamato MH arbiter config*/
#define KGSL_CFG_YAMATO_MHARB \
	(0x10 \
		| (0 << MH_ARBITER_CONFIG__SAME_PAGE_GRANULARITY__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__L1_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__L2_ARB_CONTROL__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PAGE_SIZE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_REORDER_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_ARB_HOLD_ENABLE__SHIFT) \
		| (0 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT_ENABLE__SHIFT) \
		| (0x8 << MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__CP_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__VGT_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__TC_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__RB_CLNT_ENABLE__SHIFT) \
		| (1 << MH_ARBITER_CONFIG__PA_CLNT_ENABLE__SHIFT))

#define YAMATO_MMU_CONFIG						\
	(0x01								\
	 | (MMU_CONFIG << MH_MMU_CONFIG__RB_W_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_W_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R0_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R1_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R2_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R3_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__CP_R4_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R0_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__VGT_R1_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__TC_R_CLNT_BEHAVIOR__SHIFT)	\
	 | (MMU_CONFIG << MH_MMU_CONFIG__PA_W_CLNT_BEHAVIOR__SHIFT))

static struct kgsl_yamato_device yamato_device = {
	.dev = {
		.name = "kgsl-3d0",
		.id = KGSL_DEVICE_YAMATO,
		.mmu = {
			.config = YAMATO_MMU_CONFIG,
			.mpu_base = 0x00000000,
			.mpu_range =  0xFFFFF000,
			.va_base = 0x66000000,
		},
		.mutex = __MUTEX_INITIALIZER(yamato_device.dev.mutex),
		.state = KGSL_STATE_INIT,
		.active_cnt = 0,
		.context_idr = IDR_INIT(context_idr),
	},
	.gmemspace = {
		.gpu_base = 0,
		.sizebytes = SZ_256K,
	},
	.pfp_fw = NULL,
	.pm4_fw = NULL,
};

/* max msecs to wait for gpu to finish its operation(s) */
#define MAX_WAITGPU_SECS (HZ + HZ/2)


static int kgsl_yamato_start(struct kgsl_device *device,
						unsigned int init_ram);
static int kgsl_yamato_stop(struct kgsl_device *device);

static int kgsl_yamato_gmeminit(struct kgsl_yamato_device *yamato_device)
{
	struct kgsl_device *device = &yamato_device->dev;
	union reg_rb_edram_info rb_edram_info;
	unsigned int gmem_size;
	unsigned int edram_value = 0;

	/* make sure edram range is aligned to size */
	BUG_ON(yamato_device->gmemspace.gpu_base &
				(yamato_device->gmemspace.sizebytes - 1));

	/* get edram_size value equivalent */
	gmem_size = (yamato_device->gmemspace.sizebytes >> 14);
	while (gmem_size >>= 1)
		edram_value++;

	rb_edram_info.val = 0;
	rb_edram_info.f.edram_size = edram_value;
	rb_edram_info.f.edram_range = (yamato_device->gmemspace.gpu_base >> 14);
	kgsl_yamato_regwrite(device, REG_RB_EDRAM_INFO, rb_edram_info.val);
	return 0;
}

static int kgsl_yamato_gmemclose(struct kgsl_device *device)
{
	kgsl_yamato_regwrite(device, REG_RB_EDRAM_INFO, 0x00000000);
	return 0;
}

static void kgsl_yamato_rbbm_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;
	unsigned int rderr = 0;

	kgsl_yamato_regread_isr(device, REG_RBBM_INT_STATUS, &status);

	if (status & RBBM_INT_CNTL__RDERR_INT_MASK) {
		union rbbm_read_error_u rerr;
		kgsl_yamato_regread_isr(device, REG_RBBM_READ_ERROR, &rderr);
		rerr.val = rderr;
	}

	status &= GSL_RBBM_INT_MASK;
	kgsl_yamato_regwrite_isr(device, REG_RBBM_INT_ACK, status);
}

static void kgsl_yamato_sq_intrcallback(struct kgsl_device *device)
{
	unsigned int status = 0;
	kgsl_yamato_regread_isr(device, REG_SQ_INT_STATUS, &status);
	status &= GSL_SQ_INT_MASK;
	kgsl_yamato_regwrite_isr(device, REG_SQ_INT_ACK, status);

}

irqreturn_t kgsl_yamato_isr(int irq, void *data)
{
	irqreturn_t result = IRQ_NONE;
	struct kgsl_device *device;
	unsigned int status;

	device = (struct kgsl_device *) data;

	BUG_ON(device == NULL);
	BUG_ON(device->regspace.sizebytes == 0);
	BUG_ON(device->regspace.mmio_virt_base == 0);

	kgsl_yamato_regread_isr(device, REG_MASTER_INT_SIGNAL, &status);

	if (status & MASTER_INT_SIGNAL__MH_INT_STAT) {
		kgsl_mh_intrcallback(device);
		result = IRQ_HANDLED;
	}

	if (status & MASTER_INT_SIGNAL__CP_INT_STAT) {
		kgsl_cp_intrcallback(device);
		result = IRQ_HANDLED;
	}

	if (status & MASTER_INT_SIGNAL__RBBM_INT_STAT) {
		kgsl_yamato_rbbm_intrcallback(device);
		result = IRQ_HANDLED;
	}

	if (status & MASTER_INT_SIGNAL__SQ_INT_STAT) {
		kgsl_yamato_sq_intrcallback(device);
		result = IRQ_HANDLED;
	}

	if (device->pwrctrl.nap_allowed == true) {
		device->requested_state = KGSL_STATE_NAP;
		schedule_work(&device->idle_check_ws);
	}
	/* Reset the time-out in our idle timer */
	mod_timer(&device->idle_timer,
		jiffies + device->pwrctrl.interval_timeout);
	return result;
}

static int kgsl_yamato_cleanup_pt(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable)
{
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	struct kgsl_ringbuffer *rb = &yamato_device->ringbuffer;
	if (device->mmu.defaultpagetable == pagetable)
		device->mmu.defaultpagetable = NULL;

	kgsl_mmu_unmap(pagetable, rb->buffer_desc.gpuaddr,
			rb->buffer_desc.size);

	kgsl_mmu_unmap(pagetable, rb->memptrs_desc.gpuaddr,
			rb->memptrs_desc.size);

	kgsl_mmu_unmap(pagetable, device->memstore.gpuaddr,
			device->memstore.size);

	kgsl_mmu_unmap(pagetable, device->mmu.dummyspace.gpuaddr,
			device->mmu.dummyspace.size);

	return 0;
}

static int kgsl_yamato_setup_pt(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable)
{
	int result = 0;
	unsigned int flags = KGSL_MEMFLAGS_CONPHYS | KGSL_MEMFLAGS_ALIGN4K;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	struct kgsl_ringbuffer *rb = &yamato_device->ringbuffer;

	BUG_ON(rb->buffer_desc.physaddr == 0);
	BUG_ON(rb->memptrs_desc.physaddr == 0);
	BUG_ON(device->memstore.physaddr == 0);
	BUG_ON(device->mmu.dummyspace.physaddr == 0);
	if (device->mmu.defaultpagetable == NULL)
		device->mmu.defaultpagetable = pagetable;

	result = kgsl_mmu_map_global(pagetable, &rb->buffer_desc,
				     GSL_PT_PAGE_RV, flags);
	if (result)
		goto error;

	result = kgsl_mmu_map_global(pagetable, &rb->memptrs_desc,
				     GSL_PT_PAGE_RV | GSL_PT_PAGE_WV, flags);
	if (result)
		goto unmap_buffer_desc;

	result = kgsl_mmu_map_global(pagetable, &device->memstore,
				     GSL_PT_PAGE_RV | GSL_PT_PAGE_WV, flags);
	if (result)
		goto unmap_memptrs_desc;

	result = kgsl_mmu_map_global(pagetable, &device->mmu.dummyspace,
				     GSL_PT_PAGE_RV | GSL_PT_PAGE_WV, flags);
	if (result)
		goto unmap_memstore_desc;

	return result;

unmap_memstore_desc:
	kgsl_mmu_unmap(pagetable, device->memstore.gpuaddr,
			device->memstore.size);

unmap_memptrs_desc:
	kgsl_mmu_unmap(pagetable, rb->memptrs_desc.gpuaddr,
			rb->memptrs_desc.size);
unmap_buffer_desc:
	kgsl_mmu_unmap(pagetable, rb->buffer_desc.gpuaddr,
			rb->buffer_desc.size);
error:
	return result;
}

static int kgsl_yamato_setstate(struct kgsl_device *device, uint32_t flags)
{
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	unsigned int link[32];
	unsigned int *cmds = &link[0];
	int sizedwords = 0;
	unsigned int mh_mmu_invalidate = 0x00000003; /*invalidate all and tc */

	if (yamato_device->drawctxt_active) {
		if (flags & KGSL_MMUFLAGS_PTUPDATE) {
			*cmds++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE, 1);
			*cmds++ = 0x00000000;
			*cmds++ = pm4_type0_packet(REG_MH_MMU_PT_BASE, 1);
			*cmds++ = device->mmu.hwpagetable->base.gpuaddr;
			sizedwords += 4;
		}

		if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
			if (!(flags & KGSL_MMUFLAGS_PTUPDATE)) {
				*cmds++ = pm4_type3_packet(PM4_WAIT_FOR_IDLE,
								1);
				*cmds++ = 0x00000000;
				sizedwords += 2;
			}
			*cmds++ = pm4_type0_packet(REG_MH_MMU_INVALIDATE, 1);
			*cmds++ = mh_mmu_invalidate;
			sizedwords += 2;
		}

		if (flags & (KGSL_MMUFLAGS_PTUPDATE | KGSL_MMUFLAGS_TLBFLUSH)) {
			*cmds++ = pm4_type3_packet(PM4_INVALIDATE_STATE, 1);
			*cmds++ = 0x7fff; /* invalidate all base pointers */
			sizedwords += 2;
		}

		kgsl_ringbuffer_issuecmds(device, KGSL_CMD_FLAGS_PMODE,
					&link[0], sizedwords);
	} else {
		if (flags & KGSL_MMUFLAGS_PTUPDATE) {
			kgsl_yamato_idle(device, KGSL_TIMEOUT_DEFAULT);
			kgsl_yamato_regwrite(device, REG_MH_MMU_PT_BASE,
				     device->mmu.hwpagetable->base.gpuaddr);
		}

		if (flags & KGSL_MMUFLAGS_TLBFLUSH) {
			kgsl_yamato_regwrite(device, REG_MH_MMU_INVALIDATE,
					   mh_mmu_invalidate);
		}
	}
	return 0;
}

static unsigned int
kgsl_yamato_getchipid(struct kgsl_device *device)
{
	unsigned int chipid;
	unsigned int coreid, majorid, minorid, patchid, revid;

	/* YDX */
	kgsl_yamato_regread_isr(device, REG_RBBM_PERIPHID1, &coreid);
	coreid &= 0xF;

	kgsl_yamato_regread_isr(device, REG_RBBM_PERIPHID2, &majorid);
	majorid = (majorid >> 4) & 0xF;

	kgsl_yamato_regread_isr(device, REG_RBBM_PATCH_RELEASE, &revid);
	minorid = ((revid >> 0)  & 0xFF);
	patchid = ((revid >> 16) & 0xFF);
	chipid  = ((coreid << 24) | (majorid << 16) |
			(minorid << 8) | (patchid << 0));
	if (chipid == KGSL_CHIPID_YAMATODX_REV21)
		chipid = KGSL_CHIPID_YAMATODX_REV211;
	return chipid;
}

int __init
kgsl_yamato_init_pwrctrl(struct kgsl_device *device)
{
	int result = 0;
	struct clk *clk, *grp_clk;
	struct platform_device *pdev = kgsl_driver.pdev;
	struct kgsl_platform_data *pdata = pdev->dev.platform_data;

	/*acquire clocks */
	BUG_ON(device->pwrctrl.grp_clk != NULL);
	if (pdata->grp3d_pclk_name) {
		clk = clk_get(&pdev->dev, pdata->grp3d_pclk_name);
		if (IS_ERR(clk)) {
			result = PTR_ERR(clk);
			goto done;
		}
		device->pwrctrl.grp_pclk = clk;
	}

	clk = clk_get(&pdev->dev, pdata->grp3d_clk_name);
	if (IS_ERR(clk)) {
		result = PTR_ERR(clk);
		goto done;
	}
	device->pwrctrl.grp_clk = grp_clk = clk;

	clk = clk_get(&pdev->dev, "grp_src_clk");
	if (IS_ERR(clk))
		clk = grp_clk; /* Fallback to slave */
	device->pwrctrl.grp_src_clk = clk;

	/* put the AXI bus into asynchronous mode with the graphics cores */
	if (pdata->set_grp3d_async != NULL)
		pdata->set_grp3d_async();
	if (pdata->max_grp3d_freq) {
		device->pwrctrl.clk_freq[KGSL_MIN_FREQ] =
			clk_round_rate(clk, pdata->min_grp3d_freq);
		device->pwrctrl.clk_freq[KGSL_MAX_FREQ] =
			clk_round_rate(clk, pdata->max_grp3d_freq);
		clk_set_rate(clk, device->pwrctrl.clk_freq[KGSL_MIN_FREQ]);
	}

	if (pdata->imem_clk_name != NULL) {
		clk = clk_get(&pdev->dev, pdata->imem_clk_name);
		if (IS_ERR(clk)) {
			result = PTR_ERR(clk);
			goto done;
		}
		device->pwrctrl.imem_clk = clk;
	}

	if (pdata->imem_pclk_name != NULL) {
		clk = clk_get(&pdev->dev, pdata->imem_pclk_name);
		if (IS_ERR(clk)) {
			result = PTR_ERR(clk);
			goto done;
		}
		device->pwrctrl.imem_pclk = clk;
	}

	device->pwrctrl.gpu_reg = regulator_get(NULL, "fs_gfx3d");
	if (IS_ERR(device->pwrctrl.gpu_reg))
		device->pwrctrl.gpu_reg = NULL;

	device->pwrctrl.power_flags = KGSL_PWRFLAGS_CLK_OFF |
		KGSL_PWRFLAGS_AXI_OFF | KGSL_PWRFLAGS_POWER_OFF |
		KGSL_PWRFLAGS_IRQ_OFF;
	device->pwrctrl.nap_allowed = pdata->nap_allowed;
	device->pwrctrl.pwrrail_first = pdata->pwrrail_first;
	device->pwrctrl.clk_freq[KGSL_AXI_HIGH] = pdata->high_axi_3d;
	/* per test, io_fraction default value is set to 33% for best
	   power/performance result */
	device->pwrctrl.io_fraction = 33;
	device->pwrctrl.io_count = 0;
	device->pwrctrl.ebi1_clk = clk_get(NULL, "ebi1_kgsl_clk");
	if (IS_ERR(device->pwrctrl.ebi1_clk))
		device->pwrctrl.ebi1_clk = NULL;
	else
		clk_set_rate(device->pwrctrl.ebi1_clk,
				device->pwrctrl.clk_freq[KGSL_AXI_HIGH] * 1000);
	if (pdata->grp3d_bus_scale_table != NULL) {
		device->pwrctrl.pcl =
		msm_bus_scale_register_client(pdata->grp3d_bus_scale_table);
		if (!device->pwrctrl.pcl) {
			result = -EINVAL;
			goto done;
		}
	}

	device->pwrctrl.pwr_rail = PWR_RAIL_GRP_CLK;
	device->pwrctrl.interval_timeout = pdata->idle_timeout_3d;

	if (internal_pwr_rail_mode(device->pwrctrl.pwr_rail,
						PWR_RAIL_CTL_MANUAL)) {
		result = -EINVAL;
		goto done;
	}

	/*acquire yamato interrupt */
	device->pwrctrl.interrupt_num =
	platform_get_irq_byname(pdev, "kgsl_yamato_irq");

	if (device->pwrctrl.interrupt_num <= 0) {
		result = -EINVAL;
		goto done;
	}

done:
	return result;
}

int __init
kgsl_yamato_init(struct kgsl_device *device)
{
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	int status = -EINVAL;
	struct kgsl_memregion *regspace = &device->regspace;
	struct resource *res = NULL;
	struct kgsl_platform_data *pdata = NULL;
	init_waitqueue_head(&yamato_device->ib1_wq);
	setup_timer(&device->idle_timer, kgsl_timer, (unsigned long)device);
	status = kgsl_create_device_workqueue(device);
	if (status)
		goto error;
	INIT_WORK(&device->idle_check_ws, kgsl_idle_check);

	res = platform_get_resource_byname(kgsl_driver.pdev,
					   IORESOURCE_MEM,
					   "kgsl_reg_memory");

	if (res == NULL) {
		goto error_dest_work_q;
	}

	if (res->start == 0 || resource_size(res) == 0) {
		goto error_dest_work_q;
	}

	regspace->mmio_phys_base = res->start;
	regspace->sizebytes = resource_size(res);

	if (!request_mem_region(regspace->mmio_phys_base,
				regspace->sizebytes, DRIVER_NAME)) {
		status = -ENODEV;
		goto error_dest_work_q;
	}

	regspace->mmio_virt_base = ioremap(regspace->mmio_phys_base,
					   regspace->sizebytes);
	if (regspace->mmio_virt_base == NULL) {
		status = -ENODEV;
		goto error_release_mem;
	}

	status = request_irq(device->pwrctrl.interrupt_num, kgsl_yamato_isr,
			     IRQF_TRIGGER_HIGH, DRIVER_NAME, device);
	if (status) {
		goto error_iounmap;
	}
	device->pwrctrl.have_irq = 1;
	disable_irq(device->pwrctrl.interrupt_num);
	init_completion(&device->hwaccess_gate);
	init_completion(&device->suspend_gate);

	ATOMIC_INIT_NOTIFIER_HEAD(&device->ts_notifier_list);
	INIT_LIST_HEAD(&device->memqueue);

	kgsl_yamato_getfunctable(&device->ftbl);

	pdata = kgsl_driver.pdev->dev.platform_data;
	device->mmu.va_range = pdata->pt_va_size;

	status = kgsl_mmu_init(device);
	if (status != 0) {
		status = -ENODEV;
		goto error_free_irq;
	}

	status = kgsl_cmdstream_init(device);
	if (status != 0) {
		status = -ENODEV;
		goto error_close_mmu;
	}

	status = kgsl_sharedmem_alloc_coherent(&device->memstore,
					       sizeof(device->memstore));
	if (status != 0)  {
		status = -ENODEV;
		goto error_close_cmdstream;
	}
	status = kgsl_ringbuffer_init(device);
	if (status != 0)
		goto error_free_memstore;

	status = kgsl_drawctxt_init(device);
	if (status != 0) {
		goto error_close_rb;
	}

	device->flags &= ~KGSL_FLAGS_SOFT_RESET;
	wake_lock_init(&device->idle_wakelock, WAKE_LOCK_IDLE, device->name);
	return 0;

error_close_rb:
	kgsl_ringbuffer_close(&yamato_device->ringbuffer);
error_free_memstore:
	kgsl_sharedmem_free(&device->memstore);
error_close_cmdstream:
	kgsl_cmdstream_close(device);
error_close_mmu:
	kgsl_mmu_close(device);
error_free_irq:
	free_irq(device->pwrctrl.interrupt_num, NULL);
	device->pwrctrl.have_irq = 0;
error_iounmap:
	iounmap(regspace->mmio_virt_base);
	regspace->mmio_virt_base = NULL;
error_release_mem:
	release_mem_region(regspace->mmio_phys_base, regspace->sizebytes);
error_dest_work_q:
	destroy_workqueue(device->work_queue);
	device->work_queue = NULL;
error:
	return status;
}

int kgsl_yamato_close(struct kgsl_device *device)
{
	struct kgsl_memregion *regspace = &device->regspace;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);

	kgsl_ringbuffer_close(&yamato_device->ringbuffer);
	if (device->memstore.hostptr)
		kgsl_sharedmem_free(&device->memstore);

	kgsl_mmu_close(device);

	kgsl_cmdstream_close(device);

	if (regspace->mmio_virt_base != NULL) {
		iounmap(regspace->mmio_virt_base);
		regspace->mmio_virt_base = NULL;
		release_mem_region(regspace->mmio_phys_base,
					regspace->sizebytes);
	}
	kgsl_pwrctrl_close(device);

	if (device->work_queue) {
		destroy_workqueue(device->work_queue);
		device->work_queue = NULL;
	}

	wake_lock_destroy(&device->idle_wakelock);
	return 0;
}

static int kgsl_yamato_start(struct kgsl_device *device, unsigned int init_ram)
{
	int status = -EINVAL;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	int init_reftimestamp = 0x7fffffff;
	device->state = KGSL_STATE_INIT;
	device->requested_state = KGSL_STATE_NONE;
	/* Order pwrrail/clk sequence based upon platform. */
	if (device->pwrctrl.pwrrail_first)
		kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_POWER_ON);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_CLK_ON);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_AXI_ON);
	if (!device->pwrctrl.pwrrail_first)
		kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_POWER_ON);
	if (kgsl_mmu_start(device))
		goto error_clk_off;

	kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE1, 0xfffffffe);
	device->chip_id = kgsl_yamato_getchipid(device);
	if (device->chip_id == CHIP_REV_251)
		kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE2, 0x000000ff);
	else
		kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE2, 0xffffffff);

	kgsl_yamato_regwrite(device, REG_RBBM_SOFT_RESET, 0x00000001);
	msleep(30);
	kgsl_yamato_regwrite(device, REG_RBBM_SOFT_RESET, 0x00000000);
	kgsl_yamato_regwrite(device, REG_RBBM_CNTL, 0x00004442);
	kgsl_yamato_regwrite(device, REG_MH_ARBITER_CONFIG,
				KGSL_CFG_YAMATO_MHARB);
	kgsl_yamato_regwrite(device, REG_SQ_VS_PROGRAM, 0x00000000);
	kgsl_yamato_regwrite(device, REG_SQ_PS_PROGRAM, 0x00000000);
	kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE1, 0);
	kgsl_yamato_regwrite(device, REG_RBBM_PM_OVERRIDE2, 0x80);

	kgsl_sharedmem_set(&device->memstore, 0, 0,
			   device->memstore.size);

	kgsl_sharedmem_writel(&device->memstore,
			      KGSL_DEVICE_MEMSTORE_OFFSET(ref_wait_ts),
			      init_reftimestamp);

	kgsl_yamato_regwrite(device, REG_RBBM_DEBUG, 0x00080000);
	kgsl_yamato_regwrite(device, REG_RBBM_INT_CNTL, GSL_RBBM_INT_MASK);
	kgsl_yamato_regwrite(device, REG_SQ_INT_CNTL, 0);
	yamato_device->gmemspace.sizebytes = SZ_256K;
	kgsl_yamato_gmeminit(yamato_device);
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_IRQ_ON);

	status = kgsl_ringbuffer_start(&yamato_device->ringbuffer, init_ram);
	if (status != 0)
		goto error_irq_off;

	mod_timer(&device->idle_timer, jiffies + FIRST_TIMEOUT);
	status = KGSL_SUCCESS;
	return status;

error_irq_off:
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_IRQ_OFF);
error_clk_off:
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_AXI_OFF);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_CLK_OFF);
	kgsl_mmu_stop(device);
	return status;
}

static int kgsl_yamato_stop(struct kgsl_device *device)
{
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	del_timer(&device->idle_timer);
	kgsl_yamato_regwrite(device, REG_RBBM_INT_CNTL, 0);
	kgsl_yamato_regwrite(device, REG_SQ_INT_CNTL, 0);
	kgsl_drawctxt_close(device);
	kgsl_ringbuffer_stop(&yamato_device->ringbuffer);
	kgsl_yamato_gmemclose(device);
	kgsl_mmu_stop(device);
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_IRQ_OFF);
	if (!device->pwrctrl.pwrrail_first)
		kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_POWER_OFF);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_AXI_OFF);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_CLK_OFF);
	if (device->pwrctrl.pwrrail_first)
		kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_POWER_OFF);
	return 0;
}

struct kgsl_device *kgsl_get_yamato_generic_device(void)
{
	return &yamato_device.dev;
}

static int kgsl_yamato_getproperty(struct kgsl_device *device,
				enum kgsl_property_type type,
				void *value,
				unsigned int sizebytes)
{
	int status = -EINVAL;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);

	switch (type) {
	case KGSL_PROP_DEVICE_INFO:
		{
			struct kgsl_devinfo devinfo;

			if (sizebytes != sizeof(devinfo)) {
				status = -EINVAL;
				break;
			}

			memset(&devinfo, 0, sizeof(devinfo));
			devinfo.device_id = device->id+1;
			devinfo.chip_id = device->chip_id;
			devinfo.mmu_enabled = kgsl_mmu_isenabled(&device->mmu);
			devinfo.gmem_hostbaseaddr = (unsigned int)
					yamato_device->gmemspace.mmio_virt_base;
			devinfo.gmem_gpubaseaddr = yamato_device->gmemspace.
					gpu_base;
			devinfo.gmem_sizebytes = yamato_device->gmemspace.
					sizebytes;

			if (copy_to_user(value, &devinfo, sizeof(devinfo)) !=
					0) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_DEVICE_SHADOW:
		{
			struct kgsl_shadowprop shadowprop;

			if (sizebytes != sizeof(shadowprop)) {
				status = -EINVAL;
				break;
			}
			memset(&shadowprop, 0, sizeof(shadowprop));
			if (device->memstore.hostptr) {
				shadowprop.gpuaddr = device->memstore.physaddr;
				shadowprop.size = device->memstore.size;
				shadowprop.flags = KGSL_FLAGS_INITIALIZED;
			}
			if (copy_to_user(value, &shadowprop,
				sizeof(shadowprop))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_MMU_ENABLE:
		{
			int mmuProp = 1;
			if (sizebytes != sizeof(int)) {
				status = -EINVAL;
				break;
			}
			if (copy_to_user(value, &mmuProp, sizeof(mmuProp))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	case KGSL_PROP_INTERRUPT_WAITS:
		{
			int int_waits = 1;
			if (sizebytes != sizeof(int)) {
				status = -EINVAL;
				break;
			}
			if (copy_to_user(value, &int_waits, sizeof(int))) {
				status = -EFAULT;
				break;
			}
			status = 0;
		}
		break;
	default:
		status = -EINVAL;
	}

	return status;
}

int kgsl_yamato_idle(struct kgsl_device *device, unsigned int timeout)
{
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	struct kgsl_ringbuffer *rb = &yamato_device->ringbuffer;
	unsigned int rbbm_status;
	unsigned long wait_time = jiffies + MAX_WAITGPU_SECS;
	if (rb->flags & KGSL_FLAGS_STARTED) {
		do {
			GSL_RB_GET_READPTR(rb, &rb->rptr);
			if (time_after(jiffies, wait_time))
				goto err;
		} while (rb->rptr != rb->wptr);
	}

	wait_time = jiffies + MAX_WAITGPU_SECS;
	while (time_before(jiffies, wait_time)) {
		kgsl_yamato_regread(device, REG_RBBM_STATUS, &rbbm_status);
		if (rbbm_status == 0x110) {
			return 0;
		}
	}

err:
	return -ETIMEDOUT;
}

static unsigned int kgsl_yamato_isidle(struct kgsl_device *device)
{
	int status = KGSL_FALSE;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	struct kgsl_ringbuffer *rb = &yamato_device->ringbuffer;
	unsigned int rbbm_status;

	if (rb->flags & KGSL_FLAGS_STARTED) {
		/* Is the ring buffer is empty? */
		GSL_RB_GET_READPTR(rb, &rb->rptr);
		if (!device->active_cnt && (rb->rptr == rb->wptr)) {
			/* Is the core idle? */
			kgsl_yamato_regread(device, REG_RBBM_STATUS,
					    &rbbm_status);
			if (rbbm_status == 0x110)
				status = KGSL_TRUE;
		}
	} else {
		BUG();
	}
	return status;
}


/******************************************************************/
/* Caller must hold the driver mutex. */
static int kgsl_yamato_resume_context(struct kgsl_device *device)
{
	int status = KGSL_SUCCESS;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);

	if (device->pwrctrl.suspended_ctxt != NULL) {
		kgsl_drawctxt_switch(yamato_device,
				     device->pwrctrl.suspended_ctxt, 0);
		status = kgsl_yamato_idle(device, 0);

	}
	return status;
}

static int kgsl_yamato_suspend_context(struct kgsl_device *device)
{
	int status = KGSL_SUCCESS;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	device->pwrctrl.suspended_ctxt = yamato_device->drawctxt_active;
	if (device->pwrctrl.suspended_ctxt != NULL) {
		kgsl_drawctxt_switch(yamato_device, NULL, 0);
		status = kgsl_yamato_idle(device, KGSL_TIMEOUT_DEFAULT);
	}

	return status;
}

void _yamato_regread(struct kgsl_device *device, unsigned int offsetwords,
		    unsigned int *value)
{
	unsigned int *reg;

	BUG_ON(offsetwords*sizeof(uint32_t) >= device->regspace.sizebytes);

	reg = (unsigned int *)(device->regspace.mmio_virt_base
				+ (offsetwords << 2));
	*value = readl(reg);
}

void kgsl_yamato_regread(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int *value)
{
	kgsl_pre_hwaccess(device);
	_yamato_regread(device, offsetwords, value);
}

void kgsl_yamato_regread_isr(struct kgsl_device *device,
			     unsigned int offsetwords,
			     unsigned int *value)
{
	_yamato_regread(device, offsetwords, value);
}


void _yamato_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value)
{
	unsigned int *reg;

	BUG_ON(offsetwords*sizeof(uint32_t) >= device->regspace.sizebytes);

	reg = (unsigned int *)(device->regspace.mmio_virt_base
				+ (offsetwords << 2));
	writel(value, reg);
}

void kgsl_yamato_regwrite(struct kgsl_device *device, unsigned int offsetwords,
				unsigned int value)
{
	kgsl_pre_hwaccess(device);
	_yamato_regwrite(device, offsetwords, value);
}

void kgsl_yamato_regwrite_isr(struct kgsl_device *device,
			      unsigned int offsetwords,
			      unsigned int value)
{
	_yamato_regwrite(device, offsetwords, value);
}

static int kgsl_check_interrupt_timestamp(struct kgsl_device *device,
					unsigned int timestamp)
{
	int status;
	unsigned int ref_ts, enableflag;

	status = kgsl_check_timestamp(device, timestamp);
	if (!status) {
		mutex_lock(&device->mutex);
		kgsl_sharedmem_readl(&device->memstore, &enableflag,
			KGSL_DEVICE_MEMSTORE_OFFSET(ts_cmp_enable));
		rmb();

		if (enableflag) {
			kgsl_sharedmem_readl(&device->memstore, &ref_ts,
				KGSL_DEVICE_MEMSTORE_OFFSET(ref_wait_ts));
			rmb();
			if (timestamp_cmp(ref_ts, timestamp)) {
				kgsl_sharedmem_writel(&device->memstore,
				KGSL_DEVICE_MEMSTORE_OFFSET(ref_wait_ts),
				timestamp);
				wmb();
			}
		} else {
			unsigned int cmds[2];
			kgsl_sharedmem_writel(&device->memstore,
				KGSL_DEVICE_MEMSTORE_OFFSET(ref_wait_ts),
				timestamp);
			enableflag = 1;
			kgsl_sharedmem_writel(&device->memstore,
				KGSL_DEVICE_MEMSTORE_OFFSET(ts_cmp_enable),
				enableflag);
			wmb();
			/* submit a dummy packet so that even if all
			* commands upto timestamp get executed we will still
			* get an interrupt */
			cmds[0] = pm4_type3_packet(PM4_NOP, 1);
			cmds[1] = 0;
			kgsl_ringbuffer_issuecmds(device, 0, &cmds[0], 2);
		}
		mutex_unlock(&device->mutex);
	}

	return status;
}

/*
 wait_io_event_interruptible_timeout checks for the exit condition before
 placing a process in wait q. For conditional interrupts we expect the
 process to already be in its wait q when its exit condition checking
 function is called.
*/
#define kgsl_wait_io_event_interruptible_timeout(wq, condition, timeout)\
({									\
	long __ret = timeout;						\
	__wait_io_event_interruptible_timeout(wq, condition, __ret);	\
	__ret;								\
})
#define kgsl_wait_event_interruptible_timeout(wq, condition, timeout)	\
({									\
	long __ret = timeout;						\
	__wait_event_interruptible_timeout(wq, condition, __ret);	\
	__ret;								\
})

/* MUST be called with the device mutex held */
static int kgsl_yamato_waittimestamp(struct kgsl_device *device,
				unsigned int timestamp,
				unsigned int msecs)
{
	long status = 0;
	struct kgsl_yamato_device *yamato_device = KGSL_YAMATO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (!kgsl_check_timestamp(device, timestamp)) {
		mutex_unlock(&device->mutex);
		/* We need to make sure that the process is placed in wait-q
		 * before its condition is called */
		pwr->io_count = (pwr->io_count + 1) % 100;
		if (pwr->io_count < pwr->io_fraction ||
				 pwr->io_fraction == 100) {
			status = kgsl_wait_io_event_interruptible_timeout(
					yamato_device->ib1_wq,
					kgsl_check_interrupt_timestamp(device,
					timestamp), msecs_to_jiffies(msecs));
		} else {
			status = kgsl_wait_event_interruptible_timeout(
					yamato_device->ib1_wq,
					kgsl_check_interrupt_timestamp(device,
					timestamp), msecs_to_jiffies(msecs));
		}
		mutex_lock(&device->mutex);

		if (status > 0)
			status = 0;
		else if (status == 0) {
			if (!kgsl_check_timestamp(device, timestamp)) {
				status = -ETIMEDOUT;
			}
		}
	}

	return (int)status;
}

static long kgsl_yamato_ioctl(struct kgsl_device_private *dev_priv,
			unsigned int cmd,
			unsigned long arg)
{
	int result = 0;
	struct kgsl_drawctxt_set_bin_base_offset binbase;
	struct kgsl_context *context;

	switch (cmd) {
	case IOCTL_KGSL_DRAWCTXT_SET_BIN_BASE_OFFSET:
		if (copy_from_user(&binbase, (void __user *)arg,
				   sizeof(binbase))) {
			result = -EFAULT;
			break;
		}

		context = kgsl_find_context(dev_priv, binbase.drawctxt_id);
		if (context) {
			result = kgsl_drawctxt_set_bin_base_offset(
					dev_priv->device,
					context,
					binbase.offset);
		} else {
			result = -EINVAL;
		}
		break;

	default:
		result = -EINVAL;
		break;
	}
	return result;

}

int kgsl_yamato_getfunctable(struct kgsl_functable *ftbl)
{
	if (ftbl == NULL)
		return KGSL_FAILURE;
	ftbl->device_regread = kgsl_yamato_regread;
	ftbl->device_regwrite = kgsl_yamato_regwrite;
	ftbl->device_regread_isr = kgsl_yamato_regread_isr;
	ftbl->device_regwrite_isr = kgsl_yamato_regwrite_isr;
	ftbl->device_setstate = kgsl_yamato_setstate;
	ftbl->device_idle = kgsl_yamato_idle;
	ftbl->device_isidle = kgsl_yamato_isidle;
	ftbl->device_suspend_context = kgsl_yamato_suspend_context;
	ftbl->device_resume_context = kgsl_yamato_resume_context;
	ftbl->device_start = kgsl_yamato_start;
	ftbl->device_stop = kgsl_yamato_stop;
	ftbl->device_getproperty = kgsl_yamato_getproperty;
	ftbl->device_waittimestamp = kgsl_yamato_waittimestamp;
	ftbl->device_cmdstream_readtimestamp = kgsl_cmdstream_readtimestamp;
	ftbl->device_issueibcmds = kgsl_ringbuffer_issueibcmds;
	ftbl->device_drawctxt_create = kgsl_drawctxt_create;
	ftbl->device_drawctxt_destroy = kgsl_drawctxt_destroy;
	ftbl->device_ioctl = kgsl_yamato_ioctl;
	ftbl->device_setup_pt = kgsl_yamato_setup_pt;
	ftbl->device_cleanup_pt = kgsl_yamato_cleanup_pt;

	return KGSL_SUCCESS;
}
