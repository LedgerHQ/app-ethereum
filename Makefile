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

APPVERSION_M=1
APPVERSION_N=1
APPVERSION_P=6
APPVERSION=$(APPVERSION_M).$(APPVERSION_N).$(APPVERSION_P)
APP_LOAD_FLAGS= --appFlags 0x40 --dep Ethereum:$(APPVERSION)

ifeq ($(CHAIN),)
CHAIN=ethereum
endif

ifeq ($(CHAIN),ethereum)
#TODO : Fix in 1.4.3
#APP_LOAD_PARAMS += --path "44'/60'"
APP_LOAD_PARAMS += --path "44'"
DEFINES += CHAINID_UPCASE=\"ETHEREUM\" CHAINID_COINNAME=\"ETH\" CHAIN_KIND=CHAIN_KIND_ETHEREUM CHAIN_ID=0
APPNAME = "Ethereum"
DEFINES_LIB=
APP_LOAD_FLAGS=--appFlags 0x840
else ifeq ($(CHAIN),ellaism)
APP_LOAD_PARAMS += --path "44'/163'"
DEFINES += CHAINID_UPCASE=\"ELLA\" CHAINID_COINNAME=\"ELLA\" CHAIN_KIND=CHAIN_KIND_ELLAISM CHAIN_ID=64
APPNAME = "Ellaism"
else ifeq ($(CHAIN),ethereum_classic)
APP_LOAD_PARAMS += --path "44'/61'"
DEFINES += CHAINID_UPCASE=\"ETC\" CHAINID_COINNAME=\"ETC\" CHAIN_KIND=CHAIN_KIND_ETHEREUM_CLASSIC CHAIN_ID=61
APPNAME = "Ethereum Classic"
else ifeq ($(CHAIN),ethersocial)
APP_LOAD_PARAMS += --path "44'/31102'"
DEFINES += CHAINID_UPCASE=\"ETHERSOCIAL\" CHAINID_COINNAME=\"ESN\" CHAIN_KIND=CHAIN_KIND_ETHERSOCIAL CHAIN_ID=31102
APPNAME = "Ethersocial"
else ifeq ($(CHAIN),ether1)
APP_LOAD_PARAMS += --path "44'/1313114'"
DEFINES += CHAINID_UPCASE=\"ETHER1\" CHAINID_COINNAME=\"ETHO\" CHAIN_KIND=CHAIN_KIND_ETHER1 CHAIN_ID=1313114
APPNAME = "Ether-1"
else ifeq ($(CHAIN),expanse)
APP_LOAD_PARAMS += --path "44'/40'"
DEFINES += CHAINID_UPCASE=\"EXPANSE\" CHAINID_COINNAME=\"EXP\" CHAIN_KIND=CHAIN_KIND_EXPANSE CHAIN_ID=2
APPNAME = "Expanse"
else ifeq ($(CHAIN),pirl)
APP_LOAD_PARAMS += --path "44'/164'"
DEFINES += CHAINID_UPCASE=\"PIRL\" CHAINID_COINNAME=\"PIRL\" CHAIN_KIND=CHAIN_KIND_PIRL CHAIN_ID=3125659152
APPNAME = "Pirl"
else ifeq ($(CHAIN),poa)
APP_LOAD_PARAMS += --path "44'/60'"
DEFINES += CHAINID_UPCASE=\"POA\" CHAINID_COINNAME=\"POA\" CHAIN_KIND=CHAIN_KIND_POA CHAIN_ID=99
APPNAME = "POA"
else ifeq ($(CHAIN),rsk)
APP_LOAD_PARAMS += --path "44'/137'"
DEFINES += CHAINID_UPCASE=\"RSK\" CHAINID_COINNAME=\"SBTC\" CHAIN_KIND=CHAIN_KIND_RSK CHAIN_ID=30
APPNAME = "RSK"
else ifeq ($(CHAIN),rsk_testnet)
APP_LOAD_PARAMS += --path "44'/37310'"
DEFINES += CHAINID_UPCASE=\"RSKTESTNET\" CHAINID_COINNAME=\"SBTC\" CHAIN_KIND=CHAIN_KIND_RSK CHAIN_ID=31
APPNAME = "RSK Test"
else ifeq ($(CHAIN),ubiq)
APP_LOAD_PARAMS += --path "44'/108'"
DEFINES += CHAINID_UPCASE=\"UBIQ\" CHAINID_COINNAME=\"UBQ\" CHAIN_KIND=CHAIN_KIND_UBIQ CHAIN_ID=8
APPNAME = "Ubiq"
else ifeq ($(CHAIN),akroma)
APP_LOAD_PARAMS += --path "44'/200625'"
DEFINES += CHAINID_UPCASE=\"AKA\" CHAINID_COINNAME=\"AKA\" CHAIN_KIND=CHAIN_KIND_AKROMA CHAIN_ID=200625
APPNAME = "Akroma"
else ifeq ($(CHAIN),wanchain)
APP_LOAD_PARAMS += --path "44'/5718350'"
DEFINES += CHAINID_UPCASE=\"WAN\" CHAINID_COINNAME=\"WAN\" CHAIN_KIND=CHAIN_KIND_WANCHAIN CHAIN_ID=1
APPNAME = "Wanchain"
else ifeq ($(CHAIN),kusd)
APP_LOAD_PARAMS += --path "44'/91927009'"
DEFINES += CHAINID_UPCASE=\"KUSD\" CHAINID_COINNAME=\"KUSD\" CHAIN_KIND=CHAIN_KIND_KUSD CHAIN_ID=1
APPNAME = "kUSD"
else ifeq ($(CHAIN),musicoin)
APP_LOAD_PARAMS += --path "44'/184'"
DEFINES += CHAINID_UPCASE=\"MUSICOIN\" CHAINID_COINNAME=\"MUSIC\" CHAIN_KIND=CHAIN_KIND_MUSICOIN CHAIN_ID=7762959
APPNAME = "Musicoin"
else ifeq ($(CHAIN),callisto)
APP_LOAD_PARAMS += --path "44'/820'"
DEFINES += CHAINID_UPCASE=\"CALLISTO\" CHAINID_COINNAME=\"CLO\" CHAIN_KIND=CHAIN_KIND_CALLISTO CHAIN_ID=820
APPNAME = "Callisto"
else ifeq ($(CHAIN),ethergem)
APP_LOAD_PARAMS += --path "44'/1987'"
DEFINES += CHAINID_UPCASE=\"ETHERGEM\" CHAINID_COINNAME=\"EGEM\" CHAIN_KIND=CHAIN_KIND_ETHERGEM CHAIN_ID=1987
APPNAME = "EtherGem"
else ifeq ($(CHAIN),atheios)
APP_LOAD_PARAMS += --path "44'/1620'"
DEFINES += CHAINID_UPCASE=\"ATHEIOS\" CHAINID_COINNAME=\"ATH\" CHAIN_KIND=CHAIN_KIND_ATHEIOS CHAIN_ID=1620
APPNAME = "Atheios"
else ifeq ($(CHAIN),gochain)
APP_LOAD_PARAMS += --path "44'/6060'"
DEFINES += CHAINID_UPCASE=\"GOCHAIN\" CHAINID_COINNAME=\"GO\" CHAIN_KIND=CHAIN_KIND_GOCHAIN CHAIN_ID=60
APPNAME = "GoChain"
else ifeq ($(CHAIN),eosclassic)
APP_LOAD_PARAMS += --path "44'/2018'"
DEFINES += CHAINID_UPCASE=\"EOSCLASSIC\" CHAINID_COINNAME=\"EOSC\" CHAIN_KIND=CHAIN_KIND_EOSCLASSIC CHAIN_ID=20
APPNAME = "EOSClassic"
else ifeq ($(CHAIN),mix)
APP_LOAD_PARAMS += --path "44'/76'"
DEFINES += CHAINID_UPCASE=\"MIX\" CHAINID_COINNAME=\"MIX\" CHAIN_KIND=CHAIN_KIND_MIX CHAIN_ID=76
APPNAME = "Mix"
else ifeq ($(CHAIN),aquachain)
APP_LOAD_PARAMS += --path "44'/61717561'"
DEFINES += CHAINID_UPCASE=\"AQUACHAIN\" CHAINID_COINNAME=\"AQUA\" CHAIN_KIND=CHAIN_KIND_AQUACHAIN CHAIN_ID=61717561
APPNAME = "Aquachain"
else
ifeq ($(filter clean,$(MAKECMDGOALS)),)
$(error Unsupported CHAIN - use ethereum, ethereum_classic, expanse, poa, rsk, rsk_testnet, ubiq, wanchain, kusd, musicoin, pirl, akroma, atheios, callisto, ethersocial, ellaism, ether1, ethergem, gochain, eosclassic, mix, aquachain)
endif
endif

