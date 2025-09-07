#ifndef FLUID_H
#define FLUID_H

#include <stdint.h>

#define MATRIX_WIDTH    16
#define MATRIX_HEIGHT   16
#define NUM_LEDS        (MATRIX_WIDTH * MATRIX_HEIGHT)
#define FLUID_PARTICLES 64

typedef struct {
    float x;
    float y;
} Vector2D;

typedef struct {
    Vector2D position;
    Vector2D velocity;
} Particle;

void fluid_init(void);
void fluid_update(float ax, float ay, uint32_t dt_ms);
void fluid_draw(uint8_t *leds);   // 0 = off, 1 = on

#endif
