# Lock the application on its standard path for 1.5. Please complain if non compliant
APP_LOAD_PARAMS += --path "44'/60'"
TICKER = "ETH"
CHAIN_ID = 1
# Allow to derive ETH 2 public keys
APP_LOAD_PARAMS += --path "12381/3600" --curve bls12381g1
DEFINES += HAVE_ETH2
APPNAME = "Ethereum"
DEFINES_LIB=
DEFINES += HAVE_BOLOS_APP_STACK_CANARY
APP_LOAD_FLAGS=--appFlags 0xa40
