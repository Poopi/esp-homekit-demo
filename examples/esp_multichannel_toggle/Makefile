PROGRAM = sonoff_basic

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	$(abspath ../../components/wifi_config) \
	$(abspath ../../components/wolfssl) \
	$(abspath ../../components/cJSON) \
	$(abspath ../../components/homekit)

FLASH_SIZE ?= 8
FLASH_MODE ?= dout
FLASH_SPEED ?= 40
FLASH_SPEED ?= 40
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x7A000

ifeq ($(HOMEKIT_DEBUG),1)
	EXTRA_CFLAGS += -DHOMEKIT_DEBUG
endif

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS


include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
