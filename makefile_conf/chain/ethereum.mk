# Lock the application on its standard path for 1.5. Please complain if non compliant
PATH_APP_LOAD_PARAMS += "44'/60'"
TICKER = "ETH"
CHAIN_ID = 1
# Allow to derive ETH 2 public keys
PATH_APP_LOAD_PARAMS += "12381/3600"
CURVE_APP_LOAD_PARAMS += bls12381g1
DEFINES += HAVE_ETH2
APPNAME = "Ethereum"
DEFINES += HAVE_BOLOS_APP_STACK_CANARY
