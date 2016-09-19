# blue-app-eth
Ethereum wallet application framework for Ledger Blue and Nano S

This follows the specification available in the doc/ folder

To use the generic wallet (which disables all data transaction) refer to signTx.py (requires rlp package) or Ledger Ethereum Wallet Chrome application available on Github at https://github.com/LedgerHQ/ledger-wallet-ethereum-chrome or the Chrome Web store at https://chrome.google.com/webstore/detail/ledger-wallet-ethereum/hmlhkialjkaldndjnlcdfdphcgeadkkm 

Other examples are provided reusing the Ethereum parsing framwork to implement some advanced use cases : 

  * An ETH/ETC splitting contract - see src_chainsplit, splitEther.py and our Medium post at https://blog.ledger.co/splitting-your-ethers-securely-on-your-nano-s-147f20e9e341
  * An unsupported use case (transferring The DAO (RIP) tokens), using deprecated code - see src_daosend 


For more information about the parsing framework you can refer to https://medium.com/@Ledger/dynamic-secure-applications-with-bolos-and-ledger-blue-a-use-case-with-ethereum-and-the-dao-6be91260e89f#.204qgmogo 

