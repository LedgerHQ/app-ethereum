import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu, txFromEtherscan } from './test.fixture';

test('[Nano S] Transfer erc721', zemu("nanos", async (sim, eth) => {

  // https://etherscan.io/tx/0x73cec4fc07de3a24ba42e8756e13b7ddfa9bd449126c37640881195e8ea9e679
  // Modified to put a bigger token id
  const rawTx = "0x02f8d101058459682f0085233da9943e8301865b94bd3531da5cf5857e7cfaa92426877b022e612cf880b86423b872dd0000000000000000000000004cc568b73c0dcf8e90db26d7fd3a6cfadca108a3000000000000000000000000d4c9b20950c3eca38fc1f33f54bdf9694e488799ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffc080a094c8632fe7277aa8c54cea9d81a15911cfa4970a2bf7356d14d04cc5afbcdab7a013a77b8c79e5d9b2b35edb3c44db3bb41b92f5c463ff126bf19d213b2b9ba8b5"
  const serializedTx = txFromEtherscan(rawTx);

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    serializedTx,
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_erc721_transfer', [12, 0]);

  await expect(tx).resolves.toEqual({
    "r": "59f6a9769cff66eed8be8716c44d39808d1e43f3aa0bb97538e124dba4bc4565",
    "s": "662990a841c663a165ba9a83e5cc95c03a999b851e0bd6d296aa70a0f7c96c1a",
    "v": "01",
  });
}));

test('[Nano S] Transfer erc721 with attached ETH', zemu("nanos", async (sim, eth) => {

  const rawTx = "0x02f8d601058459682f0085233da9943e8301865b94bd3531da5cf5857e7cfaa92426877b022e612cf8854242424242b86423b872dd0000000000000000000000004cc568b73c0dcf8e90db26d7fd3a6cfadca108a3000000000000000000000000d4c9b20950c3eca38fc1f33f54bdf9694e4887990000000000000000000000000000000000000000000000000000000000000f21c080a094c8632fe7277aa8c54cea9d81a15911cfa4970a2bf7356d14d04cc5afbcdab7a013a77b8c79e5d9b2b35edb3c44db3bb41b92f5c463ff126bf19d213b2b9ba8b5"
  const serializedTx = txFromEtherscan(rawTx);


  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    serializedTx,
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_erc721_transfer_with_eth', [13, 0]);

  await expect(tx).resolves.toEqual({
    "r": "9c42e10b49f3ee315ab2d5f7ad96f1068c75578734b66504716cc279ead27d47",
    "s": "45dde78470ad75ffdb27a799b87e4934e2e10e98dbc6f88bc4a9bc19c4de86bc",
    "v": "00",
  });
}));

test('[Nano S] set approval for all erc721', zemu("nanos", async (sim, eth) => {

  // https://etherscan.io/tx/0x86b936db53c19fddf26b8d145f165e1c7fdff3c0f8b14b7758a38f0400cfd93f
  const rawTx = "0x02f8b0010c8459682f00852cfbb00ee682b54294d4e4078ca3495de5b1d4db434bebc5a98619778280b844a22cb4650000000000000000000000002efcb1e8d4472d35356b9747bea8a051eac2e3f50000000000000000000000000000000000000000000000000000000000000001c001a0c5b8c024c15ca1452ce8a13eacfcdc25f1c6f581bb3ce570e82f08f1b792b3aca03be4dba0302ae190618a72eb1202ce3af3e17afd7d8a94345a48cae5cad15541";
  const serializedTx = txFromEtherscan(rawTx);


  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    serializedTx,
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_erc721_approval_for_all', [12, 0]);

  await expect(tx).resolves.toEqual({
    "r": "8b6a70a1fe76d8e9b1250531a17eb1e367936732d4dfb9befc81a5031b271dc8",
    "s": "7658d7151bba0d8504cea2013bead64cb8407dc6be1fca829bb9594b56f679af",
    "v": "00",
  });
}));

// NOT DONE
test.skip('[Nano S] approval erc721', zemu("nanos", async (sim, eth) => {

  // INCORRECT, need to find / create an approval tx
  const rawTx = "";
  const serializedTx = txFromEtherscan(rawTx);


  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    serializedTx,
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_erc721_approval', [12, 0]);

  await expect(tx).resolves.toEqual({
  });
}));

test('[Nano S] safe transfer erc721', zemu("nanos", async (sim, eth) => {

  // https://etherscan.io/tx/0x1ee6ce9be1c9fe6f030ff124ba8c88a410223c022816547e4b3fedd3a4d2dc1e
  const rawTx = "0xf8cc82028585077359400083061a8094d4e4078ca3495de5b1d4db434bebc5a98619778280b86442842e0e000000000000000000000000c352b534e8b987e036a93539fd6897f53488e56a0000000000000000000000000a9287d9339c175cd3ea0ad4228f734a9f75ee6200000000000000000000000000000000000000000000000000000000000000621ca08250f4b2c8f28c5e4ef621dba4682990d1faf930c8cb6d032c6e7278e8951d92a03c1e1f6d63ed339041f69f24c6c0968ba26f244f779cb4fa7a468f3ba3d3e06e";
  const serializedTx = txFromEtherscan(rawTx);


  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    serializedTx,
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_erc721_safe_transfer', [10, 0]);

  await expect(tx).resolves.toEqual({
    "r": "b936684d5d0e99e09701021fb73ae9403f2ec79414d822d42c5bd1c0a2118f1a",
    "s": "23e517c6cac998f392d179be2fe7c3225f0e0a165b1af85548da5d6acaa73c4f",
    "v": "25",
  });
}));

// NOT DONE
test.skip('[Nano S] safe transfer with data erc721', zemu("nanos", async (sim, eth) => {

  // need to find or create a safe transfer with data on etherscan?
  const rawTx = "";
  const serializedTx = txFromEtherscan(rawTx);


  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    serializedTx,
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_erc721_safe_transfer_with_data', [12, 0]);

  await expect(tx).resolves.toEqual({
  });
}));