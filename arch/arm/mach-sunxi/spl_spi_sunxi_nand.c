// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 Benedikt-Alexander Mokro? <u-boot@bamkrs.de>
 */

#include "spl_spi_sunxi.h"

#define DUMMY_BURST_BYTE			0x00
/* 64 bytes FIFO size, minus 4 bytes of the header */
#define SPI_READ_MAX_SIZE			60

struct sunxi_nand_config {
	const char *name;
	u32 jedec_id;
	u32 page_mask;
	u8 page_shift;
	u32 addr_mask;
	u8 addr_shift;
};

/**
 * List of known NANDS. For now, all of them are generic 2k devices.
 */
static struct sunxi_nand_config sunxi_known_nands[] = {
	{
		.name = "Macronix MX35LF1GE4AB",
		.jedec_id = 0x00C212C2, /* MX35LFxGE4AB repeat the C2x2 over and over again */
		.page_mask = 0x00FFFFFF,
		.page_shift = 11,
		.addr_mask = 0x7FF,
		.addr_shift = 0,
	},
	{
		.name = "Macronix MX35LF2GE4AB",
		.jedec_id = 0x00C222C2,
		.page_mask = 0x00FFFFFF,
		.page_shift = 11,
		.addr_mask = 0x7FF,
		.addr_shift = 0,
	},
	{
		.name = "Winbond W25N01GVxxIG",
		.jedec_id = 0x00EFAA21,
		.page_mask = 0x0000FFFF,
		.page_shift = 11,
		.addr_mask = 0x7FF,
		.addr_shift = 0,
	},
	{
		.name = "GigaDevice GD5F1GQ4RCxxG",
		.jedec_id = 0x00C8B148, /* 3.3v */
		.page_mask = 0x00FFFFFF,
		.page_shift = 11,
		.addr_mask = 0x7FF,
		.addr_shift = 0,
	},
	{
		.name = "GigaDevice GD5F1GQ4RCxxG",
		.jedec_id = 0x00C8A148, /* 1.8V */
		.page_mask = 0x00FFFFFF,
		.page_shift = 11,
		.addr_mask = 0x7FF,
		.addr_shift = 0,
	},
	{
		.name = "HeYangTek HYF4GQ4U",
		.jedec_id = 0x00C954C9, /* 3.3V */
		.page_mask = 0x00FFFFFF,
		.page_shift = 11,
		.addr_mask = 0x7FF,
		.addr_shift = 0,
	},

	{ /* Sentinel */
		.name = NULL,
	}
};

/**
 * Generic 2k device-configuration
 */
static struct sunxi_nand_config sunxi_generic_nand_config = {
	.name = "Generic 2K SPI-NAND",
	.jedec_id = 0xFFFFFFFF,
	.page_mask = 0x00FFFFFF,
	.page_shift = 11,
	.addr_mask = 0x7FF,
	.addr_shift = 0,
};

/*
 * Enumerate all known nands and return the config if found.
 * Returns NULL if none is found or if CONFIG_SPL_SPI_SUNXI_NAND_USE_GENERIC2K_ON_UNKNOWN
 * is set, returns a generic configuration compatible to most standard 2k Page-Size SPI-NANDs
 */
static struct sunxi_nand_config *sunxi_spinand_enumerate(u32 jedec_id)
{
	struct sunxi_nand_config *ptr = sunxi_known_nands;

	while (ptr->name) {
		if (jedec_id == ptr->jedec_id)
			return ptr;
		++ptr;
	}

	if (ptr->jedec_id && (ptr->jedec_id != 0xFFFFFFFF))
		return &sunxi_generic_nand_config;

	return NULL;
}

/**
 * Load a page in devices cache
 */
