// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Wills Wang <wills.wang@live.com>
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <env_internal.h>
#include <asm/global_data.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <search.h>
#include <errno.h>
#include <mtd.h>

#include <linux/compat.h>
#include <linux/mtd/mtd.h>
#include <u-boot/crc.h>

#if defined(ENV_IS_EMBEDDED)
static env_t *env_ptr = &environment;
#endif /* ENV_IS_EMBEDDED */

DECLARE_GLOBAL_DATA_PTR;

static int env_mtd_init(void) {
#if defined(ENV_IS_EMBEDDED)
        int crc1_ok = 0, crc2_ok = 0;
        env_t *tmp_env1;

#ifdef CONFIG_ENV_OFFSET_REDUND
        env_t *tmp_env2;

        tmp_env2 = (env_t *)((ulong)env_ptr + CONFIG_ENV_SIZE);
        crc2_ok = crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc;
#endif
        tmp_env1 = env_ptr;
        crc1_ok = crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc;

        if (!crc1_ok && !crc2_ok) {
                gd->env_addr = 0;
                gd->env_valid = ENV_INVALID;

                return 0;
        } else if (crc1_ok && !crc2_ok) {
                gd->env_valid = ENV_VALID;
        }
#ifdef CONFIG_ENV_OFFSET_REDUND
        else if (!crc1_ok && crc2_ok) {
                gd->env_valid = ENV_REDUND;
        } else {
                /* both ok - check serial */
                if (tmp_env1->flags == 255 && tmp_env2->flags == 0)
                        gd->env_valid = ENV_REDUND;
                else if (tmp_env2->flags == 255 && tmp_env1->flags == 0)
                        gd->env_valid = ENV_VALID;
                else if (tmp_env1->flags > tmp_env2->flags)
                        gd->env_valid = ENV_VALID;
                else if (tmp_env2->flags > tmp_env1->flags)
                        gd->env_valid = ENV_REDUND;
                else /* flags are equal - almost impossible */
                        gd->env_valid = ENV_VALID;
        }

        if (gd->env_valid == ENV_REDUND)
                env_ptr = tmp_env2;
        else
#endif
        if (gd->env_valid == ENV_VALID)
                env_ptr = tmp_env1;

        gd->env_addr = (ulong)env_ptr->data;

#else /* !ENV_IS_EMBEDDED */
        gd->env_valid = ENV_INVALID;
#endif

        return 0;
}

static int writeenv(struct mtd_info *mtd, loff_t offset, u_char *buf) {
        loff_t end = offset + CONFIG_ENV_RANGE;
        size_t amount_saved = 0;
        size_t blocksize, len, rlen = 0;
        u_char *char_ptr;

        blocksize = mtd->erasesize;
        len = min(blocksize, (size_t)CONFIG_ENV_SIZE);
        while (amount_saved < CONFIG_ENV_SIZE && offset < end) {
                if (mtd_block_isbad(mtd, offset)) {
                        offset += blocksize;
                } else {
                        char_ptr = &buf[amount_saved];

                        if (mtd_write(mtd, offset, len, &rlen, char_ptr)) {
                                printf("MTD: write 0x%08x bytes failed at 0x%08llx\n",
                                       len, offset);
                                return 1;
                        }

                        offset += blocksize;
                        amount_saved += rlen;
                }
        }

        if (amount_saved != CONFIG_ENV_SIZE)
                return 2;

        return 0;
}

static int erase_and_write_env(struct mtd_info *mtd, loff_t offset,
                               size_t length, u_char *env_new) {
        struct erase_info erase = { 0 };

        erase.len = length;
        erase.addr = offset;
        erase.mtd = mtd;
        if (mtd_erase(mtd, &erase)) {
                printf("MTD: erase failed at 0x%08llx\n", offset);
                return 1;
        }

        if (writeenv(mtd, offset, (u_char *)env_new))
                return 2;

        return 0;
}

