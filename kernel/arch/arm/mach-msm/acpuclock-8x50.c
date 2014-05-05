/* FeraLab */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/cpufreq.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include "acpuclock.h"
#include "clock.h"

#define SHOT_SWITCH		4
#define HOP_SWITCH		5
#define SIMPLE_SLEW		6
#define COMPLEX_SLEW		7
#define VREF_SEL		1
#define V_STEP			25
#define L_VAL_384MHZ		0xA
#define L_VAL_768MHZ		0x14
#define SEMC_ACPU_MIN_UV_MV	900U
#define SEMC_ACPU_MAX_UV_MV	1450U
#define SPSS_CLK_CNTL_ADDR	(MSM_CSR_BASE + 0x100)
#define SPSS_CLK_SEL_ADDR	(MSM_CSR_BASE + 0x104)
#define SCPLL_CTL_ADDR		(MSM_SCPLL_BASE + 0x4)
#define SCPLL_STATUS_ADDR	(MSM_SCPLL_BASE + 0x18)
#define SCPLL_FSM_CTL_EXT_ADDR	(MSM_SCPLL_BASE + 0x10)

enum {
	ACPU_PLL_TCXO	= -1,
	ACPU_PLL_0	= 0,
	ACPU_PLL_1,
	ACPU_PLL_2,
	ACPU_PLL_3,
	ACPU_PLL_END,
};

struct clkctl_acpu_speed {
	unsigned int     use_for_scaling;
	unsigned int     acpuclk_khz;
	int              pll;
	unsigned int     acpuclk_src_sel;
	unsigned int     acpuclk_src_div;
	unsigned int     ahbclk_khz;
	unsigned int     ahbclk_div;
	unsigned int     axiclk_khz;
	unsigned int     sc_core_src_sel_mask;
	unsigned int     sc_l_value;
	int              vdd;
	unsigned long    lpj;
};

#ifdef CONFIG_X10_UNDERVOLT
struct clkctl_acpu_speed acpu_freq_tbl_1228[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 0, 0, 14000, 0, 0, 925 },
	{ 0, 192000, ACPU_PLL_1, 1, 5, 0, 0, 14000, 2, 0, 925 },
	{ 1, 245760, ACPU_PLL_0, 4, 0, 0, 0, 29000, 0, 0, 925 },
	{ 1, 384000, ACPU_PLL_3, 0, 0, 0, 0, 58000, 1, 0xA, 925 },
	{ 0, 422400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xB, 925 },
	{ 0, 460800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xC, 925 },
	{ 0, 499200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xD, 975 },
	{ 0, 537600, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xE, 975 },
	{ 1, 576000, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xF, 975 },
	{ 0, 614400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x10, 1025 },
	{ 0, 652800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x11, 1050 },
	{ 0, 691200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x12, 1075 },
	{ 0, 729600, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x13, 1100 },
	{ 1, 768000, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x14, 1100 },
	{ 0, 806400, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x15, 1150 },
	{ 0, 844800, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x16, 1175 },
	{ 0, 883200, ACPU_PLL_3, 0, 0, 0, 0, 160000, 1, 0x17, 1225 },
	{ 0, 921600, ACPU_PLL_3, 0, 0, 0, 0, 160000, 1, 0x18, 1250 },
	{ 0, 960000, ACPU_PLL_3, 0, 0, 0, 0, 160000, 1, 0x19, 1250 },
	{ 1, 998400, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1A, 1250 },
	{ 0, 1036800, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1B, 1275 },
	{ 0, 1075200, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1C, 1300 },
	{ 1, 1113600, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1D, 1300 },
	{ 0, 1152000, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1E, 1325 },
	{ 1, 1190400, ACPU_PLL_3, 0, 0, 0, 0, 259200, 1, 0x1F, 1350 },
	{ 1, 1228800, ACPU_PLL_3, 0, 0, 0, 0, 259200, 1, 0x20, 1350 },
	{ 1, 1267200, ACPU_PLL_3, 0, 0, 0, 0, 259200, 1, 0x21, 1425 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};
