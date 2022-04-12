# Lock the application on its standard path for 1.5. Please complain if non compliant
APP_LOAD_PARAMS += --path "44'/60'"
DEFINES += CHAINID_UPCASE=\"ETHEREUM\" CHAINID_COINNAME=\"ETH\" CHAIN_KIND=CHAIN_KIND_ETHEREUM CHAIN_ID=1
# Starkware integration
APP_LOAD_PARAMS += --path "2645'/579218131'"
DEFINES += HAVE_STARKWARE
DEFINES += STARK_BIP32_PATH_0=0x80000A55 STARK_BIP32_PATH_1=0xA2862AD3
# Allow to derive ETH 2 public keys
APP_LOAD_PARAMS += --path "12381/3600" --curve bls12381g1
DEFINES += HAVE_ETH2
APPNAME = "Ethereum"
DEFINES_LIB=
APP_LOAD_FLAGS=--appFlags 0xa40