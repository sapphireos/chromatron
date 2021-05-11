// Copyright 2017-2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _PROVISIONER_MAIN_H_
#define _PROVISIONER_MAIN_H_

#include "net.h"
#include "mesh_bearer_adapt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_MESH_INVALID_NODE_INDEX     0xFFFF
#define BLE_MESH_NODE_NAME_SIZE         31

/* Each node information stored by provisioner */
struct bt_mesh_node {
    /* Device information */
    u8_t  addr[6];      /* Node device address */
    u8_t  addr_type;    /* Node device address type */
    u8_t  dev_uuid[16]; /* Node Device UUID */
    u16_t oob_info;     /* Node OOB information */

    /* Provisioning information */
    u16_t unicast_addr; /* Node unicast address */
    u8_t  element_num;  /* Node element number */
    u16_t net_idx;      /* Node NetKey Index */
    u8_t  flags;        /* Node key refresh flag and iv update flag */
    u32_t iv_index;     /* Node IV Index */
    u8_t  dev_key[16];  /* Node device key */

    /* Additional information */
    char  name[BLE_MESH_NODE_NAME_SIZE + 1]; /* Node name */
    u16_t comp_length;  /* Length of Composition Data */
    u8_t *comp_data;    /* Value of Composition Data */
} __packed;

int bt_mesh_provisioner_init(void);

int bt_mesh_provisioner_net_create(void);

int bt_mesh_provisioner_deinit(bool erase);

bool bt_mesh_provisioner_check_is_addr_dup(u16_t addr, u8_t elem_num, bool comp_with_own);

u16_t bt_mesh_provisioner_get_node_count(void);

int bt_mesh_provisioner_restore_node_info(struct bt_mesh_node *node);

int bt_mesh_provisioner_provision(const bt_mesh_addr_t *addr, const u8_t uuid[16],
                                  u16_t oob_info, u16_t unicast_addr,
                                  u8_t element_num, u16_t net_idx,
                                  u8_t flags, u32_t iv_index,
                                  const u8_t dev_key[16], u16_t *index);

int bt_mesh_provisioner_remove_node(const u8_t uuid[16]);

int bt_mesh_provisioner_restore_node_name(u16_t addr, const char *name);

int bt_mesh_provisioner_restore_node_comp_data(u16_t addr, const u8_t *data, u16_t length);

struct bt_mesh_node *bt_mesh_provisioner_get_node_with_uuid(const u8_t uuid[16]);

struct bt_mesh_node *bt_mesh_provisioner_get_node_with_addr(u16_t unicast_addr);

int bt_mesh_provisioner_delete_node_with_uuid(const u8_t uuid[16]);

int bt_mesh_provisioner_delete_node_with_node_addr(u16_t unicast_addr);

int bt_mesh_provisioner_delete_node_with_dev_addr(const bt_mesh_addr_t *addr);

int bt_mesh_provisioner_set_node_name(u16_t index, const char *name);

const char *bt_mesh_provisioner_get_node_name(u16_t index);

u16_t bt_mesh_provisioner_get_node_index(const char *name);

struct bt_mesh_node *bt_mesh_provisioner_get_node_with_name(const char *name);

const struct bt_mesh_node **bt_mesh_provisioner_get_node_table_entry(void);

int bt_mesh_provisioner_store_node_comp_data(u16_t addr, const u8_t *data, u16_t length);

const u8_t *bt_mesh_provisioner_net_key_get(u16_t net_idx);

struct bt_mesh_subnet *bt_mesh_provisioner_subnet_get(u16_t net_idx);

bool bt_mesh_provisioner_check_msg_dst(u16_t dst);

const u8_t *bt_mesh_provisioner_dev_key_get(u16_t dst);

struct bt_mesh_app_key *bt_mesh_provisioner_app_key_find(u16_t app_idx);

int bt_mesh_provisioner_local_app_key_add(const u8_t app_key[16],
                                          u16_t net_idx, u16_t *app_idx);

int bt_mesh_provisioner_local_app_key_update(const u8_t app_key[16],
                                             u16_t net_idx, u16_t app_idx);

const u8_t *bt_mesh_provisioner_local_app_key_get(u16_t net_idx, u16_t app_idx);

int bt_mesh_provisioner_local_app_key_del(u16_t net_idx, u16_t app_idx, bool store);

int bt_mesh_provisioner_local_net_key_add(const u8_t net_key[16], u16_t *net_idx);

int bt_mesh_provisioner_local_net_key_update(const u8_t net_key[16], u16_t net_idx);

const u8_t *bt_mesh_provisioner_local_net_key_get(u16_t net_idx);

int bt_mesh_provisioner_local_net_key_del(u16_t net_idx, bool store);

/* Provisioner bind local client model with proper appkey index */
int bt_mesh_provisioner_bind_local_model_app_idx(u16_t elem_addr, u16_t mod_id,
                                                 u16_t cid, u16_t app_idx);

typedef void (* bt_mesh_heartbeat_recv_cb_t)(u16_t hb_src, u16_t hb_dst,
                                             u8_t init_ttl, u8_t rx_ttl,
                                             u8_t hops, u16_t feat, s8_t rssi);

int bt_mesh_provisioner_recv_heartbeat(bt_mesh_heartbeat_recv_cb_t cb);

int bt_mesh_provisioner_set_heartbeat_filter_type(u8_t filter_type);

int bt_mesh_provisioner_set_heartbeat_filter_info(u8_t op, u16_t src, u16_t dst);

void bt_mesh_provisioner_heartbeat(u16_t hb_src, u16_t hb_dst,
                                   u8_t init_ttl, u8_t rx_ttl,
                                   u8_t hops, u16_t feat, s8_t rssi);

/* Provisioner print own element information */
int bt_mesh_print_local_composition_data(void);

int bt_mesh_provisioner_store_node_info(struct bt_mesh_node *node);

#ifdef __cplusplus
}
#endif

#endif /* _PROVISIONER_MAIN_H_ */
