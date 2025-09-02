// Core/Src/led_driver.c

#include "led_driver.h"
#include "main.h"
#include <math.h>
#include <stdbool.h> 

// If BR_STEPS is not defined in your header, fall back to BR_LEVELS
#ifndef BR_STEPS
#define BR_STEPS BR_LEVELS
#endif

#define TIME_LEVEL  255
#define TIME_H      (HEX_H)
#define ROW0        ((ROWS - TIME_H) / 2)   // vertically centered
#define COL_LEFT    ((COLS - (HEX_W*4 + 3)) / 2) // horizontally centered

#define COL_D0      (COL_LEFT + 0)
#define COL_D1      (COL_D0 + HEX_W + 1)
#define COL_COLON   (COL_D1 + HEX_W)
#define COL_D2      (COL_COLON + 1)
#define COL_D3      (COL_D2 + HEX_W + 1)

#define GAMMA     2.8f
#define LUT_SIZE  256

static uint16_t gamma_lut[LUT_SIZE];   // maps 0..255 → scaled CCR
typedef struct { uint8_t hi, lo; uint16_t idx; } ScanEntry;

/* -------- Framebuffer & active list -------- */
static volatile uint8_t fb[N_PIXELS];   // per-pixel brightness (0..BR_STEPS-1)
static ScanEntry act[N_PIXELS];         // active entries (pixels with fb>0)
static int16_t pos_map[N_PIXELS];       // map pixel idx -> position in act[], -1 if inactive
static uint16_t act_len;                // number of active pixels
static uint16_t scan_pos;               // next position to scan (0..act_len-1)

/* -------- Last driven pair tracking -------- */
static int8_t last_hi = -1, last_lo = -1;

/* -------- Global brightness (0..BR_STEPS-1) -------- */
static uint8_t g_master = (BR_STEPS > 0 ? BR_STEPS - 1 : 0);

/* -------- Timer handle (provided by Cube) -------- */
extern TIM_HandleTypeDef htim2;

/* ---------------- GPIO low-level helpers ---------------- */
static inline void pin_mode_input(const Pin *p){
  uint32_t sh = (uint32_t)p->pos * 2U;
  p->port->MODER &= ~(3U << sh);              // 00 = input (Hi-Z)
}
static inline void pin_mode_output(const Pin *p){
  uint32_t sh = (uint32_t)p->pos * 2U;
  p->port->MODER = (p->port->MODER & ~(3U << sh)) | (1U << sh); // 01 = output
}
static inline void pin_set(const Pin *p){ p->port->BSRR = p->pinmask; }
static inline void pin_clr(const Pin *p){ p->port->BSRR = ((uint32_t)p->pinmask) << 16U; }

static void all_hi_z(void){
  for (int i = 0; i < N_PINS; ++i) {
    pin_clr(&pins[i]);
    pin_mode_input(&pins[i]);
  }
}

static void release_last(void){
  if (last_hi >= 0){
    pin_clr(&pins[last_hi]); pin_mode_input(&pins[last_hi]);
    pin_clr(&pins[last_lo]); pin_mode_input(&pins[last_lo]);
    last_hi = last_lo = -1;
  }
}

static void drive_pair(uint8_t hi, uint8_t lo){
  release_last();
  const Pin *PH = &pins[hi], *PL = &pins[lo];
  pin_mode_output(PL); pin_clr(PL);   // cathode LOW
  pin_mode_output(PH); pin_set(PH);   // anode HIGH
  last_hi = (int8_t)hi; last_lo = (int8_t)lo;
}

/* --------------- Active list (incremental updates) --------------- */
static inline void act_add(uint16_t idx){
  if (pos_map[idx] >= 0) return;                     // already active
  if (!VALID_MASK[idx]) return;
  CP_Step s = STEPS[idx];
  if (s.hi >= N_PINS || s.lo >= N_PINS || s.hi == s.lo) return;

  __disable_irq();                                   // tiny critical section
  int16_t pos = (int16_t)act_len;
  act[pos] = (ScanEntry){ s.hi, s.lo, idx };
  pos_map[idx] = pos;
  act_len++;
  __enable_irq();
}

static inline void act_remove(uint16_t idx){
  int16_t pos = pos_map[idx];
  if (pos < 0) return;                               // already inactive

  __disable_irq();                                   // tiny critical section
  int16_t last = (int16_t)act_len - 1;
  if (pos != last) {
    act[pos] = act[last];                            // move last into hole
    pos_map[act[pos].idx] = pos;
  }
  act_len = (uint16_t)last;
  pos_map[idx] = -1;
  if (scan_pos > act_len) scan_pos = 0;             // keep scan_pos valid
  __enable_irq();
}

/* ----------------------- Public API ----------------------- */

static float easeOutExpo(float t) {
    return (t >= 1.0f) ? 1.0f : (1 - powf(2, -10 * t));
}

