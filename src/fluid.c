#include "fluid.h"
#include <math.h>
#include <string.h>   // memset

// --- Constants ---
static const float GRAVITY      = 0.25f;
static const float DAMPING      = 0.92f;
static const float MAX_VELOCITY = 1.2f;

// --- Globals ---
static Particle particles[FLUID_PARTICLES];

// --- Helpers ---
static inline int xy(int x, int y) {
    if (x < 0) x = 0;
    if (x >= MATRIX_WIDTH)  x = MATRIX_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= MATRIX_HEIGHT) y = MATRIX_HEIGHT - 1;

    // serpentine layout
    if (y & 1) {
        return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
    } else {
        return y * MATRIX_WIDTH + x;
    }
}

// --- Public Functions ---
void fluid_init(void) {
    int index = 0;
    for (int y = MATRIX_HEIGHT - 4; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH && index < FLUID_PARTICLES; x++) {
            particles[index].position.x = (float)x;
            particles[index].position.y = (float)y;
            particles[index].velocity.x = 0.0f;
            particles[index].velocity.y = 0.0f;
            index++;
        }
    }
}

void fluid_update(float ax, float ay, uint32_t dt_ms) {
    // normalize dt to ~60 Hz baseline (16 ms)
    float dt = dt_ms / 16.0f;

    // map accel: rotate axes if needed
    Vector2D accel = {-ay, ax};

    for (int i = 0; i < FLUID_PARTICLES; i++) {
        // velocity update with damping and gravity scaled by dt
        particles[i].velocity.x = particles[i].velocity.x * 0.95f + accel.x * GRAVITY * dt;
        particles[i].velocity.y = particles[i].velocity.y * 0.95f + accel.y * GRAVITY * dt;

        // clamp velocity
        if (particles[i].velocity.x >  MAX_VELOCITY) particles[i].velocity.x =  MAX_VELOCITY;
        if (particles[i].velocity.x < -MAX_VELOCITY) particles[i].velocity.x = -MAX_VELOCITY;
        if (particles[i].velocity.y >  MAX_VELOCITY) particles[i].velocity.y =  MAX_VELOCITY;
        if (particles[i].velocity.y < -MAX_VELOCITY) particles[i].velocity.y = -MAX_VELOCITY;

        // update positions
        float newX = particles[i].position.x + particles[i].velocity.x;
        float newY = particles[i].position.y + particles[i].velocity.y;

        // boundary collisions
        if (newX < 0.0f) {
            newX = 0.0f;
            particles[i].velocity.x = fabsf(particles[i].velocity.x) * DAMPING;
        } else if (newX >= (MATRIX_WIDTH - 1)) {
            newX = MATRIX_WIDTH - 1;
            particles[i].velocity.x = -fabsf(particles[i].velocity.x) * DAMPING;
        }

        if (newY < 0.0f) {
            newY = 0.0f;
            particles[i].velocity.y = fabsf(particles[i].velocity.y) * DAMPING;
        } else if (newY >= (MATRIX_HEIGHT - 1)) {
            newY = MATRIX_HEIGHT - 1;
            particles[i].velocity.y = -fabsf(particles[i].velocity.y) * DAMPING;
        }

        particles[i].position.x = newX;
        particles[i].position.y = newY;
    }

    // simple particle repulsion
    for (int i = 0; i < FLUID_PARTICLES; i++) {
        for (int j = i + 1; j < FLUID_PARTICLES; j++) {
            float dx = particles[j].position.x - particles[i].position.x;
            float dy = particles[j].position.y - particles[i].position.y;
            float dist2 = dx*dx + dy*dy;

            if (dist2 < 1.0f) {
                float angle = atan2f(dy, dx);
                float repX = cosf(angle) * 0.5f;
                float repY = sinf(angle) * 0.5f;

                particles[i].position.x -= repX * 0.3f;
                particles[i].position.y -= repY * 0.3f;
                particles[j].position.x += repX * 0.3f;
                particles[j].position.y += repY * 0.3f;

                Vector2D avg = {
                    (particles[i].velocity.x + particles[j].velocity.x) * 0.5f,
                    (particles[i].velocity.y + particles[j].velocity.y) * 0.5f
                };
                particles[i].velocity = avg;
                particles[j].velocity = avg;
            }
        }
    }
}


void fluid_draw(uint8_t *leds) {
    Display_Clear();   // use your screen clear instead of memset

    for (int i = 0; i < FLUID_PARTICLES; i++) {
        int x = (int)roundf(particles[i].position.x);
        int y = (int)roundf(particles[i].position.y);

        if (x < 0) x = 0;
        if (x >= MATRIX_WIDTH)  x = MATRIX_WIDTH - 1;
        if (y < 0) y = 0;
        if (y >= MATRIX_HEIGHT) y = MATRIX_HEIGHT - 1;

        // check orientation: most drivers use (x,y)
        led_set_pixel(y, x, 15);
    }
}
