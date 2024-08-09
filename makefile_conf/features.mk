# Activate requested features
# ---------------------------
# Bypass the signature verification for setExternalPlugin, setPlugin, provideERC20TokenInfo and provideNFTInfo calls
BYPASS_SIGNATURES ?= 0
ifneq ($(BYPASS_SIGNATURES),0)
    DEFINES += HAVE_BYPASS_SIGNATURES
endif

# Enable the SET_PLUGIN test key
SET_PLUGIN_TEST_KEY ?= 0
ifneq ($(SET_PLUGIN_TEST_KEY),0)
    DEFINES += HAVE_SET_PLUGIN_TEST_KEY
endif

# NFTs
ifneq ($(TARGET_NAME),TARGET_NANOS)
    DEFINES	+= HAVE_NFT_SUPPORT
    NFT_TEST_KEY ?= 0
    ifneq ($(NFT_TEST_KEY),0)
        DEFINES += HAVE_NFT_TEST_KEY
    endif
    NFT_STAGING_KEY ?= 0
    ifneq ($(NFT_STAGING_KEY),0)
        # Key used by the staging backend
        DEFINES += HAVE_NFT_STAGING_KEY
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
CAL_TEST_KEY ?= 0
ifneq ($(CAL_TEST_KEY),0)
    # Key used in our test framework
    DEFINES += HAVE_CAL_TEST_KEY
endif
CAL_STAGING_KEY ?= 0
ifneq ($(CAL_STAGING_KEY),0)
    # Key used by the staging CAL
    DEFINES += HAVE_CAL_STAGING_KEY
endif

# ENS
ifneq ($(TARGET_NAME),TARGET_NANOS)
    DEFINES += HAVE_DOMAIN_NAME
    DOMAIN_NAME_TEST_KEY ?= 0
    ifneq ($(DOMAIN_NAME_TEST_KEY),0)
        DEFINES += HAVE_DOMAIN_NAME_TEST_KEY
    endif
endif

# Check features incompatibilities
# --------------------------------
# NFTs
ifneq (,$(filter $(DEFINES),HAVE_NFT_TEST_KEY))
    ifneq (, $(filter $(DEFINES),HAVE_NFT_STAGING_KEY))
        $(error Multiple alternative NFT keys set at once)
    endif
endif

# CryptoAssetsList key
ifneq (,$(filter $(DEFINES),HAVE_CAL_TEST_KEY))
    ifneq (, $(filter $(DEFINES),HAVE_CAL_STAGING_KEY))
        # Can't use both the staging and testing keys
        $(error Multiple alternative CAL keys set at once)
    endif
endif
