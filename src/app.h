#pragma once
#include "main.h"
#include <stdbool.h> 

typedef enum {
    APP_STATE_CLOCK = 0,     // default digital time
    APP_STATE_ANALOG,        // analog-style clock face
    APP_STATE_DEBUG,         // debug info (sensor values, ticks, etc.)
    APP_STATE_EXPERIMENT     // playground for animations/pixels
} AppState;

extern volatile AppState g_appState;

void App_Init(void);
void App_Loop(void);
void App_GoToSleep(void);