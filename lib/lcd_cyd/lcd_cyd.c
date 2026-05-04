#include "lcd_cyd.h"

#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "hw.h"

#include "driver/gpio.h"

static const char *TAG = "lcd_cyd";

/*
 * ILI9341 MADCTL (0x36). Must match panel batch + esp_lcd BGR handling.
 * Default 0xE8 matches original Cydintosh tuning and works on typical CYD / CYD2USB.
 * Some boards look rotated or wrong with 0xE8; override in platformio.ini e.g.:
 *   -DCYD_ILI9341_MADCTL=0x88
 *   -DCYD_ILI9341_MADCTL=0x48
 * If a value gives a blank or rastered screen, try another or remove the override.
 */
#if !defined(CYD_ILI9341_MADCTL)
#define CYD_ILI9341_MADCTL 0xE8
#endif

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;

static const ili9341_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xCF, (uint8_t[]){0x00, 0xD9, 0x30},             3, 0},
    {0xED, (uint8_t[]){0x64, 0x03, 0x12, 0x81},       4, 0},
    {0xE8, (uint8_t[]){0x85, 0x10, 0x7A},             3, 0},
    {0xCB, (uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5, 0},
    {0xF7, (uint8_t[]){0x20},                         1, 0},
    {0xEA, (uint8_t[]){0x00, 0x00},                   2, 0},
    {0xC0, (uint8_t[]){0x1B},                         1, 0},
    {0xC1, (uint8_t[]){0x12},                         1, 0},
    {0xC5, (uint8_t[]){0x26, 0x26},                   2, 0},
    {0xC7, (uint8_t[]){0xB0},                         1, 0},
    {0xB1, (uint8_t[]){0x00, 0x1A},                   2, 0},
    {0xB6, (uint8_t[]){0x0A, 0xA2},                   2, 0},
    {0xF2, (uint8_t[]){0x00},                         1, 0},
    {0x26, (uint8_t[]){0x01},                         1, 0},
    {0xE0,
     (uint8_t[]){0x1F, 0x24, 0x24, 0x0D, 0x12, 0x09, 0x52, 0xB7, 0x3F, 0x0C, 0x15, 0x06, 0x0E, 0x08,
                 0x00},
     15,                                                 0},
    {0xE1,
     (uint8_t[]){0x00, 0x1B, 0x1B, 0x02, 0x0E, 0x06, 0x2E, 0x48, 0x3F, 0x03, 0x0A, 0x09, 0x31, 0x37,
                 0x1F},
     15,                                                 0},
    {0x36, (uint8_t[]){CYD_ILI9341_MADCTL},           1, 0},
    {0x21, NULL,                                      0, 0},
};

void lcd_cyd_init(void) {
    static bool initialized = false;
    if (initialized) {
        ESP_LOGI(TAG, "LCD already initialized, skipping");
        return;
    }

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t bus_config =
        ILI9341_PANEL_BUS_SPI_CONFIG(TFT_SPI_CLK, TFT_SPI_MOSI, 240 * 320 * sizeof(uint16_t));
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config =
        ILI9341_PANEL_IO_SPI_CONFIG(TFT_SPI_CS, TFT_DC, NULL, NULL);
    io_config.pclk_hz = CYD_SPI_CLOCK_HZ;
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    ili9341_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(ili9341_lcd_init_cmd_t),
    };
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT_RESET,
        .rgb_ele_order = CYD_RGB_ORDER,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

#ifdef CYD_MIRROR_DISPLAY
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    gpio_reset_pin(TFT_BL);
    gpio_set_direction(TFT_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(TFT_BL, 1);

    initialized = true;
    ESP_LOGI(TAG, "LCD initialized");
}

void lcd_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data) {
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + w, y + h, data);
}

void lcd_wait_trans_complete(void) {
    // Wait for all queued transactions to complete
    // This will block until queue is empty
    esp_lcd_panel_io_tx_param(io_handle, -1, NULL, 0);
}