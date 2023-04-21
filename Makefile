#*******************************************************************************
#   Ledger App
#   (c) 2017 Ledger
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#*******************************************************************************

ifeq ($(BOLOS_SDK),)
$(error Environment variable BOLOS_SDK is not set)
endif

include $(BOLOS_SDK)/Makefile.defines

DEFINES_LIB = USE_LIB_ETHEREUM
APP_LOAD_PARAMS= --curve secp256k1 $(COMMON_LOAD_PARAMS)
# Allow the app to use path 45 for multi-sig (see BIP45).
APP_LOAD_PARAMS += --path "45'"
# Samsung temporary implementation for wallet ID on 0xda7aba5e/0xc1a551c5
APP_LOAD_PARAMS += --path "1517992542'/1101353413'"

##################
# Define Version #
##################

APPVERSION_M=1
APPVERSION_N=10
APPVERSION_P=2
APPVERSION=$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)-dev
APP_LOAD_FLAGS= --appFlags 0xa40 --dep Ethereum:$(APPVERSION)

###########################
# Set Chain environnement #
###########################

ifeq ($(CHAIN),)
CHAIN=ethereum
endif

SUPPORTED_CHAINS=$(shell find makefile_conf/chain/ -type f -name '*.mk'| sed 's/.*\/\(.*\).mk/\1/g' | sort)

# Check if chain is available
ifeq ($(shell test -s ./makefile_conf/chain/$(CHAIN).mk && echo -n yes), yes)
include ./makefile_conf/chain/$(CHAIN).mk
else
$(error Unsupported CHAIN - use $(SUPPORTED_CHAINS))
endif

#########
# Other #
#########

APP_LOAD_PARAMS += $(APP_LOAD_FLAGS) --path "44'/1'"
DEFINES += $(DEFINES_LIB)

#prepare hsm generation
ifeq ($(TARGET_NAME),TARGET_NANOS)
ICONNAME=icons/nanos_app_$(CHAIN).gif
else
ICONNAME=icons/nanox_app_$(CHAIN).gif
endif

################
# Default rule #
################
all: default

############
# Platform #
############

DEFINES   += OS_IO_SEPROXYHAL
DEFINES   += HAVE_BAGL HAVE_SPRINTF HAVE_SNPRINTF_FORMAT_U
DEFINES   += HAVE_IO_USB HAVE_L4_USBLIB IO_USB_MAX_ENDPOINTS=4 IO_HID_EP_LENGTH=64 HAVE_USB_APDU
DEFINES   += LEDGER_MAJOR_VERSION=$(APPVERSION_M) LEDGER_MINOR_VERSION=$(APPVERSION_N) LEDGER_PATCH_VERSION=$(APPVERSION_P)

# U2F
DEFINES   += HAVE_U2F HAVE_IO_U2F
DEFINES   += U2F_PROXY_MAGIC=\"w0w\"
DEFINES   += USB_SEGMENT_SIZE=64
DEFINES   += BLE_SEGMENT_SIZE=32 #max MTU, min 20
DEFINES   += UNUSED\(x\)=\(void\)x
DEFINES   += APPVERSION=\"$(APPVERSION)\"
DEFINES   += HAVE_UX_FLOW

#WEBUSB_URL     = www.ledgerwallet.com
#DEFINES       += HAVE_WEBUSB WEBUSB_URL_SIZE_B=$(shell echo -n $(WEBUSB_URL) | wc -c) WEBUSB_URL=$(shell echo -n $(WEBUSB_URL) | sed -e "s/./\\\'\0\\\',/g")

DEFINES   += HAVE_WEBUSB WEBUSB_URL_SIZE_B=0 WEBUSB_URL=""

ifeq ($(TARGET_NAME),TARGET_NANOX)
DEFINES   += HAVE_BLE BLE_COMMAND_TIMEOUT_MS=2000
DEFINES   += HAVE_BLE_APDU # basic ledger apdu transport over BLE
endif

ifeq ($(TARGET_NAME),TARGET_NANOS)
DEFINES   += IO_SEPROXYHAL_BUFFER_SIZE_B=72
DEFINES   += HAVE_WALLET_ID_SDK
else
DEFINES   += IO_SEPROXYHAL_BUFFER_SIZE_B=300
DEFINES   += HAVE_GLO096
DEFINES   += HAVE_BAGL BAGL_WIDTH=128 BAGL_HEIGHT=64
DEFINES   += HAVE_BAGL_ELLIPSIS # long label truncation feature
DEFINES   += HAVE_BAGL_FONT_OPEN_SANS_REGULAR_11PX
DEFINES   += HAVE_BAGL_FONT_OPEN_SANS_EXTRABOLD_11PX
DEFINES   += HAVE_BAGL_FONT_OPEN_SANS_LIGHT_16PX
endif

# Enables direct data signing without having to specify it in the settings. Useful when testing with speculos.
ALLOW_DATA:=0
ifneq ($(ALLOW_DATA),0)
DEFINES += HAVE_ALLOW_DATA
endif

# Bypass the signature verification for setExternalPlugin, setPlugin, provideERC20TokenInfo and provideNFTInfo calls
BYPASS_SIGNATURES:=0
ifneq ($(BYPASS_SIGNATURES),0)
DEFINES += HAVE_BYPASS_SIGNATURES
endif

