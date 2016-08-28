# blue-app-eth
Ethereum wallet application framework for Ledger Blue and Nano S

This follows the specification available in the doc/ folder

To use the generic wallet (disabling all data transaction) refer to signTx.py (requires at least ethereum==1.1.0) or Ledger Ethereum Wallet Chrome application available on Github at https://github.com/LedgerHQ/ledger-wallet-ethereum-chrome or the Chrome Web store at https://chrome.google.com/webstore/detail/ledger-wallet-ethereum/hmlhkialjkaldndjnlcdfdphcgeadkkm    

An example application reusing the Ethereum parsing framework to implement some advanced use case (transfering The DAO (RIP) tokens) is also provided for historical reasons - not supported on Nano S and probably not compiling on Blue with the new SDK. 

For more information you can refer to https://medium.com/@Ledger/dynamic-secure-applications-with-bolos-and-ledger-blue-a-use-case-with-ethereum-and-the-dao-6be91260e89f#.204qgmogo 

