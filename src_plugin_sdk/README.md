# ethereum-plugin-sdk

This repository is meant to be linked as submodule and used in external plugins working with [app-ethereum](https://github.com/LedgerHQ/app-ethereum).
It is composed of a few headers containing definitions about app-ethereum's internal transaction parsing state and some structures to communicate via shared memory.

## Updating this SDK

This SDK is updated at (app-ethereum) build time every time one of app-ethereum internals structures of interest are modified.
If this SDK gets updated, it is possible that all plugins must be recompiled (and eventually updated to work again with the update) with this new SDK.
Be careful, and weight your choices.

## Manual build

If for some reasons you want to rebuild this SDK manually from [app-ethereum](https://github.com/LedgerHQ/app-ethereum) (reminder: it is rebuild automatically when building app-ethereum itself):

```shell
$> ./tools/build_sdk.sh
```
