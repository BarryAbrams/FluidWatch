#include "app.h"
#include "led_driver.h"
#include "main.h"
#include "ICM426xx.h"
#include "fluid.h"
#include "time.h"

volatile uint32_t last_motion_ms = 0;
volatile uint32_t last_tick = 0;
volatile AppState g_appState = APP_STATE_DEBUG;  // default
uint8_t status = 0x00;
static uint32_t last_physics = 0;

void App_Init(void) {
    Time_Init();
    uint8_t hh, mm, ss;
    Time_Get(&hh, &mm, &ss);
    Led_CurtainReveal(300, hh, mm, ss);

    last_motion_ms = HAL_GetTick();
    last_tick = HAL_GetTick();
    App_SetState(g_appState);  // start in clock mode

    fluid_init();  // 40 particles for example
}

void App_Loop(void) {
    uint32_t now = HAL_GetTick();
    uint32_t wakeTime = 2000;

    switch (g_appState) {
        case APP_STATE_CLOCK:
        case APP_STATE_ANALOG:
            wakeTime = 2000;
            if ((now - last_tick) >= 1000) {
                last_tick = now;
                uint8_t hh, mm, ss;
                Time_Get(&hh, &mm, &ss);
                if (g_appState == APP_STATE_CLOCK) {
                    Led_DrawClock(hh, mm, ss);
                }
            }
            break;

        case APP_STATE_DEBUG:
        case APP_STATE_EXPERIMENT:
            wakeTime = 100000;
            // Led_AllOn(50);
            break;
    }

    // Sleep check
    // static uint32_t last_wake_ms = 0;
    // if ((now - last_motion_ms) > wakeTime) {
    //     App_GoToSleep();
    //     last_wake_ms = HAL_GetTick();   // remember when we last woke up
    // }
}

void App_GoToSleep(void)
{
    uint8_t hh, mm, ss;

    // Clear any stale interrupts before sleep
    ICM426xx_interruptStatus();
    __HAL_GPIO_EXTI_CLEAR_IT(ACC_INT_Pin);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    // Animate closing curtains
    switch (g_appState) {
        case APP_STATE_CLOCK:
            Time_Get(&hh, &mm, &ss);
            Led_CurtainClose(300, hh, mm, ss);
            break;
        case APP_STATE_ANALOG:
            // TODO: analog-specific c lose
            break;
        case APP_STATE_DEBUG:
        case APP_STATE_EXPERIMENT:
            // TODO: debug/experiment close
            break;
    }

    Display_Clear();
    HAL_Delay(10);

    // Prep for STOP2
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_HIGH); // PA0 = WKUP1
    HAL_SuspendTick();

    // Enter STOP2 (I/O + EXTI still alive)
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

    // --- Wakeup sequence ---
    HAL_ResumeTick();
    SystemClock_Config();   // re-init clocks
    last_motion_ms = HAL_GetTick();

    Display_Clear();

    // Animate opening curtains
    switch (g_appState) {
        case APP_STATE_CLOCK:
            Time_Get(&hh, &mm, &ss);
            Led_CurtainReveal(300, hh, mm, ss);
            break;
        case APP_STATE_ANALOG:
            // TODO: analog-specific open
            break;
        case APP_STATE_DEBUG:
        case APP_STATE_EXPERIMENT:
            // TODO: debug/experiment open
            break;
    }
}



void App_SetState(AppState newState) {
    g_appState = newState;
    Display_Clear(); // optional: clear screen when switching modes
}

void App_SetLastTick(void) {
    last_tick = HAL_GetTick();
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ACC_INT_Pin) {
        ICM426xx_interruptStatus();
        last_motion_ms = HAL_GetTick();
    }
}
