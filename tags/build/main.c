/**
 * @file    main.c
 * @brief   ESP32-S3 · WiFi FTM (802.11mc) Responder (Tag / Anchor)
 *
 * Flow:
 * 1. Initialize NVS and WiFi.
 * 2. Configure the ESP32-S3 as a SoftAP (Access Point).
 * 3. Enable the FTM Responder flag in the AP configuration.
 * 4. Wait indefinitely. The initiator (Node) will FTM scan against this AP.
 *
 * Target:   ESP32-S3 · ESP-IDF >= v5.0
 * Requires: CONFIG_ESP_WIFI_FTM_RESPONDER_SUPPORT=y in sdkconfig
 */

 #include <string.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "esp_system.h"
 #include "esp_wifi.h"
 #include "esp_event.h"
 #include "esp_log.h"
 #include "nvs_flash.h"
 #include "esp_netif.h"
 
 /* ═══════════════════════════════════════════════════════════════════════════
  * USER-CONFIGURABLE PLACEHOLDERS
  * ═══════════════════════════════════════════════════════════════════════════ */
 #define WIFI_SSID       "TOF_NODE"                  /* Requested SSID */
 #define WIFI_PASS       ".esp_receiver_capstone_32" /* Matches Initiator */
 #define MAX_STA_CONN    4                           /* Max concurrent connections */
 #define WIFI_CHANNEL    1                           /* FTM performs best on fixed channels */
 
 static const char *TAG = "FTM_RESPONDER";
 
 /* ═══════════════════════════════════════════════════════════════════════════
  * EVENT HANDLER
  * ═══════════════════════════════════════════════════════════════════════════ */
 static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
 {
     if (event_id == WIFI_EVENT_AP_STACONNECTED) {
         wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
         ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d",
                  MAC2STR(event->mac), event->aid);
     } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
         wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
         ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d",
                  MAC2STR(event->mac), event->aid);
     }
 }
 
 /* ═══════════════════════════════════════════════════════════════════════════
  * WIFI SOFT-AP INITIALISATION
  * ═══════════════════════════════════════════════════════════════════════════ */
 void wifi_init_softap(void)
 {
     ESP_ERROR_CHECK(esp_netif_init());
     ESP_ERROR_CHECK(esp_event_loop_create_default());
     esp_netif_create_default_wifi_ap();
 
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
 
     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                         ESP_EVENT_ANY_ID,
                                                         &wifi_event_handler,
                                                         NULL,
                                                         NULL));
 
     wifi_config_t wifi_config = {
         .ap = {
             .ssid = WIFI_SSID,
             .ssid_len = strlen(WIFI_SSID),
             .channel = WIFI_CHANNEL,
             .password = WIFI_PASS,
             .max_connection = MAX_STA_CONN,
             .authmode = WIFI_AUTH_WPA2_PSK,
             .ftm_responder = true,         /* ← CRITICAL: Enables 802.11mc pinging */
             .pmf_cfg = {
                 .required = false,
             },
         },
     };
 
     if (strlen(WIFI_PASS) == 0) {
         wifi_config.ap.authmode = WIFI_AUTH_OPEN;
     }
 
     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
     ESP_ERROR_CHECK(esp_wifi_start());
 
     ESP_LOGI(TAG, "FTM Responder initialized on SSID: %s (Channel: %d)",
              WIFI_SSID, wifi_config.ap.channel);
     ESP_LOGI(TAG, "Waiting for FTM Initiators to scan...");
 }
 
 /* ═══════════════════════════════════════════════════════════════════════════
  * APP ENTRY POINT
  * ═══════════════════════════════════════════════════════════════════════════ */
 void app_main(void)
 {
     /* Initialize NVS — required by the WiFi driver */
     esp_err_t ret = nvs_flash_init();
     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
       ESP_ERROR_CHECK(nvs_flash_erase());
       ret = nvs_flash_init();
     }
     ESP_ERROR_CHECK(ret);
 
     ESP_LOGI(TAG, "Starting ESP32-S3 FTM Responder (Tag)");
     
     /* Spin up the SoftAP */
     wifi_init_softap();
 }