# NFTs
ifneq ($(TARGET_NAME),TARGET_NANOS)
DEFINES	+= HAVE_NFT_SUPPORT
# Enable the NFT testing key
NFT_TESTING_KEY:=0
ifneq ($(NFT_TESTING_KEY),0)
DEFINES += HAVE_NFT_TESTING_KEY
endif
endif

# Dynamic memory allocator
ifneq ($(TARGET_NAME),TARGET_NANOS)
DEFINES += HAVE_DYN_MEM_ALLOC
endif

# EIP-712
ifneq ($(TARGET_NAME),TARGET_NANOS)
DEFINES	+= HAVE_EIP712_FULL_SUPPORT
endif

# CryptoAssetsList key
CAL_TEST_KEY:=0
CAL_CI_KEY:=0
ifneq ($(CAL_TEST_KEY),0)
DEFINES += HAVE_CAL_TEST_KEY
endif
ifneq ($(CAL_CI_KEY),0)
DEFINES += HAVE_CAL_CI_KEY
endif

# ENS
ifneq ($(TARGET_NAME),TARGET_NANOS)
DEFINES += HAVE_DOMAIN_NAME
DOMAIN_NAME_TEST_KEY:=0
ifneq ($(DOMAIN_NAME_TEST_KEY),0)
DEFINES += HAVE_DOMAIN_NAME_TEST_KEY
endif
endif

# Enabling debug PRINTF
DEBUG:=0
ifneq ($(DEBUG),0)
DEFINES += HAVE_STACK_OVERFLOW_CHECK
ifeq ($(TARGET_NAME),TARGET_NANOS)
DEFINES   += HAVE_PRINTF PRINTF=screen_printf
else
DEFINES   += HAVE_PRINTF PRINTF=mcu_usb_printf
endif
else
DEFINES   += PRINTF\(...\)=
endif

ifneq ($(NOCONSENT),)
DEFINES   += NO_CONSENT
endif

#DEFINES   += HAVE_TOKENS_LIST # Do not activate external ERC-20 support yet

##############
#  Compiler  #
##############
ifneq ($(BOLOS_ENV),)
$(info BOLOS_ENV=$(BOLOS_ENV))
CLANGPATH := $(BOLOS_ENV)/clang-arm-fropi/bin/
GCCPATH := $(BOLOS_ENV)/gcc-arm-none-eabi-5_3-2016q1/bin/
else
$(info BOLOS_ENV is not set: falling back to CLANGPATH and GCCPATH)
endif
ifeq ($(CLANGPATH),)
$(info CLANGPATH is not set: clang will be used from PATH)
endif
ifeq ($(GCCPATH),)
$(info GCCPATH is not set: arm-none-eabi-* will be used from PATH)
endif

CC       := $(CLANGPATH)clang

#CFLAGS   += -O0
CFLAGS   += -Oz -Wno-format-invalid-specifier -Wno-format-extra-args

AS     := $(GCCPATH)arm-none-eabi-gcc

LD       := $(GCCPATH)arm-none-eabi-gcc
LDFLAGS  += -O3 -Os
LDLIBS   += -lm -lgcc -lc

# import rules to compile glyphs(/pone)
include $(BOLOS_SDK)/Makefile.glyphs

### variables processed by the common makefile.rules of the SDK to grab source files and include dirs
APP_SOURCE_PATH  += src_common src src_features src_plugins
SDK_SOURCE_PATH  += lib_stusb lib_stusb_impl lib_u2f
SDK_SOURCE_PATH  += lib_ux
ifeq ($(TARGET_NAME),TARGET_NANOX)
SDK_SOURCE_PATH  += lib_blewbxx lib_blewbxx_impl
endif
APP_SOURCE_PATH  += src_bagl

### initialize plugin SDK submodule if needed, rebuild it, and warn if a difference is noticed
ifeq ($(CHAIN),ethereum)
ifneq ($(shell git submodule status | grep '^[-+]'),)
$(info INFO: Need to reinitialize git submodules)
$(shell git submodule update --init)
endif

# rebuild SDK
$(shell python3 tools/build_sdk.py)

# check if a difference is noticed (fail if it happens in CI build)
ifneq ($(shell git status | grep 'ethereum-plugin-sdk'),)
ifneq ($(JENKINS_URL),)
$(error ERROR: please update ethereum-plugin-sdk submodule first)
else
$(warning WARNING: please update ethereum-plugin-sdk submodule first)
endif
endif
endif

load: all
	python3 -m ledgerblue.loadApp $(APP_LOAD_PARAMS)

delete:
	python3 -m ledgerblue.deleteApp $(COMMON_DELETE_PARAMS)

install_tests:
	cd tests/zemu/ && (yarn install || sudo yarn install)

run_tests:
	cd tests/zemu/ && (yarn test || sudo yarn test)

test: install_tests run_tests

unit-test:
	make -C tests/unit

# import generic rules from the sdk
include $(BOLOS_SDK)/Makefile.rules

#add dependency on custom makefile filename
dep/%.d: %.c Makefile

listvariants:
	@echo VARIANTS CHAIN $(SUPPORTED_CHAINS)