static float easeInExpo(float t) {
    return (t <= 0.0f) ? 0.0f : powf(2, 10 * (t - 1));
}

void Led_CurtainReveal(uint16_t duration_ms, uint8_t hh, uint8_t mm, uint8_t ss)
{
    const int start_row = 7;
    const int reveal_rows = 7;          // only move 4 rows away
    const uint8_t line_level = BR_STEPS - 1;
    const uint32_t frame_delay = 20;
    const uint32_t start = HAL_GetTick();

    while (1) {
        uint32_t now = HAL_GetTick();
        uint32_t elapsed = now - start;
        float t = (float)elapsed / (float)duration_ms;
        if (t > 1.0f) t = 1.0f;

        float eased = easeInExpo(t);

        // Offset goes from 0 → reveal_rows
        int offset = (int)(eased * reveal_rows);

        int top = start_row - offset;
        int bottom = start_row + offset;

        // Step 1: clear
        Display_Clear();

        // Step 2: draw clock
        Led_DrawClock(hh, mm, ss);

        // Step 3: mask above top
        if (top >= 0) {
            Display_SetRegion(0, 0, COLS, top, 0);
        }
        // Step 4: mask below bottom
        if (bottom + 1 < ROWS) {
            Display_SetRegion(bottom + 1, 0, COLS, ROWS - (bottom + 1), 0);
        }

        // Step 5: draw the moving lines themselves
        if (top >= 0 && top < ROWS) {
            for (int c = 0; c < COLS; c++)
                Display_SetPixelRC(top, c, line_level);
        }
        if (bottom >= 0 && bottom < ROWS) {
            for (int c = 0; c < COLS; c++)
                Display_SetPixelRC(bottom, c, line_level);
        }

        if (t >= 1.0f) break;
        HAL_Delay(frame_delay);
    }

    // Final state = just the clock
    Display_Clear();
    Led_DrawClock(hh, mm, ss);
}

void Led_CurtainClose(uint16_t duration_ms, uint8_t hh, uint8_t mm, uint8_t ss)
{
    const int start_row = 7;
    const int reveal_rows = 4;          // distance the lines move
    const uint8_t line_level = BR_STEPS - 1;
    const uint32_t frame_delay = 20;
    const uint32_t start = HAL_GetTick();

    while (1) {
        uint32_t now = HAL_GetTick();
        uint32_t elapsed = now - start;
        float t = (float)elapsed / (float)duration_ms;
        if (t > 1.0f) t = 1.0f;

        // Run easing backwards: 1 → 0
        float eased = 1.0f - easeInExpo(1.0f - t);

        // Offset goes from reveal_rows → 0
        int offset = (int)((1.0f - eased) * reveal_rows);

        int top = start_row - offset;
        int bottom = start_row + offset;

        // Step 1: clear
        Display_Clear();

        // Step 2: draw clock
        Led_DrawClock(hh, mm, ss);

        // Step 3: mask above top
        if (top >= 0) {
            Display_SetRegion(0, 0, COLS, top, 0);
        }
        // Step 4: mask below bottom
        if (bottom + 1 < ROWS) {
            Display_SetRegion(bottom + 1, 0, COLS, ROWS - (bottom + 1), 0);
        }

        // Step 5: draw closing lines
        if (top >= 0 && top < ROWS) {
            for (int c = 0; c < COLS; c++)
                Display_SetPixelRC(top, c, line_level);
        }
        if (bottom >= 0 && bottom < ROWS) {
            for (int c = 0; c < COLS; c++)
                Display_SetPixelRC(bottom, c, line_level);
        }

        if (t >= 1.0f) break;
        HAL_Delay(frame_delay);
    }

    // Final = screen blank (curtains closed)
    Display_Clear();
}


static void Led_DrawDigit3x5(uint8_t r0, uint8_t c0, int d, uint8_t level)
{
  if (d < 0 || d > 9) return;
  for (uint8_t ry = 0; ry < HEX_H; ++ry) {
    uint8_t row = HEX_3x5[d][ry];
    for (uint8_t rx = 0; rx < HEX_W; ++rx) {
      uint8_t bit = (row >> (HEX_W - 1 - rx)) & 1u;
      uint8_t rr = r0 + ry, cc = c0 + rx;
      if (rr < ROWS && cc < COLS)
        Display_SetPixelRC(rr, cc, bit ? level : 0);
    }
  }
}

static void Led_DrawColon(uint8_t r0, uint8_t c, bool on, uint8_t level)
{
  uint8_t rr1 = r0 + 1, rr2 = r0 + 3;
  if (rr1 < ROWS) Display_SetPixelRC(rr1, c, on ? level : 0);
  if (rr2 < ROWS) Display_SetPixelRC(rr2, c, on ? level : 0);
}