#else
struct clkctl_acpu_speed acpu_freq_tbl_1228[] = {
	{ 0, 19200, ACPU_PLL_TCXO, 0, 0, 0, 0, 14000, 0, 0, 1000 },
	{ 0, 192000, ACPU_PLL_1, 1, 5, 0, 0, 14000, 2, 0, 1000 },
	{ 1, 245760, ACPU_PLL_0, 4, 0, 0, 0, 29000, 0, 0, 1000 },
	{ 1, 384000, ACPU_PLL_3, 0, 0, 0, 0, 58000, 1, 0xA, 1000 },
	{ 0, 422400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xB, 1000 },
	{ 0, 460800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xC, 1000 },
	{ 0, 499200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xD, 1050 },
	{ 0, 537600, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xE, 1050 },
	{ 1, 576000, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0xF, 1050 },
	{ 0, 614400, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x10, 1075 },
	{ 0, 652800, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x11, 1100 },
	{ 0, 691200, ACPU_PLL_3, 0, 0, 0, 0, 117000, 1, 0x12, 1125 },
	{ 0, 729600, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x13, 1150 },
	{ 1, 768000, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x14, 1150 },
	{ 0, 806400, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x15, 1175 },
	{ 0, 844800, ACPU_PLL_3, 0, 0, 0, 0, 128000, 1, 0x16, 1200 },
	{ 0, 883200, ACPU_PLL_3, 0, 0, 0, 0, 160000, 1, 0x17, 1225 },
	{ 0, 921600, ACPU_PLL_3, 0, 0, 0, 0, 160000, 1, 0x18, 1275 },
	{ 0, 960000, ACPU_PLL_3, 0, 0, 0, 0, 160000, 1, 0x19, 1275 },
	{ 1, 998400, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1A, 1300 },
	{ 0, 1036800, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1B, 1300 },
	{ 0, 1075200, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1C, 1325 },
	{ 1, 1113600, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1D, 1325 },
	{ 0, 1152000, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1E, 1325 },
	{ 1, 1190400, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x1F, 1350 },
        { 1, 1228800, ACPU_PLL_3, 0, 0, 0, 0, 192000, 1, 0x20, 1375 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};
#endif

static struct clkctl_acpu_speed *acpu_freq_tbl = acpu_freq_tbl_1228;
#define AXI_S	(&acpu_freq_tbl[1])
#define PLL0_S	(&acpu_freq_tbl[2])
#define POWER_COLLAPSE_KHZ 192000
#define WAIT_FOR_IRQ_KHZ 245760

#ifdef CONFIG_CPU_FREQ_MSM
static struct cpufreq_frequency_table freq_table[20];
static void __init cpufreq_table_init(void)
{
	unsigned int i;
	unsigned int freq_cnt = 0;
	for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0
			&& freq_cnt < ARRAY_SIZE(freq_table)-1; i++) {
		if (acpu_freq_tbl[i].use_for_scaling) {
			freq_table[freq_cnt].index = freq_cnt;
			freq_table[freq_cnt].frequency
				= acpu_freq_tbl[i].acpuclk_khz;
			freq_cnt++;
		}
	}

	freq_table[freq_cnt].index = freq_cnt;
	freq_table[freq_cnt].frequency = CPUFREQ_TABLE_END;
}
#endif
struct clock_state {
	struct clkctl_acpu_speed	*current_speed;
	struct mutex			lock;
	uint32_t			acpu_switch_time_us;
	uint32_t			max_speed_delta_khz;
	uint32_t			vdd_switch_time_us;
	unsigned int			max_vdd;
	int (*acpu_set_vdd) (int mvolts);
};
static struct clock_state drv_state = { 0 };
unsigned long clk_get_max_axi_khz(void)
{
	return 192000;
}
EXPORT_SYMBOL(clk_get_max_axi_khz);

static void scpll_set_freq(uint32_t lval, unsigned freq_switch)
{
	uint32_t regval;
	if (lval > 33)
		lval = 33;
	if (lval < 10)
		lval = 10;

	while (readl(SCPLL_STATUS_ADDR) & 0x3);
	regval = readl(SCPLL_FSM_CTL_EXT_ADDR);
	regval &= ~(0x3f << 3);
	regval |= (lval << 3);

	if (freq_switch == SIMPLE_SLEW)
		regval |= (0x1 << 9);

	regval &= ~(0x3 << 0);
	regval |= (freq_switch << 0);
	writel(regval, SCPLL_FSM_CTL_EXT_ADDR);
	dmb();
	regval = readl(SCPLL_CTL_ADDR);
	regval |= 0x7;
	writel(regval, SCPLL_CTL_ADDR);
	dmb();
	while (readl(SCPLL_STATUS_ADDR) & 0x1);
	udelay(63);
}
static void scpll_apps_enable(bool state)
{
	uint32_t regval;
	while (readl(SCPLL_STATUS_ADDR) & 0x1);
	regval = readl(SCPLL_CTL_ADDR);
	regval &= ~(0x7);
	regval |= (0x2);
	writel(regval, SCPLL_CTL_ADDR);
	dmb();
	if (state) {
		regval = readl(SCPLL_CTL_ADDR);
		regval |= (0x7);
		writel(regval, SCPLL_CTL_ADDR);
		udelay(180);
	} else {
		regval = readl(SCPLL_CTL_ADDR);
		regval &= ~(0x7);
		writel(regval, SCPLL_CTL_ADDR);
	}
	udelay(45);
}