static void sunxi_spi0_load_page(struct sunxi_nand_config *config, u32 addr, ulong spi_ctl_reg,
				 ulong spi_ctl_xch_bitmask,
				 ulong spi_fifo_reg,
				 ulong spi_tx_reg,
				 ulong spi_rx_reg,
				 ulong spi_bc_reg,
				 ulong spi_tc_reg,
				 ulong spi_bcc_reg) {
	/* Read Page in Cache */
	u8 status = 0x01;

	addr = addr >> (config->page_shift);
	addr = addr & config->page_mask;

	writel(4, spi_bc_reg);           /* Burst counter (total bytes) */
	writel(4, spi_tc_reg);           /* Transfer counter (bytes to send) */
	if (spi_bcc_reg)
		writel(4, spi_bcc_reg);  /* SUN6I also needs this */

	/* Send the Read Data Bytes (13h) command header */
	writeb(0x13, spi_tx_reg);
	writeb((u8)(addr >> 16), spi_tx_reg);
	writeb((u8)(addr >> 8),  spi_tx_reg);
	writeb((u8)(addr),       spi_tx_reg);

	/* Start the data transfer */
	setbits_le32(spi_ctl_reg, spi_ctl_xch_bitmask);

	/* Wait till all bytes are send */
	while ((readl(spi_fifo_reg) & 0x7F0000) > 0)
		;

	/* wait till all bytes are read */
	while ((readl(spi_fifo_reg) & 0x7F) < 4)
		;

	/* Discard the 4 empty bytes from our send */
	readl(spi_rx_reg);

	/* Wait until page loaded */
	do {
		/* tCS = 100ns + tRD_ECC 70ns -> 200ns wait */
		ndelay(200);

		/* Poll */
		writel(2 + 1, spi_bc_reg);   /* Burst counter (total bytes) */
		writel(2, spi_tc_reg);       /* Transfer counter (bytes to send) */

		/* SUN6I also needs this */
		if (spi_bcc_reg)
			writel(2, spi_bcc_reg);

		/* Send the Read Status Bytes (0FC0h) command header */
		writeb(0x0F, spi_tx_reg);
		writeb(0xC0, spi_tx_reg);

		/* Start the data transfer */
		setbits_le32(spi_ctl_reg, spi_ctl_xch_bitmask);

		while ((readl(spi_fifo_reg) & 0x7F) < 2 + 1)
			;

		/* skip 2 since we send 2 */
		readb(spi_rx_reg);
		readb(spi_rx_reg);

		status = readb(spi_rx_reg);

	} while ((status & 0x01) == 0x01);
}

static void spi0_load_page(struct sunxi_nand_config *config, u32 addr)
{
	uintptr_t base = sunxi_spi0_base_address();

	if (is_sun6i_gen_spi() || IS_ENABLED(CONFIG_MACH_SUN8I_T113)) {
		sunxi_spi0_load_page(config, addr,
				     base + SUN6I_SPI0_TCR,
				     SUN6I_TCR_XCH,
				     base + SUN6I_SPI0_FIFO_STA,
				     base + SUN6I_SPI0_TXD,
				     base + SUN6I_SPI0_RXD,
				     base + SUN6I_SPI0_MBC,
				     base + SUN6I_SPI0_MTC,
				     base + SUN6I_SPI0_BCC);
	} else {
		sunxi_spi0_load_page(config, addr,
				     base + SUN4I_SPI0_CTL,
				     SUN4I_CTL_XCH,
				     base + SUN4I_SPI0_FIFO_STA,
				     base + SUN4I_SPI0_TX,
				     base + SUN4I_SPI0_RX,
				     base + SUN4I_SPI0_BC,
				     base + SUN4I_SPI0_TC,
				     0);
	}
}

/**
 * Read data from devices cache
 */
static void sunxi_spi0_read_cache(struct sunxi_nand_config *config, u8 *buf, u32 addr, u32 bufsize,
				  ulong spi_ctl_reg,
				  ulong spi_ctl_xch_bitmask,
				  ulong spi_fifo_reg,
				  ulong spi_tx_reg,
				  ulong spi_rx_reg,
				  ulong spi_bc_reg,
				  ulong spi_tc_reg,
				  ulong spi_bcc_reg)
{
	addr = addr >> (config->addr_shift);
	addr = addr & config->addr_mask;

	writel(4 + bufsize, spi_bc_reg); /* Burst counter (total bytes) */
	writel(4, spi_tc_reg);           /* Transfer counter (bytes to send) */

	/* SUN6I also needs this */
	if (spi_bcc_reg)
		writel(4, spi_bcc_reg);

	/* Send the Read Data Bytes (0Bh) command header */
	writeb(0x0B, spi_tx_reg);
	writeb((u8)((addr >> 8)), spi_tx_reg);
	writeb((u8)(addr), spi_tx_reg);
	writeb(DUMMY_BURST_BYTE, spi_tx_reg);

	/* Start the data transfer */
	setbits_le32(spi_ctl_reg, spi_ctl_xch_bitmask);

	/* Wait until everything is received in the RX FIFO */
	while ((readl(spi_fifo_reg) & 0x7F) < 4 + bufsize)
		;

	/* Skip 4 bytes since we send 4 */
	readl(spi_rx_reg);

	/* Read the data */
	while (bufsize-- > 0)
		*buf++ = readb(spi_rx_reg);

	/* tSHSL time is up to 100 ns in various SPI flash datasheets */
	ndelay(100);
}

