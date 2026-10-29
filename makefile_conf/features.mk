# Activate requested features
# ---------------------------
# Bypass the signature verification for set_external_plugin, set_plugin, provide_erc20_token_information and provide_nft_information calls
BYPASS_SIGNATURES ?= 0
ifneq ($(BYPASS_SIGNATURES),0)
    DEFINES += HAVE_BYPASS_SIGNATURES
endif

# Bypass the challenge verification
CHALLENGE_NO_CHECK ?= 0
ifneq ($(CHALLENGE_NO_CHECK),0)
    DEFINES += HAVE_CHALLENGE_NO_CHECK
endif

# NFTs
NFT_STAGING_KEY ?= 0
ifneq ($(NFT_STAGING_KEY),0)
    # Expect the staging key ids in NFT and SET_PLUGIN descriptors
    DEFINES += HAVE_NFT_STAGING_KEY
endif

# Transaction Checks
# TODO: remove this check once the Transaction checks are implemented on all targets
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_STAX TARGET_FLEX TARGET_APEX_M TARGET_APEX_P))
    DEFINES	+= HAVE_TRANSACTION_CHECKS
endif


# EIP-7702: authorization signing and type 4 transactions
EIP7702_SUPPORT ?= 0
ifneq ($(EIP7702_SUPPORT),0)
    DEFINES += HAVE_7702_SUPPORT
endif

# Whitelist entries used by the test framework (requires EIP7702_SUPPORT)
EIP7702_TEST_WHITELIST ?= 0
ifneq ($(EIP7702_TEST_WHITELIST),0)
    DEFINES += HAVE_EIP7702_WHITELIST_TEST
endif

ifneq ($(DEBUG), 0)
    MEMORY_PROFILING ?= 0
    ifneq ($(MEMORY_PROFILING),0)
        DEFINES += HAVE_MEMORY_PROFILING
    endif
endif

# Check features incompatibilities
# --------------------------------
# EIP-7702
ifneq ($(EIP7702_TEST_WHITELIST),0)
    ifeq ($(EIP7702_SUPPORT),0)
        $(error EIP7702_TEST_WHITELIST requires EIP7702_SUPPORT=1)
    endif
endif
