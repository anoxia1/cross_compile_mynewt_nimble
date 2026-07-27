/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "host/ble_hs.h"
#include "os/util.h"
#include "store/config/ble_store_config.h"
#include "ble_store_config_priv.h"

#if MYNEWT_VAL(BLE_STORE_PERSIST_FILE)
/* Forward declarations for file-based persistence */
int ble_store_config_save_to_file(void);
int ble_store_config_load_from_file(void);
#endif
/* From ble_hs_pvcy.c (no public header) */
int ble_hs_pvcy_add_entry(const uint8_t *addr, uint8_t addr_type, const uint8_t *irk);

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
struct ble_store_value_sec
    ble_store_config_our_secs[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
#endif
int ble_store_config_num_our_secs;

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
struct ble_store_value_sec
    ble_store_config_peer_secs[MYNEWT_VAL(BLE_STORE_MAX_BONDS)];
#endif

int ble_store_config_num_peer_secs;

#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)
struct ble_store_value_cccd
    ble_store_config_cccds[MYNEWT_VAL(BLE_STORE_MAX_CCCDS)];
#endif

int ble_store_config_num_cccds;

/*****************************************************************************
 * $sec                                                                      *
 *****************************************************************************/

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
static void
ble_store_config_print_value_sec(const struct ble_store_value_sec *sec)
{
    if (sec->ltk_present) {
        BLE_HS_LOG(DEBUG, "ediv=%u rand=%llu authenticated=%d ltk=",
                       sec->ediv, sec->rand_num, sec->authenticated);
        ble_hs_log_flat_buf(sec->ltk, 16);
        BLE_HS_LOG(DEBUG, " ");
    }
    if (sec->irk_present) {
        BLE_HS_LOG(DEBUG, "irk=");
        ble_hs_log_flat_buf(sec->irk, 16);
        BLE_HS_LOG(DEBUG, " ");
    }
    if (sec->csrk_present) {
        BLE_HS_LOG(DEBUG, "csrk=");
        ble_hs_log_flat_buf(sec->csrk, 16);
        BLE_HS_LOG(DEBUG, " ");
    }

    BLE_HS_LOG(DEBUG, "\n");
}
#endif

static void
ble_store_config_print_key_sec(const struct ble_store_key_sec *key_sec)
{
    if (ble_addr_cmp(&key_sec->peer_addr, BLE_ADDR_ANY)) {
        BLE_HS_LOG(DEBUG, "peer_addr_type=%d peer_addr=",
                       key_sec->peer_addr.type);
        ble_hs_log_flat_buf(key_sec->peer_addr.val, 6);
        BLE_HS_LOG(DEBUG, " ");
    }
}

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
static int
ble_store_config_find_sec(const struct ble_store_key_sec *key_sec,
                          const struct ble_store_value_sec *value_secs,
                          int num_value_secs)
{
    const struct ble_store_value_sec *cur;
    int i;

    if (!ble_addr_cmp(&key_sec->peer_addr, BLE_ADDR_ANY)) {
        if (key_sec->idx < num_value_secs) {
            return key_sec->idx;
        }
    } else if (key_sec->idx == 0) {
        for (i = 0; i < num_value_secs; i++) {
            cur = &value_secs[i];

            if (!ble_addr_cmp(&cur->peer_addr, &key_sec->peer_addr)) {
                return i;
            }
        }
    }

    return -1;
}
#endif

static int
ble_store_config_read_our_sec(const struct ble_store_key_sec *key_sec,
                              struct ble_store_value_sec *value_sec)
{
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    int idx;

    idx = ble_store_config_find_sec(key_sec, ble_store_config_our_secs,
                                    ble_store_config_num_our_secs);
    if (idx == -1) {
        return BLE_HS_ENOENT;
    }

    *value_sec = ble_store_config_our_secs[idx];
    return 0;
#else
    return BLE_HS_ENOENT;
#endif
}


static int
ble_store_config_write_our_sec(const struct ble_store_value_sec *value_sec)
{
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    struct ble_store_key_sec key_sec;
    int idx;
    int rc;

    BLE_HS_LOG(DEBUG, "persisting our sec; ");
    ble_store_config_print_value_sec(value_sec);

    ble_store_key_from_value_sec(&key_sec, value_sec);
    idx = ble_store_config_find_sec(&key_sec, ble_store_config_our_secs,
                                    ble_store_config_num_our_secs);
    if (idx == -1) {
        if (ble_store_config_num_our_secs >= MYNEWT_VAL(BLE_STORE_MAX_BONDS)) {
            BLE_HS_LOG(DEBUG, "error persisting our sec; too many entries "
                              "(%d)\n", ble_store_config_num_our_secs);
            return BLE_HS_ESTORE_CAP;
        }

        idx = ble_store_config_num_our_secs;
        ble_store_config_num_our_secs++;
    }

    ble_store_config_our_secs[idx] = *value_sec;

    rc = ble_store_config_persist_our_secs();
    if (rc != 0) {
        return rc;
    }

    return 0;
#else
    return BLE_HS_ENOENT;
#endif

}

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
static int
ble_store_config_delete_obj(void *values, int value_size, int idx,
                            int *num_values)
{
    uint8_t *dst;
    uint8_t *src;
    int move_count;

    (*num_values)--;
    if (idx < *num_values) {
        dst = values;
        dst += idx * value_size;
        src = dst + value_size;

        move_count = *num_values - idx;
        memmove(dst, src, move_count * value_size);
    }

    return 0;
}

static int
ble_store_config_delete_sec(const struct ble_store_key_sec *key_sec,
                            struct ble_store_value_sec *value_secs,
                            int *num_value_secs)
{
    int idx;
    int rc;

    idx = ble_store_config_find_sec(key_sec, value_secs, *num_value_secs);
    if (idx == -1) {
        return BLE_HS_ENOENT;
    }

    rc = ble_store_config_delete_obj(value_secs, sizeof *value_secs, idx,
                                  num_value_secs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
#endif

static int
ble_store_config_delete_our_sec(const struct ble_store_key_sec *key_sec)
{
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    int rc;

    assert(ble_store_config_num_our_secs <= ARRAY_SIZE(ble_store_config_our_secs));
    rc = ble_store_config_delete_sec(key_sec, ble_store_config_our_secs,
                                     &ble_store_config_num_our_secs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_store_config_persist_our_secs();
    if (rc != 0) {
        return rc;
    }

    return 0;
#else
    return BLE_HS_ENOENT;
#endif
}

static int
ble_store_config_delete_peer_sec(const struct ble_store_key_sec *key_sec)
{
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    int rc;

    assert(ble_store_config_num_peer_secs <= ARRAY_SIZE(ble_store_config_peer_secs));
    rc = ble_store_config_delete_sec(key_sec, ble_store_config_peer_secs,
                                  &ble_store_config_num_peer_secs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_store_config_persist_peer_secs();
    if (rc != 0) {
        return rc;
    }
    return 0;
#else
    return BLE_HS_ENOENT;
#endif
}

static int
ble_store_config_read_peer_sec(const struct ble_store_key_sec *key_sec,
                               struct ble_store_value_sec *value_sec)
{
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    int idx;

    idx = ble_store_config_find_sec(key_sec, ble_store_config_peer_secs,
                             ble_store_config_num_peer_secs);
    if (idx == -1) {
        return BLE_HS_ENOENT;
    }

    *value_sec = ble_store_config_peer_secs[idx];
    return 0;
#else
    return BLE_HS_ENOENT;
#endif

}

static int
ble_store_config_write_peer_sec(const struct ble_store_value_sec *value_sec)
{
#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    struct ble_store_key_sec key_sec;
    int idx;
    int rc;

    BLE_HS_LOG(DEBUG, "persisting peer sec; ");
    ble_store_config_print_value_sec(value_sec);

    ble_store_key_from_value_sec(&key_sec, value_sec);
    idx = ble_store_config_find_sec(&key_sec, ble_store_config_peer_secs,
                                 ble_store_config_num_peer_secs);
    if (idx == -1) {
        if (ble_store_config_num_peer_secs >= MYNEWT_VAL(BLE_STORE_MAX_BONDS)) {
            BLE_HS_LOG(DEBUG, "error persisting peer sec; too many entries "
                             "(%d)\n", ble_store_config_num_peer_secs);
            return BLE_HS_ESTORE_CAP;
        }

        idx = ble_store_config_num_peer_secs;
        ble_store_config_num_peer_secs++;
    }

    ble_store_config_peer_secs[idx] = *value_sec;

    rc = ble_store_config_persist_peer_secs();
    if (rc != 0) {
        return rc;
    }
    return 0;
#else
    return BLE_HS_ENOENT;
#endif
}

/*****************************************************************************
 * $cccd                                                                     *
 *****************************************************************************/

#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)
static int
ble_store_config_find_cccd(const struct ble_store_key_cccd *key)
{
    struct ble_store_value_cccd *cccd;
    int skipped;
    int i;

    skipped = 0;
    for (i = 0; i < ble_store_config_num_cccds; i++) {
        cccd = ble_store_config_cccds + i;

        if (ble_addr_cmp(&key->peer_addr, BLE_ADDR_ANY)) {
            if (ble_addr_cmp(&cccd->peer_addr, &key->peer_addr)) {
                continue;
            }
        }

        if (key->chr_val_handle != 0) {
            if (cccd->chr_val_handle != key->chr_val_handle) {
                continue;
            }
        }

        if (key->idx > skipped) {
            skipped++;
            continue;
        }

        return i;
    }
    return -1;
}
#endif

static int
ble_store_config_delete_cccd(const struct ble_store_key_cccd *key_cccd)
{
#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)
    int idx;
    int rc;

    idx = ble_store_config_find_cccd(key_cccd);
    if (idx < 0) {
        return BLE_HS_ENOENT;
    }

    assert(ble_store_config_num_cccds < ARRAY_SIZE(ble_store_config_cccds));
    rc = ble_store_config_delete_obj(ble_store_config_cccds,
                                     sizeof *ble_store_config_cccds,
                                     idx,
                                     &ble_store_config_num_cccds);
    if (rc != 0) {
        return rc;
    }

    rc = ble_store_config_persist_cccds();
    if (rc != 0) {
        return rc;
    }
    return 0;
#else
    return BLE_HS_ENOENT;
#endif
}

static int
ble_store_config_read_cccd(const struct ble_store_key_cccd *key_cccd,
                           struct ble_store_value_cccd *value_cccd)
{
#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)
    int idx;

    idx = ble_store_config_find_cccd(key_cccd);
    if (idx == -1) {
        return BLE_HS_ENOENT;
    }

    *value_cccd = ble_store_config_cccds[idx];
    return 0;
#else
    return BLE_HS_ENOENT;
#endif
}

static int
ble_store_config_write_cccd(const struct ble_store_value_cccd *value_cccd)
{
#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)
    struct ble_store_key_cccd key_cccd;
    int idx;
    int rc;

    ble_store_key_from_value_cccd(&key_cccd, value_cccd);
    idx = ble_store_config_find_cccd(&key_cccd);
    if (idx == -1) {
        if (ble_store_config_num_cccds >= MYNEWT_VAL(BLE_STORE_MAX_CCCDS)) {
            BLE_HS_LOG(DEBUG, "error persisting cccd; too many entries (%d)\n",
                       ble_store_config_num_cccds);
            return BLE_HS_ESTORE_CAP;
        }

        idx = ble_store_config_num_cccds;
        ble_store_config_num_cccds++;
    }

    ble_store_config_cccds[idx] = *value_cccd;

    rc = ble_store_config_persist_cccds();
    if (rc != 0) {
        return rc;
    }

    return 0;
#else
    return BLE_HS_ENOENT;
#endif
}

/*****************************************************************************
 * $api                                                                      *
 *****************************************************************************/

/**
 * Searches the database for an object matching the specified criteria.
 *
 * @return                      0 if a key was found; else BLE_HS_ENOENT.
 */
int
ble_store_config_read(int obj_type, const union ble_store_key *key,
                      union ble_store_value *value)
{
    int rc;

    switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
        /* An encryption procedure (bonding) is being attempted.  The nimble
         * stack is asking us to look in our key database for a long-term key
         * corresponding to the specified ediv and random number.
         *
         * Perform a key lookup and populate the context object with the
         * result.  The nimble stack will use this key if this function returns
         * success.
         */
        BLE_HS_LOG(DEBUG, "looking up peer sec; ");
        ble_store_config_print_key_sec(&key->sec);
        BLE_HS_LOG(DEBUG, "\n");
        rc = ble_store_config_read_peer_sec(&key->sec, &value->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_OUR_SEC:
        BLE_HS_LOG(DEBUG, "looking up our sec; ");
        ble_store_config_print_key_sec(&key->sec);
        BLE_HS_LOG(DEBUG, "\n");
        rc = ble_store_config_read_our_sec(&key->sec, &value->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_CCCD:
        rc = ble_store_config_read_cccd(&key->cccd, &value->cccd);
        return rc;

    default:
        return BLE_HS_ENOTSUP;
    }
}

/**
 * Adds the specified object to the database.
 *
 * @return                      0 on success;
 *                              BLE_HS_ESTORE_CAP if the database is full.
 */
int
ble_store_config_write(int obj_type, const union ble_store_value *val)
{
    int rc;

    switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
        rc = ble_store_config_write_peer_sec(&val->sec);
        break;

    case BLE_STORE_OBJ_TYPE_OUR_SEC:
        rc = ble_store_config_write_our_sec(&val->sec);
        break;

    case BLE_STORE_OBJ_TYPE_CCCD:
        rc = ble_store_config_write_cccd(&val->cccd);
        break;

    default:
        return BLE_HS_ENOTSUP;
    }

    if (rc == 0) {
        ble_store_config_save_to_file();
    }
    return rc;
}

int
ble_store_config_delete(int obj_type, const union ble_store_key *key)
{
    int rc;

    switch (obj_type) {
    case BLE_STORE_OBJ_TYPE_PEER_SEC:
        rc = ble_store_config_delete_peer_sec(&key->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_OUR_SEC:
        rc = ble_store_config_delete_our_sec(&key->sec);
        return rc;

    case BLE_STORE_OBJ_TYPE_CCCD:
        rc = ble_store_config_delete_cccd(&key->cccd);
        return rc;

    default:
        return BLE_HS_ENOTSUP;
    }
}

void
ble_store_config_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    ble_hs_cfg.store_read_cb = ble_store_config_read;
    ble_hs_cfg.store_write_cb = ble_store_config_write;
    ble_hs_cfg.store_delete_cb = ble_store_config_delete;

    /* Re-initialize BSS values in case of unit tests. */
    ble_store_config_num_our_secs = 0;
    ble_store_config_num_peer_secs = 0;
    ble_store_config_num_cccds = 0;

    ble_store_config_conf_init();

#if MYNEWT_VAL(BLE_STORE_PERSIST_FILE)
    ble_store_config_load_from_file();
#endif
}

#if MYNEWT_VAL(BLE_STORE_PERSIST_FILE)
/*
 * File-based persistence for Linux (no Mynewt config backend).
 * Written once at connection teardown, loaded at startup before NimBLE init.
 */
#define BLE_STORE_CONFIG_FILE_PATH   MYNEWT_VAL(BLE_STORE_PERSIST_FILE_PATH)
#define BOND_FILE_MAGIC              0x4E424C45  /* "NBLE" */
#define BOND_FILE_VERSION            1

/**
 * @brief 绑定文件二进制头部
 */
typedef struct {
    uint32_t magic;         /**< 魔数 0x4E424C45 ("NBLE") */
    uint32_t version;       /**< 格式版本，当前为 1 */
    uint32_t num_our_secs;  /**< our_sec 条目数 */
    uint32_t num_peer_secs; /**< peer_sec 条目数 */
    uint32_t num_cccds;     /**< cccd 条目数 */
    uint32_t reserved[3];   /**< 保留字段 (未来扩展) */
} bond_file_header_t;

int
ble_store_config_save_to_file(void)
{
    bond_file_header_t hdr;
    FILE *f;

    f = fopen(BLE_STORE_CONFIG_FILE_PATH, "wb");
    if (f == NULL) {
        return -1;
    }

    hdr.magic         = BOND_FILE_MAGIC;
    hdr.version       = BOND_FILE_VERSION;
    hdr.num_our_secs  = ble_store_config_num_our_secs;
    hdr.num_peer_secs = ble_store_config_num_peer_secs;
    hdr.num_cccds     = ble_store_config_num_cccds;
    fwrite(&hdr, sizeof(hdr), 1, f);

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    fwrite(ble_store_config_our_secs,  sizeof(*ble_store_config_our_secs),  hdr.num_our_secs,  f);
    fwrite(ble_store_config_peer_secs, sizeof(*ble_store_config_peer_secs), hdr.num_peer_secs, f);
#endif
#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)
    fwrite(ble_store_config_cccds, sizeof(*ble_store_config_cccds), hdr.num_cccds, f);
#endif

    fclose(f);
    fprintf(stderr, "[STORE] saved %u our, %u peer, %u cccd\n",
            hdr.num_our_secs, hdr.num_peer_secs, hdr.num_cccds);
    return 0;
}

int
ble_store_config_load_from_file(void)
{
    bond_file_header_t hdr;
    FILE *f;

    f = fopen(BLE_STORE_CONFIG_FILE_PATH, "rb");
    if (f == NULL) {
        return -1;
    }

    if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != BOND_FILE_MAGIC || hdr.version != BOND_FILE_VERSION) {
        fclose(f);
        return -1;
    }

#if MYNEWT_VAL(BLE_STORE_MAX_BONDS)
    if (hdr.num_our_secs  > MYNEWT_VAL(BLE_STORE_MAX_BONDS) ||
        hdr.num_peer_secs > MYNEWT_VAL(BLE_STORE_MAX_BONDS)) {
        fclose(f);
        return -1;
    }
    ble_store_config_num_our_secs  = hdr.num_our_secs;
    ble_store_config_num_peer_secs = hdr.num_peer_secs;
    fread(ble_store_config_our_secs,  sizeof(*ble_store_config_our_secs),  hdr.num_our_secs,  f);
    fread(ble_store_config_peer_secs, sizeof(*ble_store_config_peer_secs), hdr.num_peer_secs, f);
#endif

#if MYNEWT_VAL(BLE_STORE_MAX_CCCDS)
    if (hdr.num_cccds > MYNEWT_VAL(BLE_STORE_MAX_CCCDS)) {
        fclose(f);
        return -1;
    }
    ble_store_config_num_cccds = hdr.num_cccds;
    fread(ble_store_config_cccds, sizeof(*ble_store_config_cccds), hdr.num_cccds, f);
#endif

    fclose(f);
    fprintf(stderr, "[STORE] loaded %u our, %u peer, %u cccd\n",
            hdr.num_our_secs, hdr.num_peer_secs, hdr.num_cccds);
    return 0;
}

int
ble_store_config_read_num_peer_secs(void)
{
    return ble_store_config_num_peer_secs;
}

/*
 * Re-populate the privacy resolving list from stored peer bonds.
 * Must be called AFTER NimBLE host is running (e.g. in sync_cb).
 */
void ble_store_config_pvcy_repopulate(void)
{
    for (int i = 0; i < ble_store_config_num_peer_secs; i++) {
        ble_hs_pvcy_add_entry(ble_store_config_peer_secs[i].peer_addr.val,
                              ble_store_config_peer_secs[i].peer_addr.type,
                              ble_store_config_peer_secs[i].irk);
    }
}
#else
/* Stubs when persistence is disabled */
int ble_store_config_load_from_file(void) { return -1; }
void ble_store_config_pvcy_repopulate(void) {}
#endif /* BLE_STORE_PERSIST_FILE */
