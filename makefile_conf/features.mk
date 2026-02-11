# Activate requested features
# ---------------------------
# Bypass the signature verification for setExternalPlugin, setPlugin, provideERC20TokenInfo and provideNFTInfo calls
BYPASS_SIGNATURES ?= 0
ifneq ($(BYPASS_SIGNATURES),0)
    DEFINES += HAVE_BYPASS_SIGNATURES
endif

# Bypass the challenge verification
CHALLENGE_NO_CHECK ?= 0
ifneq ($(CHALLENGE_NO_CHECK),0)
    DEFINES += HAVE_CHALLENGE_NO_CHECK
endif

# Transaction Checks
# TODO: remove this check once the Transaction checks are implemented on all targets
ifeq ($(TARGET_NAME),$(filter $(TARGET_NAME),TARGET_STAX TARGET_FLEX TARGET_APEX_M TARGET_APEX_P))
    DEFINES	+= HAVE_TRANSACTION_CHECKS
endif

# Gating signing - TODO: Reactivate this once the feature is fully available E2E
# DEFINES	+= HAVE_GATING_SUPPORT

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
