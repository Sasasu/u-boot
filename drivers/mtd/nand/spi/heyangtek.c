// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Wills Wang <wills.wang@live.com>
 */

#ifndef __UBOOT__
#include <malloc.h>
#include <linux/device.h>
#include <linux/kernel.h>
#endif
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_HEYANGTEK			0xC9

static SPINAND_OP_VARIANTS(hyfxgq4u_read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int hyfxgq4u_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = 32 * section + 8;
	region->length = 24;

	return 0;
}

static int hyfxgq4u_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = 32 * section + 2;
	region->length = 6;

	return 0;
}

static const struct mtd_ooblayout_ops hyfxgq4u_ooblayout = {
	.ecc = hyfxgq4u_ooblayout_ecc,
	.rfree = hyfxgq4u_ooblayout_free,
};

static const struct spinand_info heyangtek_spinand_table[] = {
	SPINAND_INFO("HYF1GQ4U", 0x51,
		     NAND_MEMORG(1, 2048, 128, 64, 1024, 1, 1, 1),
		     NAND_ECCREQ(14, 512),
		     SPINAND_INFO_OP_VARIANTS(&hyfxgq4u_read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&hyfxgq4u_ooblayout, NULL)),

	SPINAND_INFO("HYF2GQ4U", 0x52,
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 1, 1, 1),
		     NAND_ECCREQ(14, 512),
		     SPINAND_INFO_OP_VARIANTS(&hyfxgq4u_read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&hyfxgq4u_ooblayout, NULL)),

	SPINAND_INFO("HYF4GQ4U", 0x54,
		     NAND_MEMORG(1, 2048, 128, 64, 4096, 1, 1, 1),
		     NAND_ECCREQ(14, 512),
		     SPINAND_INFO_OP_VARIANTS(&hyfxgq4u_read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&hyfxgq4u_ooblayout, NULL)),
};

static int heyangtek_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	/*
	 * There is an address byte needed to shift in before IDs
	 * are read out, so the first byte in raw_id is dummy.
	 */
	if (id[1] != SPINAND_MFR_HEYANGTEK)
		return 0;

	ret = spinand_match_and_init(spinand, heyangtek_spinand_table,
				     ARRAY_SIZE(heyangtek_spinand_table),
				     id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops heyangtek_spinand_manuf_ops = {
	.detect = heyangtek_spinand_detect,
};

const struct spinand_manufacturer heyangtek_spinand_manufacturer = {
	.id = SPINAND_MFR_HEYANGTEK,
	.name = "HeYangTek",
	.ops = &heyangtek_spinand_manuf_ops,
};
