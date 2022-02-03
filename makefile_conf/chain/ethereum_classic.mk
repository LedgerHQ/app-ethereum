# Also allows ETC to access the ETH derivation path to recover forked assets
APP_LOAD_PARAMS += --path "44'/61'" --path "44'/60'"
DEFINES += CHAINID_UPCASE=\"ETC\" CHAINID_COINNAME=\"ETC\" CHAIN_KIND=CHAIN_KIND_ETHEREUM_CLASSIC CHAIN_ID=61
APPNAME = "Ethereum Classic"