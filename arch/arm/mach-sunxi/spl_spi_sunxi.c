// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Siarhei Siamashka <siarhei.siamashka@gmail.com>
 */

#include "spl_spi_sunxi.h"

/*
 * Allwinner A10/A20 SoCs were using pins PC0,PC1,PC2,PC23 for booting
 * from SPI Flash, everything else is using pins PC0,PC1,PC2,PC3.
 * The H6 uses PC0, PC2, PC3, PC5.
 */
void sunxi_spi0_pinmux_setup(unsigned int pin_function)
{
	/* All chips use PC2. */
	sunxi_gpio_set_cfgpin(SUNXI_GPC(2), pin_function);

	if (IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		sunxi_gpio_set_cfgpin(SUNXI_GPC(4), pin_function);
	else
		sunxi_gpio_set_cfgpin(SUNXI_GPC(0), pin_function);

	/* All chips except H6 use PC1, and only H6 uses PC5. */
	if (!IS_ENABLED(CONFIG_MACH_SUN50I_H6) && !IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		sunxi_gpio_set_cfgpin(SUNXI_GPC(1), pin_function);
	else
		sunxi_gpio_set_cfgpin(SUNXI_GPC(5), pin_function);

	/* Older generations use PC23 for CS, newer ones use PC3. */
	if (IS_ENABLED(CONFIG_MACH_SUN4I) || IS_ENABLED(CONFIG_MACH_SUN7I) ||
	    IS_ENABLED(CONFIG_MACH_SUN8I_R40))
		sunxi_gpio_set_cfgpin(SUNXI_GPC(23), pin_function);
	else
		sunxi_gpio_set_cfgpin(SUNXI_GPC(3), pin_function);
}

bool is_sun6i_gen_spi(void)
{
	return IS_ENABLED(CONFIG_SUNXI_GEN_SUN6I) ||
	       IS_ENABLED(CONFIG_MACH_SUN50I_H6);
}

uintptr_t sunxi_spi0_base_address(void)
{
	if (IS_ENABLED(CONFIG_MACH_SUN8I_R40))
		return 0x01C05000;

	if (IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		return 0x04025000;

	if (IS_ENABLED(CONFIG_MACH_SUN50I_H6))
		return 0x05010000;

	if (!is_sun6i_gen_spi() ||
	    IS_ENABLED(CONFIG_MACH_SUNIV))
		return 0x01C05000;

	return 0x01C68000;
}

/*
 * Setup 6 MHz from OSC24M (because the BROM is doing the same).
 */
void sunxi_spi0_enable_clock(void)
{
	uintptr_t base = sunxi_spi0_base_address();

	/* Deassert SPI0 reset on SUN6I */
	if (IS_ENABLED(CONFIG_MACH_SUN50I_H6))
		setbits_le32(CCM_H6_SPI_BGR_REG, (1U << 16) | 0x1);
	else if (IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		setbits_le32(CCM_T113_SPI_BGR_REG, (1U << 16) | 0x1);
	else if (is_sun6i_gen_spi())
		setbits_le32(SUN6I_BUS_SOFT_RST_REG0,
			     (1 << AHB_RESET_SPI0_SHIFT));

	/* Open the SPI0 gate */
	if (!IS_ENABLED(CONFIG_MACH_SUN50I_H6) && !IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		setbits_le32(CCM_AHB_GATING0, (1 << AHB_GATE_OFFSET_SPI0));

	/* Clock divider */
	if (!IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		writel(CONFIG_SPL_SPI_SUNXI_DIV, base + (is_sun6i_gen_spi() ?
		       SUN6I_SPI0_CCTL : SUN4I_SPI0_CCTL));
	/* 24MHz from OSC24M */
	if (!IS_ENABLED(CONFIG_MACH_SUNIV))
		writel((1 << 31), CCM_SPI0_CLK);

	if (is_sun6i_gen_spi() || IS_ENABLED(CONFIG_MACH_SUN8I_T113)) {
		/* Enable SPI in the master mode and do a soft reset */
		setbits_le32(base + SUN6I_SPI0_GCR, SUN6I_CTL_MASTER |
			     SUN6I_CTL_ENABLE | SUN6I_CTL_SRST);
		/* Wait for completion */
		while (readl(base + SUN6I_SPI0_GCR) & SUN6I_CTL_SRST)
			;
	} else {
		/* Enable SPI in the master mode and reset FIFO */
		setbits_le32(base + SUN4I_SPI0_CTL, SUN4I_CTL_MASTER |
						    SUN4I_CTL_ENABLE |
						    SUN4I_CTL_TF_RST |
						    SUN4I_CTL_RF_RST);
	}
}

void sunxi_spi0_disable_clock(void)
{
	uintptr_t base = sunxi_spi0_base_address();

	/* Disable the SPI0 controller */
	if (is_sun6i_gen_spi() || IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		clrbits_le32(base + SUN6I_SPI0_GCR, SUN6I_CTL_MASTER |
					     SUN6I_CTL_ENABLE);
	else
		clrbits_le32(base + SUN4I_SPI0_CTL, SUN4I_CTL_MASTER |
					     SUN4I_CTL_ENABLE);

	/* Disable the SPI0 clock */
	if (!IS_ENABLED(CONFIG_MACH_SUNIV))
		writel(0, CCM_SPI0_CLK);

	/* Close the SPI0 gate */
	if (!IS_ENABLED(CONFIG_MACH_SUN50I_H6) && !IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		clrbits_le32(CCM_AHB_GATING0, (1 << AHB_GATE_OFFSET_SPI0));

	/* Assert SPI0 reset on SUN6I */
	if (IS_ENABLED(CONFIG_MACH_SUN50I_H6))
		clrbits_le32(CCM_H6_SPI_BGR_REG, (1U << 16) | 0x1);
	else if (IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		clrbits_le32(CCM_T113_SPI_BGR_REG, (1U << 16) | 0x1);
	else if (is_sun6i_gen_spi())
		clrbits_le32(SUN6I_BUS_SOFT_RST_REG0,
			     (1 << AHB_RESET_SPI0_SHIFT));
}

void sunxi_spi0_init(void)
{
	unsigned int pin_function = SUNXI_GPC_SPI0;

	if (IS_ENABLED(CONFIG_MACH_SUN50I) ||
	    IS_ENABLED(CONFIG_MACH_SUN50I_H6))
		pin_function = SUN50I_GPC_SPI0;
	else if (IS_ENABLED(CONFIG_MACH_SUNIV))
		pin_function = SUNIV_GPC_SPI0;
	else if (IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		pin_function = SUN8I_T113_GPC_SPI0;

	sunxi_spi0_pinmux_setup(pin_function);
	sunxi_spi0_enable_clock();
}

void sunxi_spi0_deinit(void)
{
	unsigned int pin_function;

	/* New SoCs can disable pins, older could only set them as input */
	if (is_sun6i_gen_spi())
		pin_function = SUNXI_GPIO_DISABLE;
	else if (IS_ENABLED(CONFIG_MACH_SUN8I_T113))
		pin_function = SUN8I_T113_GPIO_DISABLE;
	else
		pin_function = SUNXI_GPIO_INPUT;

	sunxi_spi0_disable_clock();
	sunxi_spi0_pinmux_setup(pin_function);
}
