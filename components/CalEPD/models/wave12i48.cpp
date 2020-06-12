#include "wave12i48.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/task.h"

/*
 The EPD needs a bunch of command/data values to be initialized. They are send using the IO class
*/
#define T1 30 // charge balance pre-phase
#define T2  5 // optional extension
#define T3 30 // color change phase (b/w)
#define T4  5 // optional extension for one color

// Partial Update Delay, may have an influence on degradation
#define WAVE12I48_PU_DELAY 100

//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.

DRAM_ATTR const epd_init_42 Wave12I48::lut_20_LUTC_partial={
0x20, {
  0x00, T1, T2, T3, T4, 1, 0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
},42};

// 0x07 (2nd) VGH=20V,VGL=-20V
// 0x3f (1st) VDH= 15V
// 0x3f (2nd) VDH=-15V
DRAM_ATTR const epd_power_4 Wave12I48::epd_wakeup_power={
0x01,{0x07,0x07,0x3f,0x3f},4
};


DRAM_ATTR const epd_init_1 Wave12I48::epd_panel_setting_full={
0x00,{0x1f},1
};

DRAM_ATTR const epd_init_1 Wave12I48::epd_panel_setting_partial={
0x00,{0x3f},1
};


DRAM_ATTR const epd_init_4 Wave12I48::epd_resolution={
0x61,{
  WAVE12I48_WIDTH / 256, //source 800
  WAVE12I48_WIDTH % 256,
  WAVE12I48_HEIGHT / 256,//gate 480
  WAVE12I48_HEIGHT % 256
},4};

// Constructor
Wave12I48::Wave12I48(Epd4Spi& dio): 
  Adafruit_GFX(WAVE12I48_WIDTH, WAVE12I48_HEIGHT),
  Epd(WAVE12I48_WIDTH, WAVE12I48_HEIGHT), IO(dio)
{
  rtc_wdt_feed();
  vTaskDelay(pdMS_TO_TICKS(1));
  printf("Wave12I48() constructor injects IO and extends Adafruit_GFX(%d,%d) Pix Buffer[%d]\n",
  WAVE12I48_WIDTH, WAVE12I48_HEIGHT, WAVE12I48_BUFFER_SIZE);
  printf("\nAvailable heap after Epd bootstrap:%d\n",xPortGetFreeHeapSize());
}

void Wave12I48::initFullUpdate(){
  IO.cmd(epd_panel_setting_full.cmd);          // panel setting
  IO.data(epd_panel_setting_full.data[0]);     // full update LUT from OTP
}

void Wave12I48::initPartialUpdate(){
  IO.cmd(epd_panel_setting_partial.cmd);       // panel setting
  IO.data(epd_panel_setting_partial.data[0]);  // partial update LUT from registers

  IO.cmd(0x82);    // vcom_DC setting
  //      (0x2C);  // -2.3V same value as in OTP
  IO.data(0x26);   // -2.0V
  //       (0x1C); // -1.5V
  IO.cmd(0x50);    // VCOM AND DATA INTERVAL SETTING
  IO.data(0x39);   // LUTBD, N2OCP: copy new to old
  IO.data(0x07);

  // LUT Tables for partial update. Send them directly in 42 bytes chunks. In total 210 bytes
  IO.cmd(lut_20_LUTC_partial.cmd);
  IO.data(lut_20_LUTC_partial.data,lut_20_LUTC_partial.databytes);


 }

//Initialize the display
void Wave12I48::init(bool debug)
{
    debug_enabled = debug;
    if (debug_enabled) printf("Wave12I48::init(debug:%d) + fillScreen\n", debug);
    //Initialize SPI at 4MHz frequency. true for debug
    IO.init(4, true);
    fillScreen(EPD_WHITE);
}

void Wave12I48::fillScreen(uint16_t color)
{
  if (debug_enabled) printf("fillScreen(%x) Buffer size:%d\n",color,sizeof(_buffer));
  uint8_t data = (color == EPD_WHITE) ? 0xFF : 0x00;
  uint32_t lastx=0;
  for (uint32_t x = 0; x < sizeof(_buffer); x++)
  {
    _buffer[x] = data;
    lastx = x;
  }
  printf("Filled buffer from [0] to [%d]\n",lastx);
}

void Wave12I48::_wakeUp(){
IO.reset(10);
//IMPORTANT: Some EPD controllers like to receive data byte per byte
//So this won't work:
//IO.data(epd_wakeup_power.data,epd_wakeup_power.databytes);
  
  IO.cmd(epd_wakeup_power.cmd);
  for (int i=0;i<epd_wakeup_power.databytes;++i) {
    IO.data(epd_wakeup_power.data[i]);
  }
 
  IO.cmd(0x04);
  _waitBusy("_wakeUp power on");
  
  IO.cmd(epd_panel_setting_full.cmd);
  for (int i=0;i<epd_panel_setting_full.databytes;++i) {
    IO.data(epd_panel_setting_full.data[i]);
  }

  IO.cmd(0x61); // tres (??? Check what is)
  // Resolution setting
  IO.cmd(epd_resolution.cmd);
  for (int i=0;i<epd_resolution.databytes;++i) {
    IO.data(epd_resolution.data[i]);
  }
  IO.data(0x00);
  IO.cmd(0x50);  // VCOM AND DATA INTERVAL SETTING
  IO.data(0x29); // LUTKW, N2OCP: copy new to old
  IO.data(0x07);
  IO.cmd(0x60);  // TCON SETTING
  IO.data(0x22);

  initFullUpdate();
}

