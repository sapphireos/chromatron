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

#include <string.h>
#include <errno.h>

#include "mesh_main.h"
#include "client_common.h"
#include "mesh_common.h"

IRAM_ATTR void *bt_mesh_malloc(size_t size)
{
#ifdef CONFIG_BLE_MESH_MEM_ALLOC_MODE_INTERNAL
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#elif CONFIG_BLE_MESH_ALLOC_FROM_PSRAM_FIRST
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#elif CONFIG_BLE_MESH_MEM_ALLOC_MODE_IRAM_8BIT
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_INTERNAL|MALLOC_CAP_IRAM_8BIT, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}

IRAM_ATTR void *bt_mesh_calloc(size_t size)
{
#ifdef CONFIG_BLE_MESH_MEM_ALLOC_MODE_INTERNAL
    return heap_caps_calloc(1, size, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#elif CONFIG_BLE_MESH_ALLOC_FROM_PSRAM_FIRST
    return heap_caps_calloc_prefer(1, size, 2, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#elif CONFIG_BLE_MESH_MEM_ALLOC_MODE_IRAM_8BIT
    return heap_caps_calloc_prefer(1, size, 2, MALLOC_CAP_INTERNAL|MALLOC_CAP_IRAM_8BIT, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#else
    return calloc(1, size);
#endif
}

IRAM_ATTR void bt_mesh_free(void *ptr)
{
    heap_caps_free(ptr);
}

struct net_buf_simple *bt_mesh_alloc_buf(u16_t size)
{
    struct net_buf_simple *buf = NULL;
    u8_t *data = NULL;

    buf = (struct net_buf_simple *)bt_mesh_calloc(sizeof(struct net_buf_simple) + size);
    if (!buf) {
        BT_ERR("%s, Out of memory", __func__);
        return NULL;
    }

    data = (u8_t *)buf + sizeof(struct net_buf_simple);

    buf->data = data;
    buf->len = 0;
    buf->size = size;
    buf->__buf = data;

    return buf;
}

void bt_mesh_free_buf(struct net_buf_simple *buf)
{
    if (buf) {
        bt_mesh_free(buf);
    }
}

u8_t bt_mesh_get_device_role(struct bt_mesh_model *model, bool srv_send)
{
    bt_mesh_client_user_data_t *client = NULL;

    if (srv_send) {
        BT_DBG("Message is sent by a server model");
        return NODE;
    }

    if (!model || !model->user_data) {
        BT_ERR("%s, Invalid parameter", __func__);
        return ROLE_NVAL;
    }

    client = (bt_mesh_client_user_data_t *)model->user_data;

    return client->msg_role;
}
