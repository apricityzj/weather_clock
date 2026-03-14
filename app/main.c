#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f10x.h"
#include "main.h"
#include "led.h"
#include "rtc.h"
#include "timer.h"
#include "esp_usart.h"
#include "mpu6050.h"
#include "st7735.h"
#include "stfonts.h"
#include "stimage.h"
#include "weather.h"
#include <stdlib.h>


#define RX_CMD_MAX_LEN 512


static char rx_buf[RX_CMD_MAX_LEN];
static uint16_t rx_len = 0;

static bool time_update_req = false;
static uint32_t new_timestamp = 0;

static bool weather_update_req = false;
static weather_t new_weather;

static uint32_t runms;
static uint32_t disp_height;


static void timer_elapsed_callback(void)
{
    runms++;
    if (runms >= 24 * 60 * 60 * 1000)
    {
        runms = 0;
    }
}

static void on_serial_receive(uint8_t data)
{
    if (data == '\n')
    {
        rx_buf[rx_len] = '\0';
        
        // // 解析 T:时间戳 (保留原有逻辑)
        // if (rx_len > 2 && rx_buf[0] == 'T' && rx_buf[1] == ':')
        // {
        //     new_timestamp = (uint32_t)atoi(&rx_buf[2]);
        //     time_update_req = true;
        // }

        //解析 JSON 天气格式 (新逻辑：通过查找 results 判定)
        if (strstr(rx_buf, "\"results\"") != NULL)
        {
            if (weather_parse(rx_buf, &new_weather))
            {
                weather_update_req = true;
                
                // 解析时间 "last_update":"2026-03-12T13:00:22+08:00"
                char *p = strstr(rx_buf, "\"last_update\":\"");
                if (p != NULL)
                {
                    rtc_date_t date;
                    p += strlen("\"last_update\":\"");
                    if (sscanf(p, "%hu-%hhu-%hhuT%hhu:%hhu:%hhu", 
                        &date.year, &date.month, &date.day, 
                        &date.hour, &date.minute, &date.second) == 6)
                    {
                        rtc_set_date(&date);
                    }
                }
            }
        }
        // 解析 W:天气,温度 (保留作为备选)
        // else if (rx_len > 2 && rx_buf[0] == 'W' && rx_buf[1] == ':')
        // {
        //     char *comma = strchr(&rx_buf[2], ',');
        //     if (comma != NULL)
        //     {
        //         *comma = '\0';
        //         strncpy(new_weather.weather, &rx_buf[2], sizeof(new_weather.weather) - 1);
        //         new_weather.weather[sizeof(new_weather.weather) - 1] = '\0';
                
        //         strncpy(new_weather.temperature, comma + 1, sizeof(new_weather.temperature) - 1);
        //         new_weather.temperature[sizeof(new_weather.temperature) - 1] = '\0';
                
        //         weather_update_req = true;
        //     }
        // }
        rx_len = 0;
    }
    else if (data != '\r' && rx_len < RX_CMD_MAX_LEN - 1)
    {
        rx_buf[rx_len++] = data;
    }
}

//void receive_data(void){


//}

int main(void)
{
    board_lowlevel_init();

    led_init();
    rtc_init();
    timer_init(1000);
    timer_elapsed_register(timer_elapsed_callback);
    timer_start();

    mpu6050_init();

    st7735_init();
    st7735_fill_screen(ST7735_BLACK);

    // 显示开机内容
    st7735_write_string(0, 0, "Initializing...", &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
    disp_height += font_ascii_8x16.height;
    delay_ms(500);

    st7735_write_string(0, disp_height, "Wait Serial...", &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
    disp_height += font_ascii_8x16.height;
    
    esp_usart_init();
    esp_usart_receive_register(on_serial_receive);
    
    delay_ms(500);

    st7735_write_string(0, disp_height, "Ready", &font_ascii_8x16, ST7735_GREEN, ST7735_BLACK);
    disp_height += font_ascii_8x16.height;
    delay_ms(500);

    st7735_fill_screen(ST7735_BLACK);
    delay_ms(500);
    runms = 0;
    uint32_t last_runms = runms;
    bool weather_ok = false;
    char str[64];
    while (1)
    {
        if (runms == last_runms)
        {
            continue;
        }
        last_runms = runms;
		
		bool weather_parse(const char *data, weather_t *weather);

		
        // 更新时间信息
        if (last_runms % 100 == 0)
        {
            rtc_date_t date;
            rtc_get_date(&date);
            snprintf(str, sizeof(str), "%02d-%02d-%02d", date.year % 100, date.month, date.day);
            st7735_write_string(0, 0, str, &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
            snprintf(str, sizeof(str), "%02d%s%02d", date.hour, date.second % 2 ? " " : ":", date.minute);
            st7735_write_string(0, 78, str, &font_time_24x48, ST7735_CYAN, ST7735_BLACK);
        }

        // 串口同步时间
        if (time_update_req)
        {
            time_update_req = false;
            rtc_set_timestamp(new_timestamp + 8 * 60 * 60);
        }

        // 串口更新天气信息，后边加"\n"
        if (weather_update_req)
        {
            weather_update_req = false;
            weather_ok = true;

            weather_t *weather = &new_weather;

            const st_image_t *img = NULL;
            if (strcmp(weather->weather, "Cloudy") == 0) {
                img = &icon_weather_duoyun;
            } else if (strcmp(weather->weather, "Wind") == 0) {
                img = &icon_weather_feng;
            } else if (strcmp(weather->weather, "Clear") == 0) {
                img = &icon_weather_qing;
            } else if (strcmp(weather->weather, "Snow") == 0) {
                img = &icon_weather_xue;
            } else if (strcmp(weather->weather, "Overcast") == 0) {
                img = &icon_weather_yin;
            } else if (strcmp(weather->weather, "Rain") == 0) {
                img = &icon_weather_yu;
            }
            if (img != NULL) {
                st7735_draw_image(0, 16, img->width, img->height, img->data);
            } else {
                snprintf(str, sizeof(str), "%s", weather->weather);
                st7735_write_string(0, 16, str, &font_ascii_8x16, ST7735_YELLOW, ST7735_BLACK);
            }

            snprintf(str, sizeof(str), "%sC", weather->temperature);
            st7735_write_string(78, 0, str, &font_temper_16x32, ST7735_BLUE, ST7735_BLACK);
        }

        // 更新环境温度
        if (last_runms % (1 * 1000) == 0)
        {
            float temper = mpu6050_read_temper();
            snprintf(str, sizeof(str), "%5.1fC", temper);
            st7735_write_string(78, 32, str, &font_ascii_8x16, ST7735_GREEN, ST7735_BLACK);
        }

        // 更新网络信息
        // if (last_runms % (30 * 1000) == 0)
        // {
        //     st7735_write_string(0, 127-48, wifi_ssid, &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
        //     char ip[16];
        //     esp_at_wifi_get_ip(ip);
        //     st7735_write_string(0, 127-32, ip, &font_ascii_8x16, ST7735_WHITE, ST7735_BLACK);
        // }

		
    }
}