void Led_AllOn(uint8_t level)
{
    if (BR_STEPS == 0) return;
    if (level >= BR_STEPS) level = BR_STEPS - 1;

    __disable_irq();  // prevent scan ISR from seeing half-updated fb
    for (uint16_t i = 0; i < N_PIXELS; ++i) {
        fb[i] = level;
        act_add(i);   // ensure each pixel is in the active list
    }
    act_len = N_PIXELS;  // all pixels active
    __enable_irq();
}

void Led_DrawClock(uint8_t hh, uint8_t mm, uint8_t ss)
{
    // Convert to 12h format
    uint8_t h = hh % 12;
    if (h == 0) h = 12;

    uint8_t h10 = h / 10;
    uint8_t h1  = h % 10;
    uint8_t m10 = mm / 10;
    uint8_t m1  = mm % 10;

    bool colon_on = ((ss & 1u) == 0);

    // shift if leading digit is 0
    int8_t shift = (h10 == 0) ? -(HEX_W + 1)/2 : 0;

    // Optional: suppress leading zero
    if (h10 == 0) {
        // clear that digit’s region
        for (uint8_t ry = 0; ry < HEX_H; ++ry)
            for (uint8_t rx = 0; rx < HEX_W; ++rx)
                Display_SetPixelRC(ROW0 + ry, COL_D0 + rx + shift, 0);
    } else {
        Led_DrawDigit3x5(ROW0, COL_D0 + shift, h10, TIME_LEVEL);
    }

    Led_DrawDigit3x5(ROW0, COL_D1 + shift, h1 , TIME_LEVEL);
    Led_DrawDigit3x5(ROW0, COL_D2 + shift, m10, TIME_LEVEL);
    Led_DrawDigit3x5(ROW0, COL_D3 + shift, m1 , TIME_LEVEL);
    Led_DrawColon(ROW0, COL_COLON + shift, colon_on, TIME_LEVEL);
}


void Led_Init(void)
{
    all_hi_z();

    for (uint16_t i = 0; i < N_PIXELS; ++i) {
        fb[i] = 0;
        pos_map[i] = -1;
    }
    act_len  = 0;
    scan_pos = 0;
    last_hi  = last_lo = -1;

    // ---- build gamma LUT ----
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim2);
    for (int i = 0; i < LUT_SIZE; i++) {
        float norm = (float)i / 255.0f;
        float corrected = powf(norm, GAMMA);
        gamma_lut[i] = (uint16_t)(corrected * arr * 0.5f + 0.5f);
    }
}

void Led_SetGlobalBrightness(uint8_t level)
{
  if (BR_STEPS == 0) { g_master = 0; return; }
  if (level >= BR_STEPS) level = BR_STEPS - 1;
  g_master = level;
}

// Fast powers of 10 (for 32-bit ints)
static const uint32_t POW10[10] = {
  1u, 10u, 100u, 1000u, 10000u,
  100000u, 1000000u, 10000000u, 100000000u, 1000000000u
};

// Tiny 3x5 minus glyph (--- on middle row)
static void draw_minus(uint8_t r0, uint8_t c0, uint8_t level)
{
  for (uint8_t rx = 0; rx < HEX_W; ++rx)
    Display_SetPixelRC(r0 + (HEX_H/2), c0 + rx, level);
}

// Count base-10 digits of a non-negative value (at least 1)
static uint8_t u32_digits(uint32_t v)
{
  uint8_t n = 1;
  while (v >= 10u) { v /= 10u; ++n; }
  return n;
}

void draw_digit(uint8_t r0, uint8_t c0, uint8_t digit, uint8_t level)
{
  if (digit > 9) digit = 9;              // clamp to 0..9
  draw_hex_nibble(r0, c0, digit, level); // reuse 3x5 font for hex 0..9
}

// Centered signed integer in decimal
void draw_int_center(int32_t value, uint8_t level)
{
  const uint8_t r0 = (ROWS - HEX_H) / 2;

  bool neg = (value < 0);
  uint32_t u = neg ? (uint32_t)(-value) : (uint32_t)value;

  // how many glyphs (digits + optional minus)
  uint8_t nd = u32_digits(u);
  uint8_t glyphs = nd + (neg ? 1 : 0);

  // total width = sum of glyph widths + spaces between them
  uint8_t total_w = glyphs * HEX_W + (glyphs > 0 ? (glyphs - 1) * DIGIT_SP : 0);

  // horizontally centered starting column
  uint8_t c0 = (COLS - total_w) / 2;

  uint8_t cx = c0;

  // draw minus if needed
  if (neg) {
    draw_minus(r0, cx, level);
    cx += HEX_W + DIGIT_SP;
  }

  // draw digits from most-significant to least
  for (int i = nd - 1; i >= 0; --i) {
    uint8_t d = (uint8_t)((u / POW10[i]) % 10u);
    draw_digit(r0, cx, d, level);
    cx += HEX_W + DIGIT_SP;
  }
}

