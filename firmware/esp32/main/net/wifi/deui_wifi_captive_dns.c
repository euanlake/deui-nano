#include "deui_wifi_internal.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

static const char *TAG = "deui_wifi_dns";

static void dns_server_task(void *arg) {
  int socket_fd = (int)(intptr_t)arg;
  char buffer[512];
  const uint32_t portal_addr = ipaddr_addr(DEUI_WIFI_PORTAL_IP);

  while (true) {
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);
    int len = recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
    if (len < 0) {
      if (errno == EBADF || errno == ENOTSOCK) {
        break;
      }
      continue;
    }
    if (len < 12 || (buffer[2] & 0x80) != 0 || (len + 16) > (int)sizeof(buffer)) {
      continue;
    }

    buffer[2] |= 0x80;
    buffer[3] |= 0x80;
    buffer[7] = 1;
    memcpy(&buffer[len], "\xc0\x0c", 2);
    len += 2;
    memcpy(&buffer[len], "\x00\x01\x00\x01\x00\x00\x00\x1c\x00\x04", 10);
    len += 10;
    memcpy(&buffer[len], &portal_addr, 4);
    len += 4;
    (void)sendto(socket_fd, buffer, (size_t)len, 0, (struct sockaddr *)&client_addr, client_len);
  }

  g_deui_wifi.dns_task = NULL;
  vTaskDelete(NULL);
}

esp_err_t deui_wifi_start_captive_dns(void) {
  int socket_fd;
  struct sockaddr_in server_addr = {0};

  if (g_deui_wifi.dns_task != NULL && g_deui_wifi.dns_socket >= 0) {
    return ESP_OK;
  }

  socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (socket_fd < 0) {
    return ESP_FAIL;
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(53);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    close(socket_fd);
    return ESP_FAIL;
  }
  if (xTaskCreate(dns_server_task, "deui_wifi_dns", 4096, (void *)(intptr_t)socket_fd, 5,
                  &g_deui_wifi.dns_task) != pdPASS) {
    close(socket_fd);
    return ESP_ERR_NO_MEM;
  }
  g_deui_wifi.dns_socket = socket_fd;
  g_deui_wifi.dns_running = true;
  ESP_LOGI(TAG, "Captive DNS started on %s:53", DEUI_WIFI_PORTAL_IP);
  return ESP_OK;
}

void deui_wifi_stop_captive_dns(void) {
  if (g_deui_wifi.dns_socket >= 0) {
    shutdown(g_deui_wifi.dns_socket, SHUT_RDWR);
    close(g_deui_wifi.dns_socket);
    g_deui_wifi.dns_socket = -1;
  }
  g_deui_wifi.dns_running = false;
}
