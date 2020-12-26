
# Optimization flags
OPT = -Og -g3
# Uncomment the line below to enable link time optimization
#OPT += -flto

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -DAPP_TIMER_V2
CFLAGS += -DAPP_TIMER_V2_RTC1_ENABLED
CFLAGS += -DBOARD_PCA10056
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DFLOAT_ABI_HARD
CFLAGS += -DNRF52840_XXAA
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -Wall -Werror
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums
CFLAGS += -MD
CFLAGS += -D__HEAP_SIZE=32768
CFLAGS += -D__STACK_SIZE=32768

# C++ flags common to all targets
CXXFLAGS += $(OPT)
# Assembler flags common to all targets
SFLAGS := -g3
SFLAGS += -mcpu=cortex-m4
SFLAGS += -mthumb -mabi=aapcs
SFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
SFLAGS += -DAPP_TIMER_V2
SFLAGS += -DAPP_TIMER_V2_RTC1_ENABLED
SFLAGS += -DBOARD_PCA10056
SFLAGS += -DCONFIG_GPIO_AS_PINRESET
SFLAGS += -DFLOAT_ABI_HARD
SFLAGS += -DNRF52840_XXAA
SFLAGS += -D__HEAP_SIZE=32768
SFLAGS += -D__STACK_SIZE=32768

LINKER_SCRIPT  := $(SDK_DIR)/spi_gcc_nrf52.ld
# Linker flags
LFLAGS += $(OPT)
LFLAGS += -mthumb -mabi=aapcs -L$(SDK_DIR)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LFLAGS += -mcpu=cortex-m4
LFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LFLAGS += -Wl,--gc-sections
# use newlib in nano version
LFLAGS += --specs=nano.specs

LFLAGS += -lc -lnosys -lm