#include "Jpglib.h"


const char * jd_errors[] = {
  "Succeeded",
  "Interrupted by output function",
  "Device error or wrong termination of input stream",
  "Insufficient memory pool for the image",
  "Insufficient stream input buffer",
  "Parameter error",
  "Data format error",
  "Right format but not supported",
  "Not supported JPEG standard"
};



uint8_t *fb_jpg;

// Affects the gamma to calculate gray (lower is darker/higher contrast)
// Nice test values: 0.9 1.2 1.4 higher and is too bright
double gamma_value = 0.9;

// JPEG decoder
JDEC jd;
JRESULT rc;

uint8_t *source_buf;    // JPG download buffer
uint8_t *decoded_image; // RAW decoded image
uint8_t tjpgd_work[4096]; // tjpgd 4Kb buffer

uint8_t *tmp_img_buff;

uint32_t buffer_pos;

uint32_t time_epd_fullclear = 0;
uint32_t time_decomp = 0;
uint32_t time_update_screen = 0;
uint32_t time_render = 0;


uint16_t ep_width = EPD_WIDTH;
uint16_t ep_height = EPD_HEIGHT;
uint8_t gamme_curve[256];

char *TAG = "EPDiy";



uint32_t img_buf_pos = 0;
uint64_t startTime = 0;

void init_jpglib(uint8_t *fb_jpg_source)
{
  fb_jpg=fb_jpg_source;
  
  //解码图像内存申请， 来自PSRAM
  decoded_image = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT, MALLOC_CAP_SPIRAM);
  if (decoded_image == NULL) {
    ESP_LOGE("main", "Initial alloc back_buf failed!");
  }
  memset(decoded_image, 255, EPD_WIDTH * EPD_HEIGHT);

  // Should be big enough to allocate the JPEG file size
  source_buf = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT, MALLOC_CAP_SPIRAM);
  if (source_buf == NULL) {
    ESP_LOGE("main", "Initial alloc source_buf failed!");
  }

  printf("Free heap after buffers allocation: %d\n", xPortGetFreeHeapSize());

  tmp_img_buff = (uint8_t *)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);


  double gammaCorrection = 1.0 / gamma_value;
  for (int gray_value = 0; gray_value < 256; gray_value++)
    gamme_curve[gray_value] = round (255 * pow(gray_value / 255.0, gammaCorrection));
}



uint8_t find_closest_palette_color(uint8_t oldpixel)
{
  return oldpixel & 0xF0;
}

//====================================================================================
//   Decode and paint onto the Epaper screen
//====================================================================================
void jpegRender(int xpos, int ypos, int width, int height) {
#if JPG_DITHERING
  unsigned long pixel = 0;
  for (uint16_t by = 0; by < ep_height; by++)
  {
    for (uint16_t bx = 0; bx < ep_width; bx++)
    {
      int oldpixel = decoded_image[pixel];
      int newpixel = find_closest_palette_color(oldpixel);
      int quant_error = oldpixel - newpixel;
      decoded_image[pixel] = newpixel;
      if (bx < (ep_width - 1))
        decoded_image[pixel + 1] = minimum(255, decoded_image[pixel + 1] + quant_error * 7 / 16);

      if (by < (ep_height - 1))
      {
        if (bx > 0)
          decoded_image[pixel + ep_width - 1] =  minimum(255, decoded_image[pixel + ep_width - 1] + quant_error * 3 / 16);

        decoded_image[pixel + ep_width] =  minimum(255, decoded_image[pixel + ep_width] + quant_error * 5 / 16);
        if (bx < (ep_width - 1))
          decoded_image[pixel + ep_width + 1] = minimum(255, decoded_image[pixel + ep_width + 1] + quant_error * 1 / 16);
      }
      pixel++;
    }
  }
#endif

  // Write to display
  uint64_t drawTime = esp_timer_get_time();
  uint32_t padding_x = 0;
  uint32_t padding_y = 0;

  for (uint32_t by = 0; by < height; by++) {
    for (uint32_t bx = 0; bx < width; bx++) {
      epd_draw_pixel(bx + padding_x, by + padding_y, decoded_image[by * width + bx], fb_jpg);
    }
  }
  // calculate how long it took to draw the image
  time_render = (esp_timer_get_time() - drawTime) / 1000;
  ESP_LOGI("render", "%d ms - jpeg draw", time_render);
}



 uint32_t feed_buffer(JDEC *jd,
                                 uint8_t *buff, // Pointer to the read buffer (NULL:skip)
                                 uint32_t nd
                                ) {
  uint32_t count = 0;
  while (count < nd) {
    if (buff != NULL) {
      *buff++ = source_buf[buffer_pos];
    }
    count ++;
    buffer_pos++;
  }

  return count;
}




