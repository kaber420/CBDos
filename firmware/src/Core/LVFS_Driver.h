#pragma once
#include <lvgl.h>

void lv_fs_if_init();
void lv_fs_set_spi_mutex(void* mutex);
void lv_fs_spi_lock();
void lv_fs_spi_unlock();
