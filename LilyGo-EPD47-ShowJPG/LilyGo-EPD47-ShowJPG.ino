#include "Jpglib.h"


//代码来源：
//https://github.com/vroland/epdiy/blob/master/examples/www-image/main/jpg-render.c



uint8_t *fb;
int jpg_index = 0;

void setup() {
  Serial.begin(115200);  // 可用

  epd_init();

  fb = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  memset(fb, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  if (!SPIFFS.begin(true))
  {
    ESP_LOGE("main", "SPIFFS begin failed!");
    return;
  }

  init_jpglib(fb);

}


//注：最好每显示一图像前 epd_clear 清屏一次
//否则显示效果不好！
void loop() {

  jpg_index = jpg_index + 1;
  if (jpg_index > 3)
    jpg_index = 1;

  //清空显示缓存
  memset(fb, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  show_jpg_from_spiffs("/" + String(jpg_index) + ".jpg"); // 平均需要2.3秒

  delay(5000);
}
