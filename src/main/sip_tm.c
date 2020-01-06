void
sip_tm_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;
    const struct sip_tm_conf *cfp;

    cfp = (struct sip_tm_conf *)pvParameters;
    while (1) {
        union {
            struct sockaddr_in v4;
            struct sockaddr_in6 v6;
        } destAddr;
        if (cfp->sip_af == AF_INET) {
            destAddr.v4.sin_addr.s_addr = htonl(INADDR_ANY);
            destAddr.v4.sin_family = AF_INET;
            destAddr.v4.sin_port = htons(cfp->sip_port);
            addr_family = AF_INET;
            ip_protocol = IPPROTO_IP;
            inet_ntoa_r(destAddr.v4.sin_addr, addr_str, sizeof(addr_str) - 1);
        } else {
            bzero(&destAddr.v6.sin6_addr.un, sizeof(destAddr.v6.sin6_addr.un));
            destAddr.v6.sin6_family = AF_INET6;
            destAddr.v6.sin6_port = htons(cfp->sip_port);
            addr_family = AF_INET6;
            ip_protocol = IPPROTO_IPV6;
            inet6_ntoa_r(destAddr.v6.sin6_addr, addr_str, sizeof(addr_str) - 1);
        }

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err;
        if (cfp->sip_af == AF_INET) {
            err = bind(sock, (struct sockaddr *)&destAddr.v4, sizeof(destAddr.v4));
        } else {
            err = bind(sock, (struct sockaddr *)&destAddr.v6, sizeof(destAddr.v6));
        }
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket binded");

        while (1) {

            ESP_LOGI(TAG, "Waiting for data");
            union {
                struct sockaddr_in v4;
                struct sockaddr_in6 v6;
            } sourceAddr;

            socklen_t socklen;
            if (cfp->sip_af == AF_INET) {
                socklen = sizeof(sourceAddr.v4);
            } else {
                socklen = sizeof(sourceAddr.v6);
            }
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);

            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (sourceAddr.v4.sin_family == AF_INET) {
                    inet_ntoa_r(sourceAddr.v4.sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (sourceAddr.v6.sin6_family == AF_INET6) {
                    inet6_ntoa_r(sourceAddr.v6.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&sourceAddr, socklen);
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                    break;
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}
