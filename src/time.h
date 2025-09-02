#pragma once
#include "main.h"
#include <stdbool.h> 

void Time_Init(void);
void Time_Awake(void);
void Time_Tick(void);
void draw_hex_byte(uint8_t r0, uint8_t c0, uint8_t val, uint8_t level);