APP_LOAD_PARAMS += $(APP_LOAD_FLAGS) --path "44'/1'"
DEFINES += $(DEFINES_LIB)

#prepare hsm generation
ifeq ($(TARGET_NAME),TARGET_BLUE)
ICONNAME=blue_app_$(CHAIN).gif
else
ICONNAME=nanos_app_$(CHAIN).gif
endif

################
# Default rule #
################
all: default

############
# Platform #
############

DEFINES   += OS_IO_SEPROXYHAL IO_SEPROXYHAL_BUFFER_SIZE_B=128
DEFINES   += HAVE_BAGL HAVE_SPRINTF
#DEFINES   += HAVE_PRINTF PRINTF=screen_printf
DEFINES   += PRINTF\(...\)=
DEFINES   += HAVE_IO_USB HAVE_L4_USBLIB IO_USB_MAX_ENDPOINTS=6 IO_HID_EP_LENGTH=64 HAVE_USB_APDU
DEFINES   += LEDGER_MAJOR_VERSION=$(APPVERSION_M) LEDGER_MINOR_VERSION=$(APPVERSION_N) LEDGER_PATCH_VERSION=$(APPVERSION_P)

# U2F
DEFINES   += HAVE_U2F HAVE_IO_U2F
DEFINES   += U2F_PROXY_MAGIC=\"w0w\"
DEFINES   += USB_SEGMENT_SIZE=64
DEFINES   += BLE_SEGMENT_SIZE=32 #max MTU, min 20

