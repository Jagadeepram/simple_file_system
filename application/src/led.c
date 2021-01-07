#include <stdlib.h>
#include <string.h>
#include "boards.h"
#include "app_timer.h"
#include "sdk_config.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_error.h"
#include "app_error.h"
#include "nrf_drv_clock.h"


/* LED Blink period */
#define LED_BLINK_MS 250

/* Create timer */
APP_TIMER_DEF(m_led_blink_timer);

static uint8_t m_led_init_done = false;

static void led_blink_timer_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);
    bsp_board_led_invert(BSP_BOARD_LED_0);
}

void lfclk_request(void)
{
    ret_code_t err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
    nrf_drv_clock_lfclk_request(NULL);
}


void led_init(void)
{
    if (m_led_init_done == false)
    {
        /* Create timer for timeout */
        app_timer_create(&m_led_blink_timer, APP_TIMER_MODE_REPEATED, led_blink_timer_handler);
        app_timer_start(m_led_blink_timer, APP_TIMER_TICKS(LED_BLINK_MS), NULL);
        m_led_init_done = true;
    }
}

