#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "SPIFFS.h"


// JPG decoder
#if ESP_IDF_VERSION_MAJOR >= 4 // IDF 4+
#include "esp32/rom/tjpgd.h"
#else // ESP32 Before IDF 4.0
#include "rom/tjpgd.h"
#endif

#include "esp_task_wdt.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "epd_driver.h"

// Jpeg: Adds dithering to image rendering (Makes grayscale smoother on transitions)
#define JPG_DITHERING true

// Return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))


//int drawBufJpeg(uint8_t *source_buf, int xpos, int ypos);
//
//uint32_t tjd_output(JDEC *jd,     /* Decompressor object of current session */
//                    void *bitmap, /* Bitmap data to be output */
//                    JRECT *rect   /* Rectangular region to output */
//                   );
//uint32_t feed_buffer(JDEC *jd,
//                     uint8_t *buff, // Pointer to the read buffer (NULL:skip)
//                     uint32_t nd
//                    )        ;
//void jpegRender(int xpos, int ypos, int width, int height);
//uint8_t find_closest_palette_color(uint8_t oldpixel);
//


void show_jpg_from_spiffs(String fn) ;

void init_jpglib(uint8_t *fb_source);