/* User defined call-back function to output decoded RGB bitmap in decoded_image buffer */
  uint32_t tjd_output(JDEC *jd,     /* Decompressor object of current session */
                                void *bitmap, /* Bitmap data to be output */
                                JRECT *rect   /* Rectangular region to output */
                               ) {
  esp_task_wdt_reset();

  uint32_t w = rect->right - rect->left + 1;
  uint32_t h = rect->bottom - rect->top + 1;
  uint32_t image_width = jd->width;
  uint8_t *bitmap_ptr = (uint8_t*)bitmap;

  for (uint32_t i = 0; i < w * h; i++) {

    uint8_t r = *(bitmap_ptr++);
    uint8_t g = *(bitmap_ptr++);
    uint8_t b = *(bitmap_ptr++);

    // Calculate weighted grayscale
    //uint32_t val = ((r * 30 + g * 59 + b * 11) / 100); // original formula
    uint32_t val = (r * 38 + g * 75 + b * 15) >> 7; // @vroland recommended formula

    int xx = rect->left + i % w;
    if (xx < 0 || xx >= image_width) {
      continue;
    }
    int yy = rect->top + i / w;
    if (yy < 0 || yy >= jd->height) {
      continue;
    }

    /* Optimization note: If we manage to apply here the epd_draw_pixel directly
       then it will be no need to keep a huge raw buffer (But will loose dither) */
    decoded_image[yy * image_width + xx] = gamme_curve[val];
  }

  return 1;
}

//====================================================================================
//   This function opens source_buf Jpeg image file and primes the decoder
//====================================================================================
int drawBufJpeg(uint8_t *source_buf, int xpos, int ypos) {
  ESP_LOGE(TAG, "jd_prepare");

  //此值不要忘了初始化
  buffer_pos = 0;

  rc = jd_prepare(&jd, feed_buffer, tjpgd_work, sizeof(tjpgd_work), &source_buf);
  if (rc != JDR_OK) {
    ESP_LOGE(TAG, "JPG jd_prepare error: %s", jd_errors[rc]);
    return ESP_FAIL;
  }

  uint32_t decode_start = esp_timer_get_time();
  ESP_LOGE(TAG, "jd_decomp");
  // Last parameter scales        v 1 will reduce the image
  rc = jd_decomp(&jd, tjd_output, 0);
  if (rc != JDR_OK) {
    ESP_LOGE(TAG, "JPG jd_decomp error: %s", jd_errors[rc]);
    return ESP_FAIL;
  }

  time_decomp = (esp_timer_get_time() - decode_start) / 1000;

  ESP_LOGI("JPG", "jpeg file size, width: %d height: %d", jd.width, jd.height);
  ESP_LOGI("decode", "%d ms . image decompression", time_decomp);

  // Render the image onto the screen at given coordinates
  jpegRender(xpos, ypos, jd.width, jd.height);

  return 1;
}


void show_jpg_from_spiffs(String fn) {
  ESP_LOGI("show_jpg", "fn=%s",  fn);

  File jpegFile = SPIFFS.open(fn, "r");

  uint32_t read_len = 0;
  uint32_t all_read_len = 0;


  //不断循环读取,直到没有其他内容
  while (jpegFile.available())
  {
    read_len = jpegFile.read(tmp_img_buff, 1024);
    memcpy(&source_buf[all_read_len], tmp_img_buff, read_len);
    all_read_len = all_read_len + read_len;
  }
  jpegFile.close();

  epd_poweron();


  memset(decoded_image, 255, EPD_WIDTH * EPD_HEIGHT);

  ESP_LOGI("show_jpg", "jpegFile size=%d\n",  all_read_len);

  drawBufJpeg(source_buf, 0, 0);

  time_update_screen = esp_timer_get_time();
  //清屏
  epd_clear();
  //显示内容
  epd_draw_grayscale_image(epd_full_screen(), fb_jpg);

  epd_poweroff();

  time_update_screen = (esp_timer_get_time() - time_update_screen) / 1000;

  ESP_LOGI("show_jpg", "%d ms epd_hl_update_screen\n",  time_update_screen);

  ESP_LOGI("total", "%d ms - total time spend\n",  time_update_screen + time_decomp + time_render );
}
