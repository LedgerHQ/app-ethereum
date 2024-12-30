cmake_minimum_required(VERSION 3.14)

# project information
project(EthereumAppFuzzer
        VERSION 1.0
	      DESCRIPTION "Eth Fuzzer"
        LANGUAGES C)

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "Fuzzer needs to be built with Clang")
endif()

# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# default fuzz device target
if (NOT TARGET_DEVICE)
  set(TARGET_DEVICE "flex")
endif()

if (NOT DEFINED BOLOS_SDK)
  set(BOLOS_SDK /opt/${TARGET_DEVICE}-secure-sdk)
endif()

# compatible with ClusterFuzzLite
if (NOT DEFINED ENV{LIB_FUZZING_ENGINE})
	set(COMPILATION_FLAGS -g -O0 -Wall -Wextra -fprofile-instr-generate -fcoverage-mapping)
  if (SANITIZER MATCHES "address")
    set(COMPILATION_FLAGS ${COMPILATION_FLAGS} -fsanitize=fuzzer,address,undefined)
  elseif (SANITIZER MATCHES "memory")
    set(COMPILATION_FLAGS ${COMPILATION_FLAGS} -fsanitize=fuzzer,memory,undefined -fsanitize-memory-track-origins -fsanitize=fuzzer-no-link)
  else()
    message(FATAL_ERROR "Unkown sanitizer type. It must be set to `address` or `memory`.")
  endif()
else()
	set(COMPILATION_FLAGS "$ENV{LIB_FUZZING_ENGINE} $ENV{CFLAGS}")
  separate_arguments(COMPILATION_FLAGS)
endif()

# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

set(DEFINES
    gcc
    APPNAME=\"Fuzzing\"
    API_LEVEL=21
    TARGET=\"flex\"
    TARGET_NAME=\"TARGET_FUZZ\"
    APPVERSION=\"1.1.0\"
    SDK_NAME=\"ledger-secure-sdk\"
    SDK_VERSION=\"v21.3.3\"
    SDK_HASH=\"d88d4db3c93665f52b5b1f45099d9d36dfaa06ba\"
    gcc
    __IO=volatile
    NDEBUG
    HAVE_BAGL_FONT_INTER_REGULAR_28PX
    HAVE_BAGL_FONT_INTER_SEMIBOLD_28PX
    HAVE_BAGL_FONT_INTER_MEDIUM_36PX
    HAVE_INAPP_BLE_PAIRING
    HAVE_NBGL
    HAVE_PIEZO_SOUND
    HAVE_SE_TOUCH
    HAVE_SE_EINK_DISPLAY
    NBGL_PAGE
    NBGL_USE_CASE
    SCREEN_SIZE_WALLET
    HAVE_FAST_HOLD_TO_APPROVE
    HAVE_LEDGER_PKI
    HAVE_NES_CRYPT
    HAVE_ST_AES
    NATIVE_LITTLE_ENDIAN
    HAVE_CRC
    HAVE_HASH
    HAVE_RIPEMD160
    HAVE_SHA224
    HAVE_SHA256
    HAVE_SHA3
    HAVE_SHA384
    HAVE_SHA512
    HAVE_SHA512_WITH_BLOCK_ALT_METHOD
    HAVE_SHA512_WITH_BLOCK_ALT_METHOD_M0
    HAVE_BLAKE2
    HAVE_HMAC
    HAVE_PBKDF2
    HAVE_AES
    HAVE_MATH
    HAVE_RNG
    HAVE_RNG_RFC6979
    HAVE_RNG_SP800_90A
    HAVE_ECC
    HAVE_ECC_WEIERSTRASS
    HAVE_ECC_TWISTED_EDWARDS
    HAVE_ECC_MONTGOMERY
    HAVE_SECP256K1_CURVE
    HAVE_SECP256R1_CURVE
    HAVE_SECP384R1_CURVE
    HAVE_SECP521R1_CURVE
    HAVE_FR256V1_CURVE
    HAVE_STARK256_CURVE
    HAVE_BRAINPOOL_P256R1_CURVE
    HAVE_BRAINPOOL_P256T1_CURVE
    HAVE_BRAINPOOL_P320R1_CURVE
    HAVE_BRAINPOOL_P320T1_CURVE
    HAVE_BRAINPOOL_P384R1_CURVE
    HAVE_BRAINPOOL_P384T1_CURVE
    HAVE_BRAINPOOL_P512R1_CURVE
    HAVE_BRAINPOOL_P512T1_CURVE
    HAVE_BLS12_381_G1_CURVE
    HAVE_CV25519_CURVE
    HAVE_CV448_CURVE
    HAVE_ED25519_CURVE
    HAVE_ED448_CURVE
    HAVE_ECDH
    HAVE_ECDSA
    HAVE_EDDSA
    HAVE_ECSCHNORR
    HAVE_X25519
    HAVE_X448
    HAVE_AES_GCM
    HAVE_CMAC
    HAVE_AES_SIV
    COIN_VARIANT=1
    HAVE_BOLOS_APP_STACK_CANARY
    IO_SEPROXYHAL_BUFFER_SIZE_B=300
    HAVE_BLE
    BLE_COMMAND_TIMEOUT_MS=2000
    HAVE_BLE_APDU
    BLE_SEGMENT_SIZE=32
    HAVE_DEBUG_THROWS
    NBGL_QRCODE
    MAJOR_VERSION=1
    MINOR_VERSION=1
    PATCH_VERSION=0
    IO_HID_EP_LENGTH=64
    HAVE_SPRINTF
    HAVE_SNPRINTF_FORMAT_U
    HAVE_IO_USB
    HAVE_L4_USBLIB
    IO_USB_MAX_ENDPOINTS=4
    HAVE_USB_APDU
    USB_SEGMENT_SIZE=64
    HAVE_WEBUSB
    WEBUSB_URL_SIZE_B=0
    WEBUSB_URL=
    OS_IO_SEPROXYHAL
    STANDARD_APP_SYNC_RAPDU
    HAVE_GENERIC_TX_PARSER
    HAVE_TRUSTED_NAME
    HAVE_DYN_MEM_ALLOC
    HAVE_SWAP
    HAVE_ENUM_VALUE
    HAVE_NFT_SUPPORT
)

