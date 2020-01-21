/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "usipy_sip_tm.h"

#ifndef EXAMPLE_SIP_PORT
# define EXAMPLE_SIP_PORT CONFIG_EXAMPLE_SIP_PORT
#endif

static const char *TAG = "example";

void app_main(void)
{
#ifdef CONFIG_EXAMPLE_IPV6
    struct usipy_sip_tm_conf stc = {.sip_port = EXAMPLE_SIP_PORT, .sip_af = AF_INET6, .log_tag = TAG};
#else
    struct usipy_sip_tm_conf stc = {.sip_port = EXAMPLE_SIP_PORT, .sip_af = AF_INET, .log_tag = TAG};
#endif

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(usipy_sip_tm_task, "sip_tm", 4096, &stc, 5, NULL);
}
