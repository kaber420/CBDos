#pragma once

#include <cstdio>

#if defined(ESP_PLATFORM)
#include <esp_log.h>

#define CBD_LOG_E(tag, format, ...) ESP_LOGE(tag, format, ##__VA_ARGS__)
#define CBD_LOG_W(tag, format, ...) ESP_LOGW(tag, format, ##__VA_ARGS__)
#define CBD_LOG_I(tag, format, ...) ESP_LOGI(tag, format, ##__VA_ARGS__)
#define CBD_LOG_D(tag, format, ...) ESP_LOGD(tag, format, ##__VA_ARGS__)
#define CBD_LOG_V(tag, format, ...) ESP_LOGV(tag, format, ##__VA_ARGS__)

#else

#define CBD_LOG_E(tag, format, ...) printf("[E][%s] " format "\n", tag, ##__VA_ARGS__)
#define CBD_LOG_W(tag, format, ...) printf("[W][%s] " format "\n", tag, ##__VA_ARGS__)
#define CBD_LOG_I(tag, format, ...) printf("[I][%s] " format "\n", tag, ##__VA_ARGS__)
#define CBD_LOG_D(tag, format, ...) printf("[D][%s] " format "\n", tag, ##__VA_ARGS__)
#define CBD_LOG_V(tag, format, ...) printf("[V][%s] " format "\n", tag, ##__VA_ARGS__)

#endif