add_compile_definitions(${DEFINES})

FILE(
  GLOB_RECURSE SDK_STD_SOURCES 
  ${BOLOS_SDK}/lib_standard_app/*.c 
  ${CMAKE_SOURCE_DIR}/../../ethereum-plugin-sdk/src/*.c 
  ./src/mock.c
)
list(
  REMOVE_ITEM SDK_STD_SOURCES 
  ${BOLOS_SDK}/lib_standard_app/io.c 
  ${CMAKE_SOURCE_DIR}/../../ethereum-plugin-sdk/src/main.c 
  ${BOLOS_SDK}/lib_standard_app/main.c 
  ${BOLOS_SDK}/lib_standard_app/crypto_helpers.c
)

include_directories(
  ${CMAKE_SOURCE_DIR}/../../ethereum-plugin-sdk/src/
  ${CMAKE_SOURCE_DIR}/../../src
  ${CMAKE_SOURCE_DIR}/../../src_features/generic_tx_parser/
  ${CMAKE_SOURCE_DIR}/../../src_features/provide_enum_value/
  ${CMAKE_SOURCE_DIR}/../../src_features/provideDynamicNetwork/
  ${CMAKE_SOURCE_DIR}/../../src_features/signTx/
  ${CMAKE_SOURCE_DIR}/../../src_features/provideTrustedName/
  ${CMAKE_SOURCE_DIR}/../../src_features/getChallenge/
  ${CMAKE_SOURCE_DIR}/../../src_features/signMessageEIP712/
  ${BOLOS_SDK}/include
  ${BOLOS_SDK}/target/${TARGET_DEVICE}/include
  ${BOLOS_SDK}/lib_cxng/include
  ${BOLOS_SDK}/lib_cxng/src
  ${BOLOS_SDK}/lib_ux_nbgl
  ${BOLOS_SDK}/lib_nbgl/include
  ${BOLOS_SDK}/lib_standard_app/
  ${CMAKE_SOURCE_DIR}/src/
)

FILE(GLOB_RECURSE SOURCES
  ${CMAKE_SOURCE_DIR}/../../src_features/generic_tx_parser/*.c
  ${CMAKE_SOURCE_DIR}/../../src_features/provideTrustedName/*.c
  ${CMAKE_SOURCE_DIR}/../../src_features/getChallenge/*.c
  ${CMAKE_SOURCE_DIR}/../../src_features/provide_enum_value/*.c
  ${CMAKE_SOURCE_DIR}/../../src_features/provideDynamicNetwork/*.c
  ${CMAKE_SOURCE_DIR}/../../src_features/provideNFTInformation/*.c
  ${CMAKE_SOURCE_DIR}/../../src/mem.c
  ${CMAKE_SOURCE_DIR}/../../src/mem_utils.c
  ${CMAKE_SOURCE_DIR}/../../src/network.c
  ${CMAKE_SOURCE_DIR}/../../src/tlv.c
  ${CMAKE_SOURCE_DIR}/../../src/tlv_apdu.c
  ${CMAKE_SOURCE_DIR}/../../src/uint128.c
  ${CMAKE_SOURCE_DIR}/../../src/uint256.c
  ${CMAKE_SOURCE_DIR}/../../src/time_format.c
  ${CMAKE_SOURCE_DIR}/../../src/uint_common.c
  ${CMAKE_SOURCE_DIR}/../../src/utils.c
  ${CMAKE_SOURCE_DIR}/../../src/manage_asset_info.c
  ${CMAKE_SOURCE_DIR}/../../src/hash_bytes.c
)

add_executable(fuzzer src/fuzzer.c ${SDK_STD_SOURCES} ${SOURCES})
target_compile_options(fuzzer PRIVATE ${COMPILATION_FLAGS})
target_link_options(fuzzer PRIVATE ${COMPILATION_FLAGS})
