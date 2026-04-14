# Also allows ETC to access the ETH derivation path to recover forked assets
COIN_TYPE = 61
PATH_APP_LOAD_PARAMS += "44'/$(COIN_TYPE)'" "44'/60'"
TICKER = "ETC"
CHAIN_ID = $(COIN_TYPE)
APPNAME = "Ethereum Classic"
