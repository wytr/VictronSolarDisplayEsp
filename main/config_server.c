/* config_server.c */
#include "config_server.h"
#include "config_storage.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>  // opendir, readdir, closedir

static const char *TAG = "cfg_srv";

// Initialize Wi-Fi AP for configuration
esp_err_t wifi_ap_init(void) {
    ESP_LOGI(TAG, "Initializing NVS for Wi-Fi AP");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Initializing TCP/IP stack");
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_LOGI(TAG, "Creating default event loop");
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "Creating default Wi-Fi AP interface");
    esp_netif_create_default_wifi_ap();

    ESP_LOGI(TAG, "Initializing Wi-Fi driver");
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_LOGI(TAG, "Setting Wi-Fi mode to AP");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "VictronConfig",
            .ssid_len = 0,
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    ESP_LOGI(TAG, "Configuring AP: SSID='%s'", ap_config.ap.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_LOGI(TAG, "Starting Wi-Fi AP");
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi SoftAP started successfully");
    return ESP_OK;
}

// Mount SPIFFS partition and list contents
static void mount_spiffs(void) {
    ESP_LOGI(TAG, "Mounting SPIFFS...");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }
    size_t total = 0, used = 0;
    esp_spiffs_info(conf.partition_label, &total, &used);
    ESP_LOGI(TAG, "SPIFFS mounted. Total: %d, Used: %d", total, used);

    // Debug: list files
    DIR *dir = opendir("/spiffs");
    if (dir) {
        struct dirent *entry;
        ESP_LOGI(TAG, "SPIFFS contents:");
        while ((entry = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "  %s", entry->d_name);
        }
        closedir(dir);
    } else {
        ESP_LOGW(TAG, "Failed to open SPIFFS directory");
    }
}

// Determine MIME type
static const char* get_content_type(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css")  == 0) return "text/css";
    if (strcmp(ext, ".js")   == 0) return "application/javascript";
    if (strcmp(ext, ".png")  == 0) return "image/png";
    if (strcmp(ext, ".jpg")  == 0) return "image/jpeg";
    return "application/octet-stream";
}

// Serve file at given URI from SPIFFS
static esp_err_t serve_from_spiffs(httpd_req_t *req, const char *uri) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "/spiffs%s", uri);
    ESP_LOGI(TAG, "Serving %s", filepath);
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGW(TAG, "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, get_content_type(filepath));
    char buf[512]; size_t r;
    while ((r = fread(buf, 1, sizeof(buf), f))) {
        httpd_resp_send_chunk(req, buf, r);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// Handler for GET /
static esp_err_t handle_root(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET / -> serve index.html");
    return serve_from_spiffs(req, "/index.html");
}

// Static files catch-all
static esp_err_t handle_static(httpd_req_t *req) {
    return serve_from_spiffs(req, req->uri);
}

// Fallback form for POST save
static esp_err_t post_save(httpd_req_t *req) {
    ESP_LOGV(TAG, "HTTP POST /save");
    size_t len = req->content_len;
    if (!len || len > 64) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid length");
        return ESP_FAIL;
    }
    char *body = malloc(len+1);
    if (!body) return ESP_FAIL;
    int ret = httpd_req_recv(req, body, len);
    if (ret <= 0) { free(body); return ESP_FAIL; }
    body[ret] = '\0';
    char *hex = strchr(body, '=');
    if (!hex || strlen(hex+1)!=32) { free(body); return ESP_FAIL; }
    hex++;
    uint8_t key[16];
    for (int i=0;i<16;i++) {
        char tmp[3]={hex[i*2],hex[i*2+1],0};
        key[i]=strtol(tmp,NULL,16);
    }
    ESP_LOGI(TAG,"Parsed AES key:"); ESP_LOG_BUFFER_HEX(TAG,key,16);
    save_aes_key(key);
    free(body);
    httpd_resp_set_type(req,"text/html");
    httpd_resp_send(req,"<h3>Saved. Rebooting...</h3>",HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(100)); esp_restart();
    return ESP_OK;
}

// Start server
esp_err_t config_server_start(void) {
    // Assumes Wi-Fi AP already initialized
    mount_spiffs();
    httpd_handle_t server;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    // Enable wildcard URI matching so static handler catches all paths
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_start(&server,&cfg);
    
    httpd_uri_t uri_root={ .uri="/", .method=HTTP_GET, .handler=handle_root};
    httpd_register_uri_handler(server,&uri_root);

    httpd_uri_t uri_save={ .uri="/save", .method=HTTP_POST, .handler=post_save};
    httpd_register_uri_handler(server,&uri_save);

    httpd_uri_t uri_static={ .uri="/*", .method=HTTP_GET, .handler=handle_static};
    httpd_register_uri_handler(server,&uri_static);

    ESP_LOGI(TAG, "HTTP config server running");
    return ESP_OK;
}
