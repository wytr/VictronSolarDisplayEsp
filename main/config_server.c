/* config_server.c */
#include "config_server.h"
#include "config_storage.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

static const char *TAG = "cfg_srv";

// NVS namespace for Wi-Fi AP settings
#define WIFI_NAMESPACE    "wifi"

/**
 * @brief   Initialize and/or start the Soft-AP.
 *
 *   - On first call: initializes NVS, TCP/IP stack, event loop and Wi-Fi driver.
 *   - On every call: reads SSID/password/enabled flag from NVS.
 *     • If enabled, configures and starts Soft-AP.
 *     • If disabled, stops Soft-AP.
 */
esp_err_t wifi_ap_init(void)
{
    static bool subsystems_inited = false;
    esp_err_t err;

    // 1) One-time subsystems init
    if (!subsystems_inited) {
        // NVS
        err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGW(TAG, "Erasing NVS and retrying");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        ESP_ERROR_CHECK(err);

        // TCP/IP stack + default event loop
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        // Wi-Fi driver
        wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wcfg));

        subsystems_inited = true;
    }

    // 2) Load SSID/password/enabled from NVS
    char ssid[33] = {0};
    char pass[65] = {0};
    uint8_t enabled = 1;

    nvs_handle_t h;
    if (nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        size_t sl = sizeof(ssid), pl = sizeof(pass);
        if (nvs_get_str(h, "ssid", ssid, &sl) != ESP_OK || sl <= 1) {
            strcpy(ssid, "VictronConfig");
            nvs_set_str(h, "ssid", ssid);
        }
        if (nvs_get_str(h, "password", pass, &pl) != ESP_OK) {
            pass[0] = '\0';
            nvs_set_str(h, "password", pass);
        }
        if (nvs_get_u8(h, "enabled", &enabled) != ESP_OK) {
            enabled = 1;
            nvs_set_u8(h, "enabled", enabled);
        }
        nvs_commit(h);
        nvs_close(h);
    } else {
        ESP_LOGW(TAG, "Failed to open NVS, using defaults");
    }

    // 3) If disabled, stop Soft-AP
    if (!enabled) {
        ESP_LOGI(TAG, "AP disabled → stopping Soft-AP");
        err = esp_wifi_stop();
        if (err == ESP_OK || err == ESP_ERR_WIFI_NOT_STARTED) {
            ESP_LOGI(TAG, "Soft-AP stopped");
            return ESP_OK;
        }
        ESP_LOGE(TAG, "esp_wifi_stop() failed: %s", esp_err_to_name(err));
        return err;
    }

    // 4) Start or restart Soft-AP
    ESP_LOGI(TAG, "Starting Soft-AP, SSID='%s'", ssid);

    // This returns a pointer, not an esp_err_t
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "esp_netif_create_default_wifi_ap() failed");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid_len       = strlen(ssid),
            .max_connection = 4,
            .channel        = 1,
        }
    };
    strncpy((char*)ap_cfg.ap.ssid, ssid, sizeof(ap_cfg.ap.ssid));
    if (pass[0]) {
        strncpy((char*)ap_cfg.ap.password, pass, sizeof(ap_cfg.ap.password));
        ap_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Soft-AP started");
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

// Determine MIME type based on file extension
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

// Serve a file from SPIFFS at the given URI
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

// Fallback form for POST /save (AES key)
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
    if (!hex || strlen(hex+1) != 32) { free(body); return ESP_FAIL; }
    hex++;
    uint8_t key[16];
    for (int i = 0; i < 16; i++) {
        char tmp[3] = { hex[i*2], hex[i*2+1], 0 };
        key[i] = strtol(tmp, NULL, 16);
    }
    ESP_LOGI(TAG, "Parsed AES key:"); ESP_LOG_BUFFER_HEX(TAG, key, 16);
    save_aes_key(key);
    free(body);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h3>Saved. Rebooting...</h3>", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
}

// Start the HTTP configuration server
esp_err_t config_server_start(void) {
    // Assumes Wi-Fi AP already initialized (via wifi_ap_init)
    mount_spiffs();
    httpd_handle_t server;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    // Enable wildcard URI matching so static handler catches all
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_start(&server, &cfg);
    
    httpd_uri_t uri_root = { .uri = "/",    .method = HTTP_GET,  .handler = handle_root };
    httpd_register_uri_handler(server, &uri_root);

    httpd_uri_t uri_save = { .uri = "/save", .method = HTTP_POST, .handler = post_save };
    httpd_register_uri_handler(server, &uri_save);

    httpd_uri_t uri_static = { .uri = "/*",  .method = HTTP_GET,  .handler = handle_static };
    httpd_register_uri_handler(server, &uri_static);

    ESP_LOGI(TAG, "HTTP config server running");
    return ESP_OK;
}
