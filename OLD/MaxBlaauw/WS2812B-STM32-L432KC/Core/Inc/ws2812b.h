#ifndef WS2812B_H
#define WS2812B_H

#include <stdint.h>

/* ── Configuration ─────────────────────────────────────────────────── */
#define WS2812B_NUM_LEDS        60      /* Adjust to your strip length  */

/* PWM compare values (ARR = 99, 80 MHz clock → 800 kHz period)        */
#define WS2812B_PWM_HI          26      /* ~750 ns high  (bit 1)        */
#define WS2812B_PWM_LO          13      /* ~375 ns high  (bit 0)        */
#define WS2812B_PWM_RESET       0       /* 0 V — used for reset pulse   */

/* Reset pulse: WS2812B needs >50 µs = >40 bit-periods @ 800 kHz       */
#define WS2812B_RESET_PERIODS   50

/* Working DMA buffer: 2 LEDs × 24 bits each                           */
#define WS2812B_BUF_SIZE        (2 * 24)

/* ── Public API ────────────────────────────────────────────────────── */

/**
 * Call once after MX_TIM1_Init() and MX_DMA_Init().
 * Starts the DMA-driven PWM stream.
 */
void WS2812B_Init(void);

/**
 * Set LED colour (0-based index).
 * Changes are latched on the next DMA refill cycle automatically.
 */
void WS2812B_SetLED(uint16_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Turn off all LEDs immediately.
 */
void WS2812B_Clear(void);

/* ── Called from DMA IRQ — do not call directly ────────────────────── */
void WS2812B_DMA_HalfCpltCallback(void);
void WS2812B_DMA_CpltCallback(void);

#endif /* WS2812B_H */
