# Makefile for onDosis project
# This file is designed to execute on windows OS.
# Pre-requisite:
#	IAR compiler: Download it from IAR website
#	Makefile software: K:\110613_OnDosis_Amber\Tools\SW\make_software\msysGit-netinstall-1.9.4-preview20140929

# Compiler
CC = arm-none-eabi-gcc
# Object Copy
OBJCOPY = arm-none-eabi-objcopy
# Size
SIZE  = arm-none-eabi-size

# echo suspend
ifeq ($(VERBOSE),1)
  NO_ECHO :=
else
  NO_ECHO := @
endif

#CURRENT_DIR ?= $(shell pwd)
APP_DIR = application
SDK_DIR = sdk
BUILD_DIR ?= _build

# Binary file name
TARGET_NAME ?= spi_gcc_nRF52
TARGET_OUT ?= $(TARGET_NAME).out
TARGET_HEX ?= $(TARGET_NAME).hex
TARGET_BIN ?= $(TARGET_NAME).bin

# Include a file containing source files
include sources.mk
include flag_settings.mk

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(C_SRC)))
OBJS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(notdir $(L_SRC)))

DEPS := $(OBJS:.o=.d)
INC_FLAGS := $(addprefix -I,$(INC))
INC_FLAGS += $(addprefix -I,$(VPATH))

all: $(BUILD_DIR)/$(TARGET_HEX) $(BUILD_DIR)/$(TARGET_BIN) $(BUILD_DIR)/$(TARGET_OUT)

$(BUILD_DIR)/$(TARGET_OUT): $(OBJS)
	@echo "Linking $<";
	$(NO_ECHO) $(CC) $(OBJS) $(INC_FLAGS) $(LFLAGS) -Wl,-Map=$(@:.out=.map) -o $@
	$(SIZE) $@

# assembly
$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "Compiling $<";
	$(NO_ECHO) $(CC) $(SFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<";
	$(NO_ECHO) $(CC) $(CFLAGS) $(INC_FLAGS) -c $< -o $@

# Create binary .hex file from the .out file
%.hex: %.out
	@echo "Creating $@";
	$(NO_ECHO) $(OBJCOPY) -O ihex $< $@

# Create binary .bin file from the .out file
%.bin: %.out
	@echo "Creating $@";
	$(NO_ECHO) $(OBJCOPY) -O binary $< $@

.PHONY: clean all flash erase

# Flash the program
flash: $(BUILD_DIR)/$(TARGET_HEX)
	@echo Flashing: $(BUILD_DIR)/$(TARGET_HEX)
	nrfjprog -f nrf52 --program $(BUILD_DIR)/$(TARGET_HEX) --sectorerase
	nrfjprog -f nrf52 --reset

erase:
	nrfjprog -f nrf52 --eraseall

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)