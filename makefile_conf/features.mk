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
NFT_TEST_KEY ?= 0
ifneq ($(NFT_TEST_KEY),0)
    DEFINES += HAVE_NFT_TEST_KEY
endif
NFT_STAGING_KEY ?= 0
ifneq ($(NFT_STAGING_KEY),0)
    # Key used by the staging backend
    DEFINES += HAVE_NFT_STAGING_KEY
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
TRUSTED_NAME_TEST_KEY ?= 0
ifneq ($(TRUSTED_NAME_TEST_KEY),0)
    DEFINES += HAVE_TRUSTED_NAME_TEST_KEY
endif

# Transaction Checks
# TODO: remove this check once the Transaction checks are implemented on all targets
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_STAX TARGET_FLEX))
    DEFINES	+= HAVE_TRANSACTION_CHECKS
endif

EIP7702_TEST_WHITELIST ?= 0
ifneq ($(EIP7702_TEST_WHITELIST),0)
    DEFINES += HAVE_EIP7702_WHITELIST_TEST
endif

ENABLE_DYNAMIC_ALLOC = 1
ifneq ($(DEBUG), 0)
    MEMORY_PROFILING ?= 0
    ifneq ($(MEMORY_PROFILING),0)
        DEFINES += HAVE_MEMORY_PROFILING
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
