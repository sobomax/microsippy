#include <sys/param.h>
#include <strings.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "esp_log.h"

#include "usipy_debug.h"
#include "usipy_types.h"
#include "usipy_str.h"
#include "usipy_sip_tm.h"
#include "usipy_msg_heap.h"
#include "usipy_sip_sline.h"
#include "usipy_sip_msg.h"
#include "usipy_sip_req.h"
#include "usipy_sip_tid.h"

#define MAX_UDP_SIZE 1472 /* MTU 1500, no fragmentation */

#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"

#include "usipy_esp8266_timer1.h"

static unsigned int
tsdiff(unsigned int bts, unsigned int ets)
{
    unsigned int r;

    if (ets <= bts)
        return (bts - ets);
    r = (unsigned int)T1VMAX - ets;
    return (r + bts + 1);
}

#include <string.h>

#define TIME_HDR_PARSE(hm, to) do { \
        bts = timer1_read(); \
        rval = usipy_sip_msg_parse_hdrs(msg, hm, to); \
        ets = timer1_read(); \
        ESP_LOGI(cfp->log_tag, "usipy_sip_msg_parse_hdrs(" #hm ", " #to ") = %d: took %u cycles", \
          rval, tsdiff(bts, ets)); \
    } while (0);

void
usipy_sip_tm_task(void *pvParameters)
{
    char rx_buffer[MAX_UDP_SIZE];
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    const struct usipy_sip_tm_conf *cfp;

    cfp = (struct usipy_sip_tm_conf *)pvParameters;
    timer1_enable(TIM_DIV1, TIM_EDGE, TIM_LOOP);
    timer1_write(0xffffffff);
    while (1) {
        union {
            struct sockaddr_in v4;
#ifdef IPPROTO_IPV6
            struct sockaddr_in6 v6;
#endif
        } destAddr;
        if (cfp->sip_af == AF_INET) {
            destAddr.v4.sin_addr.s_addr = htonl(INADDR_ANY);
            destAddr.v4.sin_family = AF_INET;
            destAddr.v4.sin_port = htons(cfp->sip_port);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
            inet_ntoa_r(destAddr.v4.sin_addr, addr_str, sizeof(addr_str) - 1);
	} else {
#ifdef IPPROTO_IPV6
            bzero(&destAddr.v6.sin6_addr.un, sizeof(destAddr.v6.sin6_addr.un));
            destAddr.v6.sin6_family = AF_INET6;
            destAddr.v6.sin6_port = htons(cfp->sip_port);
            addr_family = AF_INET6;
            ip_protocol = IPPROTO_IPV6;
            inet6_ntoa_r(destAddr.v6.sin6_addr, addr_str, sizeof(addr_str) - 1);
#else
            ESP_LOGE(cfp->log_tag, "IPv6 is NOT compiled in");
            break;
#endif
        }

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(cfp->log_tag, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(cfp->log_tag, "Socket created");

        int err;
        if (cfp->sip_af == AF_INET) {
            err = bind(sock, (struct sockaddr *)&destAddr.v4, sizeof(destAddr.v4));
        } else {
#ifdef IPPROTO_IPV6
            err = bind(sock, (struct sockaddr *)&destAddr.v6, sizeof(destAddr.v6));
#else
	    err = -1;
	    errno = EAFNOSUPPORT;
#endif
        }
        if (err < 0) {
            ESP_LOGE(cfp->log_tag, "Socket unable to bind: errno %d", errno);
            break;
        }
        ESP_LOGI(cfp->log_tag, "Socket binded");

        while (1) {

            ESP_LOGI(cfp->log_tag, "Waiting for data");
            union {
                struct sockaddr_in v4;
#ifdef IPPROTO_IPV6
                struct sockaddr_in6 v6;
#endif
            } sourceAddr;

            socklen_t socklen;
            if (cfp->sip_af == AF_INET) {
                socklen = sizeof(sourceAddr.v4);
#ifdef IPPROTO_IPV6
            } else {
                socklen = sizeof(sourceAddr.v6);
#endif
            }
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);

            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(cfp->log_tag, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (sourceAddr.v4.sin_family == AF_INET) {
                    inet_ntoa_r(sourceAddr.v4.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
#ifdef IPPROTO_IPV6
                } else if (sourceAddr.v6.sin6_family == AF_INET6) {
                    inet6_ntoa_r(sourceAddr.v6.sin6_addr, addr_str, sizeof(addr_str) - 1);
#endif
                } else {
                    ESP_LOGE(cfp->log_tag, "recvfrom unknown AF: %d", sourceAddr.v4.sin_family);
                    break;
                }
                ESP_LOGI(cfp->log_tag, "Received %d bytes from %s:", len, addr_str);
		ESP_LOGI(cfp->log_tag, "%.*s", len, rx_buffer);

                struct usipy_msg_parse_err cerror = USIPY_MSG_PARSE_ERR_init;
                unsigned int bts, ets;
                bts = timer1_read();
                struct usipy_msg *msg = usipy_sip_msg_ctor_fromwire(rx_buffer, len, &cerror);
                ets = timer1_read();
                ESP_LOGI(cfp->log_tag, "usipy_sip_msg_ctor_fromwire() = %p: took tsdiff(%u, %u) = %u cycles",
                  msg, bts, ets, tsdiff(bts, ets));
#if 0
                ESP_LOGI(cfp->log_tag, "timer1_enabled() = %u, timer1_read() = %u:%u",
                  timer1_enabled(), timer1_read(), timer1_read());
#endif
                if (msg == NULL)
                    continue;

                int rval;

                struct usipy_sip_tid tid;
                bts = timer1_read();
                rval = usipy_sip_msg_get_tid(msg, &tid);
                ets = timer1_read();
                ESP_LOGI(cfp->log_tag, "usipy_sip_msg_get_tid() = %d: took %u cycles", rval,
                  tsdiff(bts, ets));
                if (rval == 0) {
                    usipy_sip_tid_dump(&tid, cfp->log_tag, "  tid.");
                }

                TIME_HDR_PARSE(USIPY_HF_TID_MASK, 1);
                usipy_sip_msg_dtor(msg);
                msg = usipy_sip_msg_ctor_fromwire(rx_buffer, len, &cerror);
                if (msg == NULL) {
                    USIPY_DABORT();
                    continue;
                }
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_VIA), 0);
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_CONTACT), 0);
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_CSEQ), 0);
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_CALLID), 0);
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_TO), 0);
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_FROM), 0);
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_ROUTE), 0);
                TIME_HDR_PARSE(USIPY_HFT_MASK(USIPY_HF_RECORDROUTE), 0);

                if (msg->kind == USIPY_SIP_MSG_REQ) {
                    bts = timer1_read();
                    rval = usipy_sip_req_parse_ruri(msg);
                    ets = timer1_read();
                    ESP_LOGI(cfp->log_tag, "usipy_sip_req_parse_ruri() = %d: took %u cycles", rval,
                      tsdiff(bts, ets));
                }

                int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&sourceAddr, socklen);

                usipy_sip_msg_dump(msg, cfp->log_tag);
                usipy_sip_msg_dtor(msg);

                if (err < 0) {
                    ESP_LOGE(cfp->log_tag, "Error occured during sending: errno %d", errno);
                    break;
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(cfp->log_tag, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}