#pragma once
using esp_err_t=int;
constexpr int ESP_OK=0, ESP_ERR_INVALID_STATE=1;
inline int feeds=0;
struct esp_task_wdt_config_t {uint32_t timeout_ms;uint32_t idle_core_mask;bool trigger_panic;};
inline int esp_task_wdt_init(esp_task_wdt_config_t*){return ESP_ERR_INVALID_STATE;}
inline int esp_task_wdt_init(int,bool){return ESP_OK;}
inline int esp_task_wdt_reconfigure(esp_task_wdt_config_t*){return ESP_OK;}
inline int esp_task_wdt_status(void*){return 1;}
inline int esp_task_wdt_add(void*){return ESP_OK;}
inline int esp_task_wdt_reset(){++feeds;return ESP_OK;}