void draw_byte_center(uint8_t byte, uint8_t level)
{
    const uint8_t r0 = (ROWS - 5)/2;
    const uint8_t c0 = (COLS - (3*2 + 1))/2;
    draw_hex_byte(r0, c0, byte, level);
}

void draw_hex_byte(uint8_t r0, uint8_t c0, uint8_t byte, uint8_t level)
{
  draw_hex_nibble(r0, c0,               (byte >> 4) & 0xF, level);
  draw_hex_nibble(r0, c0 + HEX_W + 1,    byte        & 0xF, level);
}

void draw_hex_nibble(uint8_t r0, uint8_t c0, uint8_t nib, uint8_t level)
{
  nib &= 0x0F;
  for (uint8_t ry = 0; ry < HEX_H; ++ry) {
    uint8_t row = HEX_3x5[nib][ry];
    for (uint8_t rx = 0; rx < HEX_W; ++rx) {
      uint8_t bit = (row >> (HEX_W - 1 - rx)) & 1u;
      uint8_t rr = r0 + ry, cc = c0 + rx;
      if (rr < ROWS && cc < COLS)
        Display_SetPixelRC(rr, cc, bit ? level : 0);
    }
  }
}

void Display_Clear(void)
{
  // Clear framebuffer
  for (uint16_t i = 0; i < N_PIXELS; ++i) fb[i] = 0;

  // Quickly empty active list (minimal IRQ-off time)
  __disable_irq();
  act_len = 0;
  scan_pos = 0;
  __enable_irq();

  // Reset map outside IRQ-off window
  for (uint16_t i = 0; i < N_PIXELS; ++i) pos_map[i] = -1;

  // Also release any currently driven pair
  release_last();
}

void Display_SetPixelRC(uint8_t r, uint8_t c, uint8_t level)
{
  if (r >= ROWS || c >= COLS) return;
  uint16_t idx = (uint16_t)r * COLS + c;

  if (BR_STEPS == 0) return;
  if (level >= BR_STEPS) level = BR_STEPS - 1;

  uint8_t old = fb[idx];
  fb[idx] = level;

  if (!old && level)        act_add(idx);
  else if (old && !level)   act_remove(idx);
  // else: brightness changed but remains active; leave position stable
}

void Display_SetRegion(uint8_t r0, uint8_t c0, uint8_t w, uint8_t h, uint8_t level)
{
  if (BR_STEPS == 0) return;
  if (level >= BR_STEPS) level = BR_STEPS - 1;

  for (uint8_t r = 0; r < h; ++r){
    for (uint8_t c = 0; c < w; ++c){
      uint8_t rr = (uint8_t)(r0 + r);
      uint8_t cc = (uint8_t)(c0 + c);
      if (rr < ROWS && cc < COLS) {
        uint16_t idx = (uint16_t)rr * COLS + cc;
        uint8_t old = fb[idx];
        fb[idx] = level;
        if (!old && level)        act_add(idx);
        else if (old && !level)   act_remove(idx);
      }
    }
  }
}

/* -------------- TIM2 ISR hooks -------------- */
/* Call these from:
   - HAL_TIM_PeriodElapsedCallback(TIM2) → Led_ScanSlotStart()
   - HAL_TIM_OC_DelayElapsedCallback(TIM2, CH1) → Led_ScanSlotEnd()
*/

void Led_ScanSlotStart(void)
{
    uint16_t len = act_len;
    if (len == 0) { release_last(); return; }

    uint16_t pos = scan_pos;
    if (pos >= len) pos = 0;

    ScanEntry e = act[pos];
    scan_pos = (uint16_t)(pos + 1);

    uint8_t level = fb[e.idx];
    if (!level) { release_last(); return; }

    // Apply gamma from LUT + global brightness
    uint16_t base = gamma_lut[level];
    uint32_t arr  = __HAL_TIM_GET_AUTORELOAD(&htim2);
    uint32_t ccr  = ((uint32_t)base * g_master) / 255u;
    if (ccr > arr) ccr = arr;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);
    drive_pair(e.hi, e.lo);
}


void Led_ScanSlotEnd(void)
{
  // End of ON-time for current slot: tri-state active pins
  release_last();
}


void Led_Suspend(void) {  // public wrapper
  // release_last() and all_hi_z() exist in your file already
  extern void Led_ScanSlotEnd(void); // if release_last is static-only
  Led_ScanSlotEnd();
  // all_hi_z() is static; add a thin public wrapper if needed:
//   extern void led__all_hi_z(void);   // or make all_hi_z() non-static
//   led__all_hi_z();
}
void Led_Resume(void) {
  // no-op; scanning restarts when TIM2 is started again
}