static void spi0_read_cache(struct sunxi_nand_config *config, void *buf,
			    u32 addr, u32 len)
{
	uintptr_t base = sunxi_spi0_base_address();

	if (is_sun6i_gen_spi() || IS_ENABLED(CONFIG_MACH_SUN8I_T113)) {
			sunxi_spi0_read_cache(config, buf, addr, len,
					      base + SUN6I_SPI0_TCR,
					      SUN6I_TCR_XCH,
					      base + SUN6I_SPI0_FIFO_STA,
					      base + SUN6I_SPI0_TXD,
					      base + SUN6I_SPI0_RXD,
					      base + SUN6I_SPI0_MBC,
					      base + SUN6I_SPI0_MTC,
					      base + SUN6I_SPI0_BCC);
		} else {
			sunxi_spi0_read_cache(config, buf, addr, len,
					      base + SUN4I_SPI0_CTL,
					      SUN4I_CTL_XCH,
					      base + SUN4I_SPI0_FIFO_STA,
					      base + SUN4I_SPI0_TX,
					      base + SUN4I_SPI0_RX,
					      base + SUN4I_SPI0_BC,
					      base + SUN4I_SPI0_TC,
					      0);
		}
}

/**
 * Load (bulk) data to cache an read it
 * Handles chunking to SPI's buffsize
 */
static void spi0_read_data(struct sunxi_nand_config *config, void *buf,
			   u32 addr, u32 len)
{
	u8 *buf8 = buf;
	u32 chunk_len;
	u32 curr_page;
	u32 last_page = (addr >> (config->page_shift)) & config->page_mask;

	/* Load first page */
	spi0_load_page(config, addr);

	while (len > 0) {
		/* Check if new data must be loaded in cache */
		curr_page = (addr >> (config->page_shift)) & config->page_mask;
		if (curr_page > last_page) {
			spi0_load_page(config, addr);
			last_page = curr_page;
		}

		/* Chunk to SPI-Buffers max size */
		chunk_len = len;
		if (chunk_len > SPI_READ_MAX_SIZE) {
			chunk_len = SPI_READ_MAX_SIZE;
		}

		/* Check if chunk length exceeds page */
		if ((((addr + chunk_len) >> (config->page_shift)) & config->page_mask) > curr_page) {
			chunk_len = ((curr_page + 1) << (config->page_shift)) - addr;
		}

		/* Read data from cache */
		spi0_read_cache(config, buf8, addr, chunk_len);
		len  -= chunk_len;
		buf8 += chunk_len;
		addr += chunk_len;
	}
}

/**
 * Read ID Bytes register
 */
static u32 sunxi_spi0_read_id(ulong spi_ctl_reg,
			      ulong spi_ctl_xch_bitmask,
			      ulong spi_fifo_reg,
			      ulong spi_tx_reg,
			      ulong spi_rx_reg,
			      ulong spi_bc_reg,
			      ulong spi_tc_reg,
			      ulong spi_bcc_reg)
{
	u8 idbuf[3];

	writel(2 + 3, spi_bc_reg); /* Burst counter (total bytes) */
	writel(2, spi_tc_reg);     /* Transfer counter (bytes to send) */

	/* SUN6I also needs this */
	if (spi_bcc_reg)
		writel(2, spi_bcc_reg);

	/* Send the Read ID Bytes (9Fh) command header */
	writeb(0x9F, spi_tx_reg);
	writeb(DUMMY_BURST_BYTE, spi_tx_reg);

	/* Start the data transfer */
	setbits_le32(spi_ctl_reg, spi_ctl_xch_bitmask);

	/* Wait until everything is received in the RX FIFO */
	while ((readl(spi_fifo_reg) & 0x7F) < 2 + 3)
		;

	/* Skip 2 bytes */
	readb(spi_rx_reg);
	readb(spi_rx_reg);

	/* Read the data */
	//while (bufsize-- > 0)
	idbuf[0] = readb(spi_rx_reg);
	idbuf[1] = readb(spi_rx_reg);
	idbuf[2] = readb(spi_rx_reg);

	/* tSHSL time is up to 100 ns in various SPI flash datasheets */
	ndelay(100);

	return idbuf[2] | (idbuf[1] << 8) | (idbuf[0] << 16);
}

