#include "nrf_delay.h"
#include "systick.h"

uint32_t systick = 0;

void SysTick_Handler(void)
{
    systick++;
}

uint32_t get_systick_timer(void)
{
    return systick;
}

uint32_t config_systick_timer(void)
{
    /** Configure SysTick to generate an interrupt every millisecond */
    return SysTick_Config(SystemCoreClock / 1000);
}