static void scpll_init(void)
{
	uint32_t regval;
	writel(0x0, SCPLL_CTL_ADDR);
	dmb();
	writel(0x00400002, SCPLL_CTL_ADDR);
	writel(0x00600004, SCPLL_CTL_ADDR);
	dmb();
	while (readl(SCPLL_STATUS_ADDR) & 0x2);
	scpll_apps_enable(1);
	regval = readl(SCPLL_FSM_CTL_EXT_ADDR);
	regval &= ~(0x3f << 3);
	regval |= (L_VAL_384MHZ << 3);
	regval &= ~0x7;
	regval |= SHOT_SWITCH;
	writel(regval, SCPLL_FSM_CTL_EXT_ADDR);
	regval = readl(SCPLL_CTL_ADDR);
	regval |= (0x7);
	writel(regval, SCPLL_CTL_ADDR);
	while (readl(SCPLL_STATUS_ADDR) & 0x1);
	udelay(450);
	regval = readl(SCPLL_FSM_CTL_EXT_ADDR);
	regval &= ~(0x3f << 3);
	regval |= (L_VAL_768MHZ << 3);
	regval &= ~0x7;
	regval |= HOP_SWITCH;
	writel(regval, SCPLL_FSM_CTL_EXT_ADDR);
	regval = readl(SCPLL_CTL_ADDR);
	regval |= (0x7);
	writel(regval, SCPLL_CTL_ADDR);
	while (readl(SCPLL_STATUS_ADDR) & 0x1);
	udelay(63);
	scpll_apps_enable(0);
}

static void config_pll(struct clkctl_acpu_speed *s)
{
	uint32_t regval;

	if (s->pll == ACPU_PLL_3)
		scpll_set_freq(s->sc_l_value, HOP_SWITCH);
	else if (s->sc_core_src_sel_mask == 0) {
		regval = readl(SPSS_CLK_SEL_ADDR) & 0x1;
		switch (regval) {
		case 0x0:
			regval = readl(SPSS_CLK_CNTL_ADDR);
			regval &= ~(0x7 << 4 | 0xf);
			regval |= (s->acpuclk_src_sel << 4);
			regval |= (s->acpuclk_src_div << 0);
			writel(regval, SPSS_CLK_CNTL_ADDR);
			regval = readl(SPSS_CLK_SEL_ADDR);
			regval |= 0x1;
			writel(regval, SPSS_CLK_SEL_ADDR);
			break;

		case 0x1:
			regval = readl(SPSS_CLK_CNTL_ADDR);
			regval &= ~(0x7 << 12 | 0xf << 8);
			regval |= (s->acpuclk_src_sel << 12);
			regval |= (s->acpuclk_src_div << 8);
			writel(regval, SPSS_CLK_CNTL_ADDR);
			regval = readl(SPSS_CLK_SEL_ADDR);
			regval &= ~0x1;
			writel(regval, SPSS_CLK_SEL_ADDR);
			break;
		}
		dmb();
	}
	regval = readl(SPSS_CLK_SEL_ADDR);
	regval &= ~(0x3 << 1);
	regval |= (s->sc_core_src_sel_mask << 1);
	writel(regval, SPSS_CLK_SEL_ADDR);
}

static int acpuclk_set_vdd_level(int vdd)
{
	if (drv_state.acpu_set_vdd) {
		return drv_state.acpu_set_vdd(vdd);
	} else {
		return 0;
	}
}