void Wave12I48::update()
{
  _using_partial_mode = false;
  _wakeUp();

  IO.cmd(0x13);
  printf("Sending a %d bytes buffer via SPI\n",sizeof(_buffer));
  for (uint32_t i = 0; i < sizeof(_buffer); i++)
  {
    uint8_t data = i < sizeof(_buffer) ? _buffer[i] : 0x00;
    IO.data(data);
    // Let CPU breath. Withouth delay watchdog will jump in your neck
    if (i%8==0) {
       rtc_wdt_feed();
       vTaskDelay(pdMS_TO_TICKS(1));
     }
    if (i%500==0 && debug_enabled) printf("%d ",i);
  }

  IO.cmd(0x12);
  _waitBusy("update");
  _sleep();
}

uint16_t Wave12I48::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t xe, uint16_t ye)
{
  x &= 0xFFF8;            // byte boundary
  xe = (xe - 1) | 0x0007; // byte boundary - 1
  IO.cmd(0x90);           // partial window
  IO.data(x / 256);
  IO.data(x % 256);
  IO.data(xe / 256);
  IO.data(xe % 256);
  IO.data(y / 256);
  IO.data(y % 256);
  IO.data(ye / 256);
  IO.data(ye % 256);
  IO.data(0x00);
  return (7 + xe - x) / 8; // number of bytes to transfer per line

}

void Wave12I48::updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool using_rotation)
{
  printf("updateWindow: Still in test mode\n");
  if (using_rotation) _rotate(x, y, w, h);
  if (x >= WAVE12I48_WIDTH) return;
  if (y >= WAVE12I48_HEIGHT) return;
  uint16_t xe = gx_uint16_min(WAVE12I48_WIDTH, x + w) - 1;
  uint16_t ye = gx_uint16_min(WAVE12I48_HEIGHT, y + h) - 1;
  // x &= 0xFFF8; // byte boundary, not needed here
  uint16_t xs_bx = x / 8;
  uint16_t xe_bx = (xe + 7) / 8;
  if (!_using_partial_mode) _wakeUp();

  _using_partial_mode = true;
  initPartialUpdate();
  
  { // leave both controller buffers equal
    IO.cmd(0x91); // partial in
    _setPartialRamArea(x, y, xe, ye);
    IO.cmd(0x13);

    for (int16_t y1 = y; y1 <= ye; y1++)
    {
      for (int16_t x1 = xs_bx; x1 < xe_bx; x1++)
      {
        uint16_t idx = y1 * (WAVE12I48_WIDTH / 8) + x1;
        // white is 0x00 in buffer
        uint8_t data = (idx < sizeof(_buffer)) ? _buffer[idx] : 0x00;
        // white is 0xFF on device
        IO.data(data);
        if (idx%8==0) {
          rtc_wdt_feed();
          vTaskDelay(pdMS_TO_TICKS(1));
        }
      }
    }
    IO.cmd(0x12); // display refresh
    _waitBusy("updateWindow");
    IO.cmd(0x92); // partial out
  } // leave both controller buffers equal
  
  vTaskDelay(WAVE12I48_PU_DELAY / portTICK_PERIOD_MS);
}

void Wave12I48::_waitBusy(const char* message){
  if (debug_enabled) {
    ESP_LOGI(TAG, "_waitBusy for %s", message);
  }
  int64_t time_since_boot = esp_timer_get_time();

  while (1){
    if (gpio_get_level((gpio_num_t)CONFIG_EINK_BUSY) == 1) break;
    vTaskDelay(1);
    if (esp_timer_get_time()-time_since_boot>2000000)
    {
      if (debug_enabled) ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
}

void Wave12I48::_sleep(){
  IO.cmd(0x02);
  _waitBusy("power_off");
  IO.cmd(0x07); // Deep sleep
  IO.data(0xA5);
}

void Wave12I48::_rotate(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h)
{
  switch (getRotation())
  {
    case 1:
      swap(x, y);
      swap(w, h);
      x = WAVE12I48_WIDTH - x - w - 1;
      break;
    case 2:
      x = WAVE12I48_WIDTH - x - w - 1;
      y = WAVE12I48_HEIGHT - y - h - 1;
      break;
    case 3:
      swap(x, y);
      swap(w, h);
      y = WAVE12I48_HEIGHT - y - h - 1;
      break;
  }
}

void Wave12I48::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;
  switch (getRotation())
  {
    case 1:
      swap(x, y);
      x = WAVE12I48_WIDTH - x - 1;
      break;
    case 2:
      x = WAVE12I48_WIDTH - x - 1;
      y = WAVE12I48_HEIGHT - y - 1;
      break;
    case 3:
      swap(x, y);
      y = WAVE12I48_HEIGHT - y - 1;
      break;
  }
  uint16_t i = x / 8 + y * WAVE12I48_WIDTH / 8;

  if (color) {
    _buffer[i] = (_buffer[i] | (1 << (7 - x % 8)));
    } else {
    _buffer[i] = (_buffer[i] & (0xFF ^ (1 << (7 - x % 8))));
    }
}
