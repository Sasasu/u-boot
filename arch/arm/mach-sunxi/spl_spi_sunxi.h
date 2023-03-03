// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Siarhei Siamashka <siarhei.siamashka@gmail.com>
 */

#include <common.h>
#include <spl.h>
#include <image.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/spl.h>
#include <linux/libfdt.h>
#include <linux/delay.h>

#ifdef CONFIG_SPL_OS_BOOT
#error CONFIG_SPL_OS_BOOT is not supported yet
#endif

/*
 * This is a very simple U-Boot image loading implementation, trying to
 * replicate what the boot ROM is doing when loading the SPL. Because we
 * know the exact pins where the SPI Flash is connected and also know
 * that the Read Data Bytes (03h) command is supported, the hardware
 * configuration is very simple and we don't need the extra flexibility
 * of the SPI framework. Moreover, we rely on the default settings of
 * the SPI controller hardware registers and only adjust what needs to
 * be changed. This is good for the code size and this implementation
 * adds less than 400 bytes to the SPL.
 *
 * There are two variants of the SPI controller in Allwinner SoCs:
 * A10/A13/A20 (sun4i variant) and everything else (sun6i variant).
 * Both of them are supported.
 *
 * The pin mixing part is SoC specific and only A10/A13/A20/H3/A64 are
 * supported at the moment.
 */

/* SUN4I variant of the SPI controller */
#define SUN4I_SPI0_CCTL             0x1C
#define SUN4I_SPI0_CTL              0x08
#define SUN4I_SPI0_RX               0x00
#define SUN4I_SPI0_TX               0x04
#define SUN4I_SPI0_FIFO_STA         0x28
#define SUN4I_SPI0_BC               0x20
#define SUN4I_SPI0_TC               0x24

#define SUN4I_CTL_ENABLE            BIT(0)
#define SUN4I_CTL_MASTER            BIT(1)
#define SUN4I_CTL_TF_RST            BIT(8)
#define SUN4I_CTL_RF_RST            BIT(9)
#define SUN4I_CTL_XCH               BIT(10)

/* SUN6I variant of the SPI controller */
#define SUN6I_SPI0_CCTL             0x24
#define SUN6I_SPI0_GCR              0x04
#define SUN6I_SPI0_TCR              0x08
#define SUN6I_SPI0_FIFO_STA         0x1C
#define SUN6I_SPI0_MBC              0x30
#define SUN6I_SPI0_MTC              0x34
#define SUN6I_SPI0_BCC              0x38
#define SUN6I_SPI0_TXD              0x200
#define SUN6I_SPI0_RXD              0x300

#define SUN6I_CTL_ENABLE            BIT(0)
#define SUN6I_CTL_MASTER            BIT(1)
#define SUN6I_CTL_SRST              BIT(31)
#define SUN6I_TCR_XCH               BIT(31)

#define CCM_AHB_GATING0             (0x01C20000 + 0x60)
#define CCM_H6_SPI_BGR_REG          (0x03001000 + 0x96c)
#define CCM_T113_SPI_BGR_REG        (0x02001000 + 0x96c)

#ifdef CONFIG_MACH_SUN50I_H6
#define CCM_SPI0_CLK                (0x03001000 + 0x940)
#elif defined(CONFIG_MACH_SUN8I_T113)
#define CCM_SPI0_CLK                (0x02001000 + 0x940)
#else
#define CCM_SPI0_CLK                (0x01C20000 + 0xA0)
#endif

#define SUN6I_BUS_SOFT_RST_REG0     (0x01C20000 + 0x2C0)

#define AHB_RESET_SPI0_SHIFT        20
#define AHB_GATE_OFFSET_SPI0        20

#define SPI0_CLK_DIV_NONE           0x0000
#define SPI0_CLK_DIV_BY_2           0x1000
#define SPI0_CLK_DIV_BY_4           0x1001
#define SPI0_CLK_DIV_BY_32          0x100f

/*
 * Allwinner A10/A20 SoCs were using pins PC0,PC1,PC2,PC23 for booting
 * from SPI Flash, everything else is using pins PC0,PC1,PC2,PC3.
 * The H6 uses PC0, PC2, PC3, PC5.
 */
void sunxi_spi0_pinmux_setup(unsigned int pin_function);

bool is_sun6i_gen_spi(void);

uintptr_t sunxi_spi0_base_address(void);

/*
 * Setup 6 MHz from OSC24M (because the BROM is doing the same).
 */
void sunxi_spi0_enable_clock(void);

void sunxi_spi0_disable_clock(void);

void sunxi_spi0_init(void);

void sunxi_spi0_deinit(void);