int acpuclk_set_rate(unsigned long rate, enum setrate_reason reason)
{
	struct clkctl_acpu_speed *tgt_s, *strt_s;
	int res, rc = 0;
	int freq_index = 0;

	if (reason == SETRATE_CPUFREQ)
		mutex_lock(&drv_state.lock);

	strt_s = drv_state.current_speed;

	if (rate == strt_s->acpuclk_khz)
		goto out;

	for (tgt_s = acpu_freq_tbl; tgt_s->acpuclk_khz != 0; tgt_s++) {
		if (tgt_s->acpuclk_khz == rate)
			break;
		freq_index++;
	}

	if (tgt_s->acpuclk_khz == 0) {
		rc = -EINVAL;
		goto out;
	}

	if (reason == SETRATE_CPUFREQ) {
		if (tgt_s->vdd > strt_s->vdd) {
			rc = acpuclk_set_vdd_level(tgt_s->vdd);
			if (rc) {
				pr_err("Unable to increase ACPU vdd (%d)\n",
					rc);
				goto out;
			}
		}
	} else if (reason == SETRATE_PC
		&& rate != POWER_COLLAPSE_KHZ) {
		config_pll(PLL0_S);
	}

	if (strt_s->pll != ACPU_PLL_3 && tgt_s->pll != ACPU_PLL_3) {
		config_pll(tgt_s);
	} else if (strt_s->pll != ACPU_PLL_3 && tgt_s->pll == ACPU_PLL_3) {
		scpll_apps_enable(1);
		config_pll(tgt_s);
	} else if (strt_s->pll == ACPU_PLL_3 && tgt_s->pll != ACPU_PLL_3) {
		config_pll(tgt_s);
		scpll_apps_enable(0);
	} else {
		config_pll(PLL0_S);
		config_pll(tgt_s);
	}

	drv_state.current_speed = tgt_s;
	loops_per_jiffy = tgt_s->lpj;

	if (reason == SETRATE_SWFI)
		goto out;

	if (strt_s->axiclk_khz != tgt_s->axiclk_khz) {
		res = ebi1_clk_set_min_rate(CLKVOTE_ACPUCLK, tgt_s->axiclk_khz * 1000);
	}

	if (reason == SETRATE_PC)
		goto out;

	if (tgt_s->vdd < strt_s->vdd) {
		res = acpuclk_set_vdd_level(tgt_s->vdd);
		if (res)
			pr_warning("Unable to drop ACPU vdd (%d)\n", res);
	}
out:
	if (reason == SETRATE_CPUFREQ)
		mutex_unlock(&drv_state.lock);
	return rc;
}

static void __init acpuclk_init(void)
{
	struct clkctl_acpu_speed *speed;
	uint32_t div, sel, regval;
	int res;

	regval = readl(SPSS_CLK_SEL_ADDR);
	switch ((regval & 0x6) >> 1) {
	case 0:
	case 3:
		if (regval & 0x1) {
			sel = ((readl(SPSS_CLK_CNTL_ADDR) >> 4) & 0x7);
			div = ((readl(SPSS_CLK_CNTL_ADDR) >> 0) & 0xf);
		} else {
			sel = ((readl(SPSS_CLK_CNTL_ADDR) >> 12) & 0x7);
			div = ((readl(SPSS_CLK_CNTL_ADDR) >> 8) & 0xf);
		}

		for (speed = acpu_freq_tbl; speed->acpuclk_khz != 0; speed++) {
			if (speed->acpuclk_src_sel == sel &&
			    speed->acpuclk_src_div == div)
				break;
		}
		break;

	case 1:
		sel = ((readl(SCPLL_FSM_CTL_EXT_ADDR) >> 3) & 0x3f);

		for (speed = acpu_freq_tbl; speed->acpuclk_khz != 0; speed++) {
			if (speed->sc_l_value == sel &&
			    speed->sc_core_src_sel_mask == 1)
				break;
		}
		break;

	case 2:
		speed = AXI_S;
		break;
	default:
		BUG();
	}

	if (speed->pll != ACPU_PLL_3)
		scpll_init();

	if (speed->acpuclk_khz == 0) {
		pr_err("Error - ACPU clock reports invalid speed\n");
		return;
	}

	drv_state.current_speed = speed;
	res = ebi1_clk_set_min_rate(CLKVOTE_ACPUCLK, speed->axiclk_khz * 1000);
}

unsigned long acpuclk_get_rate(void)
{
	return drv_state.current_speed->acpuclk_khz;
}

uint32_t acpuclk_get_switch_time(void)
{
	return drv_state.acpu_switch_time_us;
}

unsigned long acpuclk_power_collapse(void)
{
	int ret = acpuclk_get_rate();
	acpuclk_set_rate(POWER_COLLAPSE_KHZ, SETRATE_PC);
	return ret;
}

unsigned long acpuclk_wait_for_irq(void)
{
	int ret = acpuclk_get_rate();
	acpuclk_set_rate(WAIT_FOR_IRQ_KHZ, SETRATE_SWFI);
	return ret;
}

#define CT_CSR_PHYS		0xA8700000
#define TCSR_SPARE2_ADDR	(ct_csr_base + 0x60)
#define PLL0_M_VAL_ADDR		(MSM_CLK_CTL_BASE + 0x308)

