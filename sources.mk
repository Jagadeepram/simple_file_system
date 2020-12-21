# Application Source Files
C_SRC := $(APP_DIR)/src/main.c

VPATH := $(APP_DIR)/src/

C_SRC += \
    $(SDK_DIR)/log/src/nrf_log_backend_rtt.c \
    $(SDK_DIR)/log/src/nrf_log_backend_serial.c \
    $(SDK_DIR)/log/src/nrf_log_backend_uart.c \
    $(SDK_DIR)/log/src/nrf_log_default_backends.c \
    $(SDK_DIR)/log/src/nrf_log_frontend.c \
    $(SDK_DIR)/log/src/nrf_log_str_formatter.c \
    $(SDK_DIR)/boards/boards.c \
    $(SDK_DIR)/button/app_button.c \
    $(SDK_DIR)/util/app_error.c \
    $(SDK_DIR)/util/app_error_handler_gcc.c \
    $(SDK_DIR)/util/app_error_weak.c \
    $(SDK_DIR)/scheduler/app_scheduler.c \
    $(SDK_DIR)/timer/app_timer2.c \
    $(SDK_DIR)/util/app_util_platform.c \
    $(SDK_DIR)/timer/drv_rtc.c \
    $(SDK_DIR)/util/nrf_assert.c \
    $(SDK_DIR)/atomic_fifo/nrf_atfifo.c \
    $(SDK_DIR)/atomic/nrf_atomic.c \
    $(SDK_DIR)/balloc/nrf_balloc.c \
    $(SDK_DIR)/fprintf/nrf_fprintf.c \
    $(SDK_DIR)/fprintf/nrf_fprintf_format.c \
    $(SDK_DIR)/memobj/nrf_memobj.c \
    $(SDK_DIR)/ringbuf/nrf_ringbuf.c \
    $(SDK_DIR)/sortlist/nrf_sortlist.c \
    $(SDK_DIR)/strerror/nrf_strerror.c \
    $(SDK_DIR)/nrfx/legacy/nrf_drv_spi.c \
    $(SDK_DIR)/nrfx/legacy/nrf_drv_uart.c \
    $(SDK_DIR)/soc/nrfx_atomic.c \
    $(SDK_DIR)/drivers/src/nrfx_gpiote.c \
    $(SDK_DIR)/drivers/src/prs/nrfx_prs.c \
    $(SDK_DIR)/drivers/src/nrfx_spi.c \
    $(SDK_DIR)/drivers/src/nrfx_spim.c \
    $(SDK_DIR)/drivers/src/nrfx_uart.c \
    $(SDK_DIR)/drivers/src/nrfx_uarte.c \
    $(SDK_DIR)/bsp/bsp.c \
    $(SDK_DIR)/segger_rtt/SEGGER_RTT.c \
    $(SDK_DIR)/segger_rtt/SEGGER_RTT_Syscalls_GCC.c \
    $(SDK_DIR)/segger_rtt/SEGGER_RTT_printf.c \
    $(SDK_DIR)/system_nrf52840.c \


# SOUP or SDK Source FIles from Nordic Semiconductor

L_SRC = $(SDK_DIR)/gcc_startup_nrf52840.S
VPATH += $(SDK_DIR)/

VPATH += \
    $(SDK_DIR)/log/src/ \
    $(SDK_DIR)/boards/ \
    $(SDK_DIR)/button/ \
    $(SDK_DIR)/util/ \
    $(SDK_DIR)/scheduler/ \
    $(SDK_DIR)/timer/ \
    $(SDK_DIR)/atomic_fifo/ \
    $(SDK_DIR)/atomic/ \
    $(SDK_DIR)/balloc/ \
    $(SDK_DIR)/fprintf/ \
    $(SDK_DIR)/memobj/ \
    $(SDK_DIR)/ringbuf/ \
    $(SDK_DIR)/sortlist/ \
    $(SDK_DIR)/strerror/ \
    $(SDK_DIR)/nrfx/legacy/ \
    $(SDK_DIR)/soc/ \
    $(SDK_DIR)/drivers/src/prs/ \
    $(SDK_DIR)/drivers/src/ \
    $(SDK_DIR)/bsp/ \
    $(SDK_DIR)/segger_rtt/ \

INC := \
    $(SDK_DIR) \
    $(SDK_DIR)/modules/nrfx/mdk \
    $(SDK_DIR)/scheduler \
    $(SDK_DIR)/timer \
    $(SDK_DIR)/boards \
    $(SDK_DIR)/strerror \
    $(SDK_DIR)/toolchain/cmsis/include \
    $(SDK_DIR)/util \
    $(APP_DIR) \
    $(APP_DIR)/src \
    $(SDK_DIR)/balloc \
    $(SDK_DIR)/ringbuf \
    $(SDK_DIR)/modules/nrfx/hal \
    $(SDK_DIR)/bsp \
    $(SDK_DIR)/log \
    $(SDK_DIR)/button \
    $(SDK_DIR)/modules/nrfx \
    $(SDK_DIR)/experimental_section_vars \
    $(SDK_DIR)/nrfx/legacy \
    $(SDK_DIR)/delay \
    $(SDK_DIR)/segger_rtt \
    $(SDK_DIR)/atomic_fifo \
    $(SDK_DIR)/nrf_soc_nosd \
    $(SDK_DIR)/atomic \
    $(SDK_DIR)/sortlist \
    $(SDK_DIR)/memobj \
    $(SDK_DIR)/nrfx \
    $(SDK_DIR)/modules/nrfx/drivers/include \
    $(SDK_DIR)/fprintf \
    $(SDK_DIR)/log/src \