static int env_mtd_save(void) {
        int ret = 0;
        struct mtd_info *mtd;
        ALLOC_CACHE_ALIGN_BUFFER(env_t, env_new, 1);

        ret = env_export(env_new);
        if (ret)
                return ret;

        mtd_probe_devices();
        mtd = __mtd_next_device(0);
        if (!mtd)
                return 1;

        ret = erase_and_write_env(mtd, CONFIG_ENV_OFFSET, CONFIG_ENV_RANGE,
                                  (u_char *)env_new);
#ifdef CONFIG_ENV_OFFSET_REDUND
        if (!ret) {
                /* preset other copy for next write */
                gd->env_valid = gd->env_valid == ENV_REDUND ? ENV_VALID :
                                ENV_REDUND;
                return ret;
        }

        ret = erase_and_write_env(mtd, CONFIG_ENV_OFFSET, CONFIG_ENV_RANGE,
                                  (u_char *)env_new);
        if (!ret)
                printf("Warning: primary env write failed,"
                                " redundancy is lost!\n");
#endif

        return ret;
}

static int readenv(loff_t offset, u_char *buf) {
        loff_t end = offset + CONFIG_ENV_RANGE;
        size_t amount_loaded = 0;
        size_t blocksize, len, rlen;
        struct mtd_info *mtd;
        u_char *char_ptr;

        mtd_probe_devices();
        mtd = __mtd_next_device(0);
        if (!mtd)
                return 1;

        blocksize = mtd->erasesize;
        len = min(blocksize, (size_t)CONFIG_ENV_SIZE);
        while (amount_loaded < CONFIG_ENV_SIZE && offset < end) {
                if (mtd_block_isbad(mtd, offset)) {
                        offset += blocksize;
                } else {
                        char_ptr = &buf[amount_loaded];
                        if (mtd_read(mtd, offset, len, &rlen, char_ptr)) {
                                printf("MTD: read 0x%08x bytes failed at 0x%08llx\n",
                                       len, offset);
                                return 1;
                        }

                        offset += blocksize;
                        amount_loaded += rlen;
                }
        }

        if (amount_loaded != CONFIG_ENV_SIZE)
                return 1;

        return 0;
}

#ifdef CONFIG_ENV_OFFSET_REDUND
static int env_mtd_load(void) {
#if defined(ENV_IS_EMBEDDED)
        return 0;
#else
        int read1_fail, read2_fail;
        env_t *tmp_env1, *tmp_env2;
        int ret = 0;

        tmp_env1 = (env_t *)malloc(CONFIG_ENV_SIZE);
        tmp_env2 = (env_t *)malloc(CONFIG_ENV_SIZE);
        if (tmp_env1 == NULL || tmp_env2 == NULL) {
                puts("Can't allocate buffers for environment\n");
                env_set_default("malloc() failed", 0);
                ret = -EIO;
                goto done;
        }

        read1_fail = readenv(CONFIG_ENV_OFFSET, (u_char *) tmp_env1);
        read2_fail = readenv(CONFIG_ENV_OFFSET_REDUND, (u_char *) tmp_env2);

        ret = env_import_redund((char *)tmp_env1, read1_fail, (char *)tmp_env2,
                                read2_fail, H_EXTERNAL);

done:
        free(tmp_env1);
        free(tmp_env2);

        return ret;
#endif
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
static int env_mtd_load(void) {
#if defined(ENV_IS_EMBEDDED)
        return 0;
#else
        int ret;
        ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);

        ret = readenv(CONFIG_ENV_OFFSET, (u_char *)buf);
        if (ret) {
                env_set_default("readenv() failed", 0);
                return -EIO;
        }

        return env_import(buf, 1, H_EXTERNAL);
#endif
}
#endif /* CONFIG_ENV_OFFSET_REDUND */

U_BOOT_ENV_LOCATION(mtd) = {
        .location       = ENVL_MTD,
        ENV_NAME("MTD")
        .load           = env_mtd_load,
        .save           = env_save_ptr(env_mtd_save),
        .init           = env_mtd_init,
};