static void __init acpu_freq_tbl_fixup(void)
{
	void __iomem *ct_csr_base;
	uint32_t tcsr_spare2, pll0_m_val;
	unsigned int max_acpu_khz;
	unsigned int i;

	ct_csr_base = ioremap(CT_CSR_PHYS, PAGE_SIZE);
	BUG_ON(ct_csr_base == NULL);
	tcsr_spare2 = readl(TCSR_SPARE2_ADDR);

	if ((tcsr_spare2 & 0xF000) != 0xA000) {
		pr_info("Efuse data on Max ACPU freq not present.\n");
		goto skip_efuse_fixup;
	}

	switch (tcsr_spare2 & 0xF0) {
	case 0x30:
	case 0x00:
		max_acpu_khz = 1267200;
		break;
	default:
		goto skip_efuse_fixup;
	}

	pr_info("Max ACPU freq from efuse data is %d KHz\n", max_acpu_khz);

	for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0; i++) {
		if (acpu_freq_tbl[i].acpuclk_khz > max_acpu_khz) {
			acpu_freq_tbl[i].acpuclk_khz = 0;
			break;
		}
	}

skip_efuse_fixup:
	iounmap(ct_csr_base);
	pll0_m_val = readl(PLL0_M_VAL_ADDR) & 0x7FFFF;
	if (pll0_m_val == 36)
		PLL0_S->acpuclk_khz = 235930;

	for (i = 0; acpu_freq_tbl[i].acpuclk_khz != 0; i++) {
		if (acpu_freq_tbl[i].vdd > drv_state.max_vdd) {
			acpu_freq_tbl[i].acpuclk_khz = 0;
			break;
		}
	}
}

static void __init lpj_init(void)
{
	int i;
	const struct clkctl_acpu_speed *base_clk = drv_state.current_speed;
	for (i = 0; acpu_freq_tbl[i].acpuclk_khz; i++) {
		acpu_freq_tbl[i].lpj = cpufreq_scale(loops_per_jiffy,
						base_clk->acpuclk_khz,
						acpu_freq_tbl[i].acpuclk_khz);
	}
}

void __init msm_acpu_clock_init(struct msm_acpu_clock_platform_data *clkdata)
{
	mutex_init(&drv_state.lock);
	drv_state.acpu_switch_time_us = clkdata->acpu_switch_time_us;
	drv_state.max_speed_delta_khz = clkdata->max_speed_delta_khz;
	drv_state.vdd_switch_time_us = clkdata->vdd_switch_time_us;
	drv_state.max_vdd = clkdata->max_vdd;
	drv_state.acpu_set_vdd = clkdata->acpu_set_vdd;
	acpu_freq_tbl_fixup();
	acpuclk_init();
	lpj_init();

	if (drv_state.current_speed->acpuclk_khz < PLL0_S->acpuclk_khz)
		acpuclk_set_rate(PLL0_S->acpuclk_khz, SETRATE_CPUFREQ);
#ifdef CONFIG_CPU_FREQ_MSM
	cpufreq_table_init();
	cpufreq_frequency_table_get_attr(freq_table, smp_processor_id());
#endif
}

#ifdef CONFIG_CPU_FREQ_VDD_LEVELS
ssize_t acpuclk_get_vdd_levels_str(char *buf)
{
	int i, len = 0;
	if (buf)
	{
		mutex_lock(&drv_state.lock);
		for (i = 0; acpu_freq_tbl[i].acpuclk_khz; i++)
		{
			len += sprintf(buf + len, "%8u: %4d\n", acpu_freq_tbl[i].acpuclk_khz, acpu_freq_tbl[i].vdd);
		}
		mutex_unlock(&drv_state.lock);
	}
	return len;
}

void acpuclk_set_vdd(unsigned int khz, int vdd)
{
	int i;
	unsigned int new_vdd;
	vdd = vdd / V_STEP * V_STEP;
	mutex_lock(&drv_state.lock);
	for (i = 0; acpu_freq_tbl[i].acpuclk_khz; i++)
	{
		if (khz == 0)
			new_vdd = min(max((unsigned int)(acpu_freq_tbl[i].vdd + vdd), SEMC_ACPU_MIN_UV_MV), SEMC_ACPU_MAX_UV_MV);
		else if (acpu_freq_tbl[i].acpuclk_khz == khz)
			new_vdd = min(max((unsigned int)vdd, SEMC_ACPU_MIN_UV_MV), SEMC_ACPU_MAX_UV_MV);
		else continue;
		acpu_freq_tbl[i].vdd = new_vdd;

	}
	mutex_unlock(&drv_state.lock);
}
#endif

