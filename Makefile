WEST ?= west
BOARD ?= nice_nano_v2
ZMK_CONFIG := $(abspath config)
CRADIO_MODULE := $(abspath local-modules/cradio-led-indicator)
BUILD_DIR := $(abspath build)
WEST_WORKSPACE := $(abspath .zmk/workspace)
WEST_MANIFEST := $(WEST_WORKSPACE)/config/west.yml
ZMK_APP := $(WEST_WORKSPACE)/zmk/app
ZEPHYR_BASE := $(WEST_WORKSPACE)/zephyr
BUILD_TOOLS_BIN := $(abspath .zmk/build-venv/bin)

.PHONY: test west-update build-left build-right

test: west-update build-left build-right

west-update:
	mkdir -p $(WEST_WORKSPACE)/config
	cp config/west.yml $(WEST_MANIFEST)
	if [ ! -f $(WEST_WORKSPACE)/.west/config ]; then PATH=$(BUILD_TOOLS_BIN):$$PATH; cd $(WEST_WORKSPACE) && $(WEST) init -l config; fi
	PATH=$(BUILD_TOOLS_BIN):$$PATH; cd $(WEST_WORKSPACE) && $(WEST) update

build-left:
	PATH=$(BUILD_TOOLS_BIN):$$PATH; ZEPHYR_BASE=$(ZEPHYR_BASE); export PATH ZEPHYR_BASE; cd $(WEST_WORKSPACE) && $(WEST) build -s $(ZMK_APP) -d $(BUILD_DIR)/cradio_left -b $(BOARD) -- \
		-DSHIELD=cradio_left \
		-DZephyr_DIR=$(ZEPHYR_BASE)/share/zephyr-package/cmake \
		-DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb \
		-DGNUARMEMB_TOOLCHAIN_PATH=/usr \
		-DZMK_CONFIG=$(ZMK_CONFIG) \
		-DZMK_EXTRA_MODULES=$(CRADIO_MODULE)

build-right:
	PATH=$(BUILD_TOOLS_BIN):$$PATH; ZEPHYR_BASE=$(ZEPHYR_BASE); export PATH ZEPHYR_BASE; cd $(WEST_WORKSPACE) && $(WEST) build -s $(ZMK_APP) -d $(BUILD_DIR)/cradio_right -b $(BOARD) -- \
		-DSHIELD=cradio_right \
		-DZephyr_DIR=$(ZEPHYR_BASE)/share/zephyr-package/cmake \
		-DZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb \
		-DGNUARMEMB_TOOLCHAIN_PATH=/usr \
		-DZMK_CONFIG=$(ZMK_CONFIG) \
		-DZMK_EXTRA_MODULES=$(CRADIO_MODULE)
