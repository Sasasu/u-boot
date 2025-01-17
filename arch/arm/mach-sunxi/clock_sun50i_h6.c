#include <common.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#ifndef CONFIG_MACH_SUN8I_T113
#include <asm/arch/prcm.h>
#endif

#ifdef CONFIG_SPL_BUILD
void clock_init_safe(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
#ifndef CONFIG_MACH_SUN8I_T113
	struct sunxi_prcm_reg *const prcm =
		(struct sunxi_prcm_reg *)SUNXI_PRCM_BASE;

	if (IS_ENABLED(CONFIG_MACH_SUN50I_H616)) {
		/* this seems to enable PLLs on H616 */
		setbits_le32(&prcm->sys_pwroff_gating, 0x10);
		setbits_le32(&prcm->res_cal_ctrl, 2);
	}

	clrbits_le32(&prcm->res_cal_ctrl, 1);
	setbits_le32(&prcm->res_cal_ctrl, 1);

	if (IS_ENABLED(CONFIG_MACH_SUN50I_H6)) {
		/* set key field for ldo enable */
		setbits_le32(&prcm->pll_ldo_cfg, 0xA7000000);
		/* set PLL VDD LDO output to 1.14 V */
		setbits_le32(&prcm->pll_ldo_cfg, 0x60000);
	}
#endif

	clock_set_pll1(408000000);

	writel(CCM_PLL6_DEFAULT, &ccm->pll6_cfg);
	while (!(readl(&ccm->pll6_cfg) & CCM_PLL6_LOCK))
		;

	clrsetbits_le32(&ccm->cpu_axi_cfg, CCM_CPU_AXI_APB_MASK | CCM_CPU_AXI_AXI_MASK,
			CCM_CPU_AXI_DEFAULT_FACTORS);

	writel(CCM_PSI_AHB1_AHB2_DEFAULT, &ccm->psi_ahb1_ahb2_cfg);
#ifndef CONFIG_MACH_SUN8I_T113
	writel(CCM_AHB3_DEFAULT, &ccm->ahb3_cfg);
#endif
	writel(CCM_APB1_DEFAULT, &ccm->apb1_cfg);

	/*
	 * The mux and factor are set, but the clock will be enabled in
	 * DRAM initialization code.
	 */
	writel(MBUS_CLK_SRC_PLL6X2 | MBUS_CLK_M(3), &ccm->mbus_cfg);
}
#endif

void clock_init_uart(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	/* uart clock source is apb2 */
	writel(APB2_CLK_SRC_OSC24M|
	       APB2_CLK_RATE_N_1|
	       APB2_CLK_RATE_M(1),
	       &ccm->apb2_cfg);

	/* open the clock for uart */
	setbits_le32(&ccm->uart_gate_reset,
		     1 << (CONFIG_CONS_INDEX - 1));

	/* deassert uart reset */
	setbits_le32(&ccm->uart_gate_reset,
		     1 << (RESET_SHIFT + CONFIG_CONS_INDEX - 1));
}

#ifdef CONFIG_SPL_BUILD
void clock_set_pll1(unsigned int clk)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	u32 val;

	/* Do not support clocks < 288MHz as they need factor P */
	if (clk < 288000000) clk = 288000000;

	/* Switch to 24MHz clock while changing PLL1 */
	val = readl(&ccm->cpu_axi_cfg);
	val &= ~CCM_CPU_AXI_MUX_MASK;
	val |= CCM_CPU_AXI_MUX_OSC24M;
	writel(val, &ccm->cpu_axi_cfg);

	/* clk = 24*n/p, p is ignored if clock is >288MHz */
	writel(CCM_PLL1_CTRL_EN | CCM_PLL1_LOCK_EN | CCM_PLL1_CLOCK_TIME_2 |
#if defined(CONFIG_MACH_SUN50I_H616)
	       CCM_PLL1_OUT_EN |
#elif defined(CONFIG_MACH_SUN8I_T113)
	       CCM_PLL1_OUT_EN | CCM_PLL1_LDO_EN |
#endif
	       CCM_PLL1_CTRL_N(clk / 24000000), &ccm->pll1_cfg);
	while (!(readl(&ccm->pll1_cfg) & CCM_PLL1_LOCK)) {}

	/* Switch CPU to PLL1 */
	val = readl(&ccm->cpu_axi_cfg);
	val &= ~CCM_CPU_AXI_MUX_MASK;
	val |= CCM_CPU_AXI_MUX_PLL_CPUX;
	writel(val, &ccm->cpu_axi_cfg);
}
#endif

unsigned int clock_get_pll6(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;

	uint32_t rval = readl(&ccm->pll6_cfg);
	int n = ((rval & CCM_PLL6_CTRL_N_MASK) >> CCM_PLL6_CTRL_N_SHIFT) + 1;
#ifndef CONFIG_MACH_SUN8I_T113
		int div1 = ((rval & CCM_PLL6_CTRL_DIV1_MASK) >>
				   CCM_PLL6_CTRL_DIV1_SHIFT) + 1;
	int m = IS_ENABLED(CONFIG_MACH_SUN50I_H6) ? 4 : 2;
#else
	int div1 = ((rval & CCM_PLL6_CTRL_P0_MASK) >>
			   CCM_PLL6_CTRL_P0_SHIFT) + 1;
	int m = 2;
#endif

	int div2 = ((rval & CCM_PLL6_CTRL_DIV2_MASK) >>
			CCM_PLL6_CTRL_DIV2_SHIFT) + 1;
	/* The register defines PLL6-2X or PLL6-4X, not plain PLL6 */
	return 24000000 / m * n / div1 / div2;
}

int clock_twi_onoff(int port, int state)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	u32 value, *ptr;
	int shift;

	value = BIT(GATE_SHIFT) | BIT (RESET_SHIFT);
#ifndef CONFIG_MACH_SUN8I_T113
	if (port == 5) {
		struct sunxi_prcm_reg *const prcm =
			(struct sunxi_prcm_reg *)SUNXI_PRCM_BASE;
		shift = 0;
		ptr = &prcm->twi_gate_reset;
	} else
#endif
	{
		shift = port;
		ptr = &ccm->twi_gate_reset;
	}

	/* set the apb clock gate and reset for twi */
	if (state)
		setbits_le32(ptr, value << shift);
	else
		clrbits_le32(ptr, value << shift);

	return 0;
}
