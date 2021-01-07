#include <stdlib.h>
#include <string.h>
#include "boards.h"
#include "app_timer.h"
#include "sdk_config.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_error.h"
#include "app_error.h"
#include "spi.h"
#include "nrf_drv_spi.h"

/* SPI timeout period : 0.5 seconds */
#define SPI_TX_CHECK_PERIOD_MS   500

#define SPI_INSTANCE  0 /**< SPI instance index. */
static const nrf_drv_spi_t m_spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE); /**< SPI instance. */
static volatile bool m_spi_xfer_done; /**< Flag used to indicate that SPI instance completed the transfer. */

static bool m_spi_init_done = false;
static volatile uint8_t m_spi_txrx_timeout = 0;

/* Create timer */
APP_TIMER_DEF(m_spi_txrx_timer);

/**
 * @brief SPI user event handler.
 * @param event
 */
static void spi_event_handler(const nrf_drv_spi_evt_t * p_event, void * p_context)
{
    m_spi_xfer_done = true;
}

static void spi_txrx_timer_handler(void *p_context)
{
    UNUSED_PARAMETER(p_context);
    m_spi_txrx_timeout = 1;
}

ret_code_t spi_txrx(const uint8_t *tx_buff, size_t tx_len, uint8_t *rx_buff, size_t rx_len)
{
    ret_code_t err_code;

    const nrfx_spim_xfer_desc_t spim_xfer_desc =
    {
        .p_tx_buffer = tx_buff,
        .tx_length = tx_len,
        .p_rx_buffer = rx_buff,
        .rx_length = rx_len
    };

    m_spi_xfer_done = false;
    app_timer_start(m_spi_txrx_timer, APP_TIMER_TICKS(SPI_TX_CHECK_PERIOD_MS), NULL);

    err_code = nrfx_spim_xfer(&m_spi.u.spim, &spim_xfer_desc, 0);

    while ((m_spi_xfer_done == false) && (m_spi_txrx_timeout == 0))
        ;

    app_timer_stop(m_spi_txrx_timer);

    if (m_spi_txrx_timeout == 1)
    {
        m_spi_txrx_timeout = 0;
        err_code = (err_code == NRF_SUCCESS) ? NRF_ERROR_TIMEOUT : err_code;
    }

    return err_code;
}

void spi_init(void)
{
    if (m_spi_init_done == false)
    {
        nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;

        spi_config.miso_pin = SPI_MISO_PIN;
        spi_config.mosi_pin = SPI_MOSI_PIN;
        spi_config.sck_pin = SPI_CLK_PIN;
        spi_config.frequency = NRF_DRV_SPI_FREQ_8M;

        nrf_drv_spi_init(&m_spi, &spi_config, spi_event_handler, NULL);

        /* Create timer for timeout */
        app_timer_create(&m_spi_txrx_timer, APP_TIMER_MODE_SINGLE_SHOT, spi_txrx_timer_handler);

        m_spi_init_done = true;
    }
}

void spi_uninit(void)
{
    if (m_spi_init_done == true)
    {
        nrf_drv_spi_uninit(&m_spi);
        m_spi_init_done = false;
    }
}
