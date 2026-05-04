#include "display.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lcd_cyd.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "display";

static uint16_t *rgb_strip = NULL; // Horizontal bands: full-width rows, top-to-bottom (matches LCD scan direction)
static uint8_t *fb_copy = NULL;

#ifndef DISP_WIDTH
#define DISP_WIDTH 240
#endif
#ifndef DISP_HEIGHT
#define DISP_HEIGHT 320
#endif

#define FB_COPY_SIZE (DISP_WIDTH * DISP_HEIGHT / 8)
#define RGB_BUF_LINES 4
#define STRIP_RGB_BYTES (DISP_WIDTH * RGB_BUF_LINES * (int)sizeof(uint16_t))

#ifndef CYD_LCD_DMA_ALIGN
#define CYD_LCD_DMA_ALIGN 32
#endif

static TaskHandle_t display_task_handle = NULL;
static const uint8_t *current_framebuffer = NULL;

void display_init(void) {
    rgb_strip = (uint16_t *)heap_caps_aligned_alloc(CYD_LCD_DMA_ALIGN, STRIP_RGB_BYTES,
                                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (rgb_strip == NULL) {
        rgb_strip = (uint16_t *)heap_caps_malloc(STRIP_RGB_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    }
    if (rgb_strip == NULL) {
        ESP_LOGE(TAG, "Failed to allocate strip RGB buffer (%d bytes)", STRIP_RGB_BYTES);
        return;
    }
    ESP_LOGI(TAG, "Band RGB buffer %d bytes (%d rows x %d cols), align %d", STRIP_RGB_BYTES,
             RGB_BUF_LINES, DISP_WIDTH, CYD_LCD_DMA_ALIGN);

    fb_copy = heap_caps_malloc(FB_COPY_SIZE, MALLOC_CAP_8BIT);
    if (fb_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer copy");
        return;
    }
    ESP_LOGI(TAG, "Framebuffer copy: %d bytes", FB_COPY_SIZE);
}

void display_notify_update(void) {
    if (display_task_handle) {
        xTaskNotifyGive(display_task_handle);
    }
}

static void display_task(void *arg) {
    ESP_LOGI(TAG, "Display task started on Core %d", xPortGetCoreID());

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (current_framebuffer && rgb_strip && fb_copy) {
            memcpy(fb_copy, current_framebuffer, FB_COPY_SIZE);

            const int mac_bytes_per_row = DISP_WIDTH / 8;
            const int lcd_width = DISP_WIDTH;
            const int lcd_height = DISP_HEIGHT;

            for (int y = 0; y < lcd_height; y += RGB_BUF_LINES) {
                int rows = (y + RGB_BUF_LINES > lcd_height) ? (lcd_height - y) : RGB_BUF_LINES;

                for (int row = 0; row < rows; row++) {
                    int mac_row = y + row;
                    for (int x = 0; x < lcd_width; x++) {
                        int byte_offset = mac_row * mac_bytes_per_row + x / 8;
                        int bit_offset = 7 - (x % 8);
                        uint8_t pixel = (fb_copy[byte_offset] >> bit_offset) & 1;
                        // Reverse x: compensates for MX=1 (GRAM col 0 → physical right)
                        rgb_strip[row * lcd_width + (lcd_width - 1 - x)] = pixel ? 0x0000 : 0xFFFF;
                    }
                }

                lcd_draw_bitmap(0, y, lcd_width, rows, (const uint8_t *)rgb_strip);
                lcd_wait_trans_complete();
            }

            // Hold the completed frame stable before accepting the next render.
            // Without this, the emulator (running at ~50fps) floods notifications
            // and the screen is always mid-render, never settling to a clean image.
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void display_task_start(int core, int priority) {
    if (display_task_handle != NULL) {
        ESP_LOGW(TAG, "Display task already running");
        return;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(display_task, "display_task", 4096, NULL, priority,
                                             &display_task_handle, core);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
    } else {
        ESP_LOGI(TAG, "Display task created on Core %d with priority %d", core, priority);
    }
}

void display_set_framebuffer(const uint8_t *fb) {
    current_framebuffer = fb;
}
