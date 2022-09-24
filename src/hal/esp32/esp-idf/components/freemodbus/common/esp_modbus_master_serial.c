/* Copyright 2018 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "esp_err.h"                // for esp_err_t
#include "mbc_master.h"             // for master interface define
#include "esp_modbus_master.h"      // for public slave defines
#include "mbc_serial_master.h"      // for public interface defines

/**
 * Initialization of Modbus master serial
 */
esp_err_t mbc_master_init(mb_port_type_t port_type, void** handler)
{
    void* port_handler = NULL;
    esp_err_t error = ESP_ERR_NOT_SUPPORTED;
    switch(port_type)
    {
    case MB_PORT_SERIAL_MASTER:
        error = mbc_serial_master_create(&port_handler);
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
    if ((port_handler != NULL) && (error == ESP_OK)) {
        mbc_master_init_iface(port_handler);
        *handler = port_handler;
    }
    return error;
}
