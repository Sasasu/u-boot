// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Siarhei Siamashka <siarhei.siamashka@gmail.com>
 */

#include "spl_spi_sunxi.h"

#define SPI_READ_MAX_SIZE 60 /* FIFO size, minus 4 bytes of the header */

static void sunxi_spi0_read_data(u8 *buf, u32 addr, u32 bufsize,
				 ulong spi_ctl_reg,
				 ulong spi_ctl_xch_bitmask,
				 ulong spi_fifo_reg,
				 ulong spi_tx_reg,
				 ulong spi_rx_reg,
				 ulong spi_bc_reg,
				 ulong spi_tc_reg,
				 ulong spi_bcc_reg)
{
	writel(4 + bufsize, spi_bc_reg); /* Burst counter (total bytes) */
	writel(4, spi_tc_reg);           /* Transfer counter (bytes to send) */
	if (spi_bcc_reg)
		writel(4, spi_bcc_reg);  /* SUN6I also needs this */

	/* Send the Read Data Bytes (03h) command header */
	writeb(0x03, spi_tx_reg);
	writeb((u8)(addr >> 16), spi_tx_reg);
	writeb((u8)(addr >> 8), spi_tx_reg);
	writeb((u8)(addr), spi_tx_reg);

	/* Start the data transfer */
	setbits_le32(spi_ctl_reg, spi_ctl_xch_bitmask);

	/* Wait until everything is received in the RX FIFO */
	while ((readl(spi_fifo_reg) & 0x7F) < 4 + bufsize)
		;

	/* Skip 4 bytes */
	readl(spi_rx_reg);

	/* Read the data */
	while (bufsize-- > 0)
		*buf++ = readb(spi_rx_reg);

	/* tSHSL time is up to 100 ns in various SPI flash datasheets */
	udelay(1);
}

static void spi0_read_data(void *buf, u32 addr, u32 len)
{
	u8 *buf8 = buf;
	u32 chunk_len;
	uintptr_t base = sunxi_spi0_base_address();

	while (len > 0) {
		chunk_len = len;
		if (chunk_len > SPI_READ_MAX_SIZE)
			chunk_len = SPI_READ_MAX_SIZE;

		if (is_sun6i_gen_spi()) {
			sunxi_spi0_read_data(buf8, addr, chunk_len,
					     base + SUN6I_SPI0_TCR,
					     SUN6I_TCR_XCH,
					     base + SUN6I_SPI0_FIFO_STA,
					     base + SUN6I_SPI0_TXD,
					     base + SUN6I_SPI0_RXD,
					     base + SUN6I_SPI0_MBC,
					     base + SUN6I_SPI0_MTC,
					     base + SUN6I_SPI0_BCC);
		} else {
			sunxi_spi0_read_data(buf8, addr, chunk_len,
					     base + SUN4I_SPI0_CTL,
					     SUN4I_CTL_XCH,
					     base + SUN4I_SPI0_FIFO_STA,
					     base + SUN4I_SPI0_TX,
					     base + SUN4I_SPI0_RX,
					     base + SUN4I_SPI0_BC,
					     base + SUN4I_SPI0_TC,
					     0);
		}

		len  -= chunk_len;
		buf8 += chunk_len;
		addr += chunk_len;
	}
}

static ulong spi_load_read(struct spl_load_info *load, ulong sector,
			   ulong count, void *buf)
{
	spi0_read_data(buf, sector, count);

	return count;
}

static int spl_spi_nor_load_image(struct spl_image_info *spl_image,
				  struct spl_boot_device *bootdev)
{
	int ret = 0;
	struct image_header *header = (struct image_header *)(CONFIG_SYS_TEXT_BASE);
	uint32_t load_offset = sunxi_get_spl_size();

	load_offset = max_t(uint32_t, load_offset, CONFIG_SYS_SPI_U_BOOT_OFFS);

	sunxi_spi0_init();

	spi0_read_data((void *)header, load_offset, 0x40);

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT image\n");
		load.dev = NULL;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spi_load_read;
		ret = spl_load_simple_fit(spl_image, &load,
					  load_offset, header);
	} else {
		ret = spl_parse_image_header(spl_image, bootdev, header);
		if (ret)
			return ret;

		spi0_read_data((void *)spl_image->load_addr,
			       load_offset, spl_image->size);
	}

	sunxi_spi0_deinit();

	return ret;
}

/* Use priority 0 to override the default if it happens to be linked in */
SPL_LOAD_IMAGE_METHOD("sunxi SPI-NOR", 0, BOOT_DEVICE_SPI, spl_spi_nor_load_image);
