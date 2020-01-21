#include <sys/param.h>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "esp_log.h"
#include "xtensa/hal.h"

#include "usipy_str.h"
#include "usipy_sip_tm.h"
#include "usipy_msg_heap.h"
#include "usipy_sip_sline.h"
#include "usipy_sip_msg.h"

#define MAX_UDP_SIZE 1472 /* MTU 1500, no fragmentation */

#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"

#define ESP8266_REG(addr) *((volatile uint32_t *)(0x60000000+(addr)))
#define T1I  ESP8266_REG(0x60C) //Interrupt Status Register (1bit) write to clear
#define T1V  ESP8266_REG(0x604) //(RO) Current Value
#define T1C  ESP8266_REG(0x608) //Control Register
#define TCIT  0 //Interrupt Type 0:edge, 1:level
#define TCPD  2 //Prescale Devider (2bit) 0:1(12.5ns/tick), 1:16(0.2us/tick), 2/3:256(3.2us/tick)
#define TCAR  6 //AutoReload (restart timer when condition is reached)
#define TCTE  7 //Timer Enable
#define timer1_read()           (T1V)
#define timer1_enabled()        ((T1C & (1 << TCTE)) != 0)

//timer dividers
enum TIM_DIV_ENUM {
  TIM_DIV1 = 0,   //80MHz (80 ticks/us - 104857.588 us max)
  TIM_DIV16 = 1,  //5MHz (5 ticks/us - 1677721.4 us max)
  TIM_DIV256 = 3 //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
};
//timer reload values
#define TIM_SINGLE      0 //on interrupt routine you need to write a new value to start the timer again
#define TIM_LOOP        1 //on interrupt the counter will start with the same value again

void timer1_enable(uint8_t divider, uint8_t int_type, uint8_t reload){
    T1C = (1 << TCTE) | ((divider & 3) << TCPD) | ((int_type & 1) << TCIT) | ((reload & 1) << TCAR);
    T1I = 0;
}

void timer1_write(uint32_t ticks){
    T1L = ((ticks)& 0x7FFFFF);
#if 0
    if ((T1C & (1 << TCIT)) == 0) TEIE |= TEIE1;//edge int enable
#endif
}

static unsigned int
tsdiff(unsigned int bts, unsigned int ets)
{
    unsigned int r;

    if (bts <= ets)
        return (ets - bts);
    r = (unsigned int)0xffffffff - bts;
    return (r + ets + 1);
}

void
usipy_sip_tm_task(void *pvParameters)
{
    char rx_buffer[MAX_UDP_SIZE];
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    const struct usipy_sip_tm_conf *cfp;

    cfp = (struct usipy_sip_tm_conf *)pvParameters;
    timer1_enable(TIM_DIV1, TIM_LOOP);
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

                int cerror;
                unsigned int bts, ets;
                bts = xthal_get_ccount();
                struct usipy_msg *msg = usipy_sip_msg_ctor_fromwire(rx_buffer, len, &cerror);
                ets = xthal_get_ccount();
                ESP_LOGI(cfp->log_tag, "usipy_sip_msg_ctor_fromwire() = %p: took tsdiff(%u, %u) = %u cycles",
                  msg, bts, ets, tsdiff(bts, ets));
                ESP_LOGI(cfp->log_tag, "timer1_enabled = %u", timer1_enabled());
                if (msg == NULL)
                    continue;
#define USIPY_HF_TID_MASK (USIPY_HFT_MASK(USIPY_HF_CSEQ) | USIPY_HFT_MASK(USIPY_HF_CALLID))
                bts = xthal_get_ccount();
                int rval = usipy_sip_msg_parse_hdrs(msg, USIPY_HF_TID_MASK);
                ets = xthal_get_ccount();
                ESP_LOGI(cfp->log_tag, "usipy_sip_msg_parse_hdrs(USIPY_HF_TID_MASK) = %d: took %u cycles", rval,
                  tsdiff(bts, ets));
                usipy_sip_msg_dump(msg, cfp->log_tag);
                usipy_sip_msg_dtor(msg);

                int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&sourceAddr, socklen);
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