static u32 spi0_read_id(void)
{
	uintptr_t base = sunxi_spi0_base_address();

	if (is_sun6i_gen_spi() || IS_ENABLED(CONFIG_MACH_SUN8I_T113)) {
		return sunxi_spi0_read_id(base + SUN6I_SPI0_TCR,
					  SUN6I_TCR_XCH,
					  base + SUN6I_SPI0_FIFO_STA,
					  base + SUN6I_SPI0_TXD,
					  base + SUN6I_SPI0_RXD,
					  base + SUN6I_SPI0_MBC,
					  base + SUN6I_SPI0_MTC,
					  base + SUN6I_SPI0_BCC);
	} else {
		return sunxi_spi0_read_id(base + SUN4I_SPI0_CTL,
					  SUN4I_CTL_XCH,
					  base + SUN4I_SPI0_FIFO_STA,
					  base + SUN4I_SPI0_TX,
					  base + SUN4I_SPI0_RX,
					  base + SUN4I_SPI0_BC,
					  base + SUN4I_SPI0_TC,
					  0);
	}
}

static ulong spi_load_read(struct spl_load_info *load, ulong sector, ulong count, void *buf)
{
	spi0_read_data((struct sunxi_nand_config *)load->priv, buf, sector, count);

	return count;
}

static int spl_spi_nand_load_image(struct spl_image_info *spl_image,
				   struct spl_boot_device *bootdev)
{
	int ret = 0;
	u32 id = 0;
	struct sunxi_nand_config *config;
	struct image_header *header = (struct image_header *)(CONFIG_SYS_TEXT_BASE);
	uint32_t load_offset = sunxi_get_spl_size();

	load_offset = max_t(uint32_t, load_offset, CONFIG_SYS_SPI_U_BOOT_OFFS);

	sunxi_spi0_init();
	id = spi0_read_id();

	/*
	 * If we receive only zeros, there is most definetly no device attached.
	 */
	if (id == 0) {
		sunxi_spi0_deinit();
		printf("Received only zeros on jedec_id probe, assuming no spi-nand attached.\n");
		return -1;
	}

	/*
	 * Check if device is known and compatible.
	 */
	config = sunxi_spinand_enumerate(id);
	if (!config) {
		sunxi_spi0_deinit();
		printf("Unknown chip %x\n", id);
		return -1;
	}

	printf("Found %s(%x)\n", config->name, id);

	/*
	 * Read the header data from the image and parse it for validity.
	 */
	spi0_read_data(config, (void *)header, load_offset, 0x40);
	if (image_check_hcrc(header)) {
		/* If it is an FIT-Image, load as FIT, if not, parse header normaly */
		if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(header) == FDT_MAGIC) {
			struct spl_load_info load;

			printf("Found FIT image\n");
			load.dev = NULL;
			load.priv = (void *)config;
			load.filename = NULL;
			load.bl_len = 1;
			load.read = spi_load_read;
			ret = spl_load_simple_fit(spl_image, &load, load_offset, header);
		} else {
			ret = spl_parse_image_header(spl_image, bootdev, header);
			if (!ret)
				spi0_read_data(config, (void *)spl_image->load_addr, load_offset, spl_image->size);
		}
	} else
		printf("U-Boot Image CRC ERROR!\n");
	sunxi_spi0_deinit();
	return ret;
}

/* Use priority 0 to override the default if it happens to be linked in */
SPL_LOAD_IMAGE_METHOD("sunxi SPI-NAND", 0, BOOT_DEVICE_SPI, spl_spi_nand_load_image);
