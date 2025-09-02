#include "time.h"
#include "led_driver.h"

extern RTC_HandleTypeDef hrtc;   // from main.c

// --------- tune here ----------
#define TIME_LEVEL       4      // per-pixel level 0..(BR_STEPS-1)
#define USE_12H          1       // 0 = 24h, 1 = 12h with leading 0 suppressed
// -------------------------------

// 3x5 font is defined in led_driver.h as HEX_3x5[10][5]
// HEX_W=3, HEX_H=5, and (usually) DIGIT_SP=1


// Time area geometry (center the 5-row band vertically)
#define TIME_H     (HEX_H)
#define ROW0       ((ROWS - TIME_H) / 2)   // 5 on a 15x15
// Horizontal layout: 4 digits + 3 gaps (1-col each). The middle gap is our colon.
// That totals 15 columns → perfectly fills 0..14, i.e., centered already.
#define COL_LEFT   ((COLS - (HEX_W*4 + 3)) / 2)  // equals 0 on 15x15

// Digit X positions
#define COL_D0     (COL_LEFT + 0)
#define COL_D1     (COL_D0 + HEX_W + 1)
#define COL_COLON  (COL_D1 + HEX_W)          // the middle 1-col gap
#define COL_D2     (COL_COLON + 1)
#define COL_D3     (COL_D2 + HEX_W + 1)

// track what’s currently displayed so we only redraw when needed
static uint8_t cur_h10 = 0xFF, cur_h1 = 0xFF, cur_m10 = 0xFF, cur_m1 = 0xFF;
static uint8_t cur_colon_on = 0xFF;

// small helpers
static inline void draw_digit3x5(uint8_t r0, uint8_t c0, int d, uint8_t level)
{
  for (uint8_t ry = 0; ry < HEX_H; ++ry) {
    uint8_t row = (d >= 0 && d <= 9) ? HEX_3x5[d][ry] : 0;
    for (uint8_t rx = 0; rx < HEX_W; ++rx) {
      uint8_t bit = (row >> (HEX_W - 1 - rx)) & 1u;
      Display_SetPixelRC(r0 + ry, c0 + rx, bit ? level : 0);
    }
  }
}


static inline void clear_digit3x5(uint8_t r0, uint8_t c0)
{
  for (uint8_t ry = 0; ry < HEX_H; ++ry)
    for (uint8_t rx = 0; rx < HEX_W; ++rx)
      Display_SetPixelRC(r0 + ry, c0 + rx, 0);
}

static inline void draw_colon(uint8_t r0, uint8_t c, uint8_t on, uint8_t level)
{
  // place two dots at rows r0+1 and r0+3 (middle of the 5-high band)
  uint8_t rr1 = r0 + 1, rr2 = r0 + 3;
  if (rr1 < ROWS) Display_SetPixelRC(rr1, c, on ? level : 0);
  if (rr2 < ROWS) Display_SetPixelRC(rr2, c, on ? level : 0);
}

static void RTC_SetOnce(uint8_t yy, uint8_t mm, uint8_t dd,
                        uint8_t wd, uint8_t hh, uint8_t min, uint8_t ss)
{
  // Years are 00..99 = 2000..2099
  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  sTime.Hours          = hh;                      // 0..23 (you’re using 24h)
  sTime.Minutes        = min;
  sTime.Seconds        = ss;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;

  // Set TIME first
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK) Error_Handler();

  // Then DATE
  sDate.WeekDay = wd;                              // RTC_WEEKDAY_MONDAY .. SUNDAY
  sDate.Month   = mm;                              // 1..12 or RTC_MONTH_*
  sDate.Date    = dd;                              // 1..31
  sDate.Year    = yy;                              // 0..99  (→ 20yy)
  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK) Error_Handler();
}

static void render_time_once(uint8_t hours, uint8_t mins, uint8_t colon_on)
{
  // split digits
  uint8_t h10 = hours / 10;
  uint8_t h1  = hours % 10;
  uint8_t m10 = mins  / 10;
  uint8_t m1  = mins  % 10;

#if USE_12H
  // 12h formatting with 12 instead of 00; suppress leading zero
  uint8_t h = hours % 12; if (h == 0) h = 12;
  h10 = h / 10;
  h1  = h % 10;
#endif

#if USE_12H
  // clear leading zero digit if needed
  if (h10 == 0) {
    if (cur_h10 != 0xFE) { clear_digit3x5(ROW0, COL_D0); cur_h10 = 0xFE; }
  } else {
    if (cur_h10 != h10)  { draw_digit3x5(ROW0, COL_D0, h10, TIME_LEVEL); cur_h10 = h10; }
  }
#else
  if (cur_h10 != h10) { draw_digit3x5(ROW0, COL_D0, h10, TIME_LEVEL); cur_h10 = h10; }
#endif

  if (cur_h1  != h1 ) { draw_digit3x5(ROW0, COL_D1, h1 , TIME_LEVEL); cur_h1  = h1;  }
  if (cur_m10 != m10) { draw_digit3x5(ROW0, COL_D2, m10, TIME_LEVEL); cur_m10 = m10; }
  if (cur_m1  != m1 ) { draw_digit3x5(ROW0, COL_D3, m1 , TIME_LEVEL); cur_m1  = m1;  }

  if (cur_colon_on != colon_on) {
    draw_colon(ROW0, COL_COLON, colon_on, TIME_LEVEL);
    cur_colon_on = colon_on;
  }
}




void Time_Init(void)
{
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0xA5A5) {
    // Example: Thu Aug 28, 2025 14:37:00
    RTC_SetOnce(/*yy=*/25,
                /*mm=*/9,
                /*dd=*/2,
                /*wd=*/RTC_WEEKDAY_TUESDAY,
                /*hh=*/8, /*min=*/2, /*ss=*/15);

    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0xA5A5);
  }

  // force first render
  cur_h10 = cur_h1 = cur_m10 = cur_m1 = 0xFF;
  cur_colon_on = 0xFF;

  // draw once using current RTC
  RTC_TimeTypeDef t; RTC_DateTypeDef d;
  HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);  // must read date after time
//   render_time_once(t.Hours, t.Minutes, (t.Seconds & 1) == 0);
}

void Time_Awake(void)
{
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
}

 void Time_Tick(void)
{
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    uint8_t colon_on = (t.Seconds & 1) == 0;
    render_time_once(t.Hours, t.Minutes, colon_on);
}

void Time_Get(uint8_t *hh, uint8_t *mm, uint8_t *ss)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN); // required to unlock shadow regs

    *hh = t.Hours;
    *mm = t.Minutes;
    *ss = t.Seconds;
}