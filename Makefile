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

########################################
#        Mandatory configuration       #
########################################

ifeq ($(CHAIN),)
    CHAIN = ethereum
endif

SUPPORTED_CHAINS = $(shell find makefile_conf/chain/ -type f -name '*.mk'| sed 's/.*\/\(.*\).mk/\1/g' | sort)
ifneq ($(CHAIN),$(filter $(CHAIN),$(SUPPORTED_CHAINS)))
    $(error Unsupported CHAIN. Use one of: $(SUPPORTED_CHAINS))
endif
include ./makefile_conf/chain/$(CHAIN).mk

APPVERSION_M = 1
APPVERSION_N = 12
APPVERSION_P = 0
APPVERSION = $(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)-dev

# Application source files
APP_SOURCE_PATH += src src_features src_plugins
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_STAX TARGET_FLEX))
    APP_SOURCE_PATH += src_nbgl
else
    APP_SOURCE_PATH += src_bagl
endif
APP_SOURCE_FILES += $(filter-out ./ethereum-plugin-sdk/src/main.c, $(wildcard ./ethereum-plugin-sdk/src/*.c))
INCLUDES_PATH += ./ethereum-plugin-sdk/src

ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_STAX TARGET_FLEX))
NETWORK_ICONS_FILE = $(GEN_SRC_DIR)/net_icons.gen.c
NETWORK_ICONS_DIR = $(shell dirname "$(NETWORK_ICONS_FILE)")

$(NETWORK_ICONS_FILE):
	$(shell python3 tools/gen_networks.py "$(NETWORK_ICONS_DIR)")

APP_SOURCE_FILES += $(NETWORK_ICONS_FILE)
endif

# Application icons following guidelines:
# https://developers.ledger.com/docs/embedded-app/design-requirements/#device-icon
ICON_NANOS = icons/nanos_app_chain_$(CHAIN_ID).gif
ICON_NANOX = icons/nanox_app_chain_$(CHAIN_ID).gif
ICON_NANOSP = icons/nanox_app_chain_$(CHAIN_ID).gif
ICON_STAX = icons/stax_app_chain_$(CHAIN_ID).gif
ICON_FLEX = icons/flex_app_chain_$(CHAIN_ID).gif

#prepare hsm generation
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_STAX TARGET_FLEX))
    DEFINES += ICONGLYPH=C_chain_$(CHAIN_ID)_64px
    DEFINES += ICONBITMAP=C_chain_$(CHAIN_ID)_64px_bitmap
    DEFINES += ICONGLYPH_SMALL=C_chain_$(CHAIN_ID)
endif

# Don't define plugin function in the plugin SDK
DEFINES += IS_NOT_A_PLUGIN


# Application allowed derivation curves.
# Possibles curves are: secp256k1, secp256r1, ed25519 and bls12381g1
# If your app needs it, you can specify multiple curves by using:
# `CURVE_APP_LOAD_PARAMS = <curve1> <curve2>`
CURVE_APP_LOAD_PARAMS += secp256k1

# Application allowed derivation paths.
# You should request a specific path for your app.
# This serve as an isolation mechanism.
# Most application will have to request a path according to the BIP-0044
# and SLIP-0044 standards.
# If your app needs it, you can specify multiple path by using:
# `PATH_APP_LOAD_PARAMS = "44'/1'" "45'/1'"`
PATH_APP_LOAD_PARAMS += "45'" "44'/1'"

# Setting to allow building variant applications
# - <VARIANT_PARAM> is the name of the parameter which should be set
#   to specify the variant that should be build.
# - <VARIANT_VALUES> a list of variant that can be build using this app code.
#   * It must at least contains one value.
#   * Values can be the app ticker or anything else but should be unique.
VARIANT_PARAM = CHAIN
VARIANT_VALUES = $(SUPPORTED_CHAINS)

# Activate dependency only for specific CHAIN
ifneq ($(CHAIN),ethereum)
    DEP_APP_LOAD_PARAMS = Ethereum:$(APPVERSION)
    DEFINES_LIB = USE_LIB_ETHEREUM
endif

# Enabling DEBUG flag will enable PRINTF and disable optimizations
#DEBUG = 1

########################################
#     Application custom permissions   #
########################################
# See SDK `include/appflags.h` for the purpose of each permission
#HAVE_APPLICATION_FLAG_DERIVE_MASTER = 1
HAVE_APPLICATION_FLAG_GLOBAL_PIN = 1
HAVE_APPLICATION_FLAG_BOLOS_SETTINGS = 1
HAVE_APPLICATION_FLAG_LIBRARY = 1

########################################
# Application communication interfaces #
########################################
ENABLE_BLUETOOTH = 1
#ENABLE_NFC = 1

########################################
#         NBGL custom features         #
########################################
ENABLE_NBGL_QRCODE = 1
#ENABLE_NBGL_KEYBOARD = 1
#ENABLE_NBGL_KEYPAD = 1

########################################
#          Features disablers          #
########################################
# These advanced settings allow to disable some feature that are by
# default enabled in the SDK `Makefile.standard_app`.
#DISABLE_STANDARD_APP_FILES = 1
#DISABLE_DEFAULT_IO_SEPROXY_BUFFER_SIZE = 1 # To allow custom size declaration
#DISABLE_STANDARD_APP_DEFINES = 1 # Will set all the following disablers
#DISABLE_STANDARD_SNPRINTF = 1
#DISABLE_STANDARD_USB = 1
#DISABLE_STANDARD_WEBUSB = 1
#DISABLE_STANDARD_BAGL_UX_FLOW = 1
#DISABLE_DEBUG_LEDGER_ASSERT = 1
#DISABLE_DEBUG_THROW = 1

########################################
#        Main app configuration        #
########################################

DEFINES += CHAINID_COINNAME=\"$(TICKER)\" CHAIN_ID=$(CHAIN_ID)

# Enabled Features #
include makefile_conf/features.mk

#########################

# Import generic rules from the SDK
include $(BOLOS_SDK)/Makefile.standard_app
