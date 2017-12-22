# To Compile

- Ubuntu 16.04
sudo apt-get install libusb-1.0-0-dev libudev-dev python-dev gcc-multilib g++-multilib clang ntp
sudo apt-get install --reinstall binutils

python --version
Python 2.7.10

pip install django==1.11
pip install image 
pip install ledgerblue
If Ledger PR hasn't merged, please install this version:
https://github.com/LedgerHQ/blue-loader-python/pull/18/files

```bash
make -f Makefile.genericwallet BOLOS_ENV=/home/$USER/ledger_ubuntu/ledger/bolos_env BOLOS_SDK=/home/$USER/ledger_ubuntu/ledger/nanos-secure-sdk CHAIN=poa load
```


# Setup BOLOS_ENV:
https://ledger.readthedocs.io/en/latest/userspace/getting_started.html

 - Choose a directory for the BOLOS environment (I’ll use ~/bolos-devenv/) and link the environment variable BOLOS_ENV to this directory.
 - Download a prebuilt gcc from https://launchpad.net/gcc-arm-embedded/+milestone/5-2016-q1-update and unpack it into ~/bolos-devenv/. Make sure there is a directory named bin directly inside ~/bolos-devenv/gcc-arm-none-eabi-5_3-2016q1/.

 - Download a prebuilt clang from http://releases.llvm.org/download.html#4.0.0 and unpack it into ~/bolos-devenv/. Rename the directory that was inside the archive you downloaded to clang-arm-fropi, or create a link to the directory with that name. Make sure there is a directory named bin directly inside ~/bolos-devenv/clang-arm-fropi/.

# Setup NANOS_SECURE_SDK:
https://github.com/LedgerHQ/nanos-secure-sdk/releases

# To generate icon:
python ./nanos-secure-sdk-nanos-1314/icon.py nanos_app_poa.gif hexbitmaponly

# To Load app onto Ledger
```bash
python -m ledgerblue.loadApp --path "44'/60'" --appFlags 0x40 --path "44'/1'" --curve secp256k1 --targetId 0x31100002 --fileName bin/app.hex --appName POA.NETWORK --apdu --icon 0100000000ffffff00ffffffffffffffffffff1ff80ff087e187e10ff01ff8ffffffffffffffffffff
```

# blue-app-eth
Ethereum wallet application framework for Ledger Blue and Nano S

This follows the specification available in the doc/ folder

To use the generic wallet (which disables all data transaction) refer to signTx.py (requires rlp package) or Ledger Ethereum Wallet Chrome application available on Github at https://github.com/LedgerHQ/ledger-wallet-ethereum-chrome or the Chrome Web store at https://chrome.google.com/webstore/detail/ledger-wallet-ethereum/hmlhkialjkaldndjnlcdfdphcgeadkkm 

Other examples are provided reusing the Ethereum parsing framwork to implement some advanced use cases : 

  * An ETH/ETC splitting contract - see src_chainsplit, splitEther.py and our Medium post at https://blog.ledger.co/splitting-your-ethers-securely-on-your-nano-s-147f20e9e341
  * An unsupported use case (transferring The DAO (RIP) tokens), using deprecated code - see src_daosend 


For more information about the parsing framework you can refer to https://medium.com/@Ledger/dynamic-secure-applications-with-bolos-and-ledger-blue-a-use-case-with-ethereum-and-the-dao-6be91260e89f#.204qgmogo 