WEBUSB_URL     = www.ledgerwallet.com
DEFINES       += HAVE_WEBUSB WEBUSB_URL_SIZE_B=$(shell echo -n $(WEBUSB_URL) | wc -c) WEBUSB_URL=$(shell echo -n $(WEBUSB_URL) | sed -e "s/./\\\'\0\\\',/g")

DEFINES   += UNUSED\(x\)=\(void\)x
DEFINES   += APPVERSION=\"$(APPVERSION)\"

DEFINES   += CX_COMPLIANCE_141

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
CFLAGS   += -O3 -Os

AS     := $(GCCPATH)arm-none-eabi-gcc

LD       := $(GCCPATH)arm-none-eabi-gcc
LDFLAGS  += -O3 -Os
LDLIBS   += -lm -lgcc -lc 

# import rules to compile glyphs(/pone)
include $(BOLOS_SDK)/Makefile.glyphs

### variables processed by the common makefile.rules of the SDK to grab source files and include dirs
APP_SOURCE_PATH  += src_genericwallet src_common src 
SDK_SOURCE_PATH  += lib_stusb lib_stusb_impl lib_u2f

load: all
	python -m ledgerblue.loadApp $(APP_LOAD_PARAMS)

delete:
	python -m ledgerblue.deleteApp $(COMMON_DELETE_PARAMS)

# import generic rules from the sdk
include $(BOLOS_SDK)/Makefile.rules

#add dependency on custom makefile filename
dep/%.d: %.c Makefile

listvariants:
	@echo VARIANTS CHAIN ethereum ethereum_classic expanse poa rsk rsk_testnet ubiq wanchain kusd pirl akroma atheios callisto ethersocial ether1 gochain # musicoin ellaism ethergem  eosclassic mix
