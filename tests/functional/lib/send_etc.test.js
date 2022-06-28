"use strict";

require("core-js/stable");

require("regenerator-runtime/runtime");

var _errors = require("@ledgerhq/errors");

var _test = require("./test.fixture");

_test.nano_models.forEach(function (model) {
  test("[Nano " + model.letter + "] Transfer on Ethereum clone app", (0, _test.zemu)(model, async (sim, eth) => {
    const tx = eth.signTransaction("44'/60'/0'/0/0", 'EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818803D8080');
    await (0, _test.waitForAppScreen)(sim);
    let clicks;
    if (model.letter === 'S') clicks = 9;else clicks = 5;
    await sim.navigateAndCompareSnapshots('.', model.name + '_transfer_ethereum_clone', [clicks, -1, 0]);
    await expect(tx).resolves.toEqual({
      "r": "60df850d297e355596f87dc313a742032de4b59c5579186b3d59bdf31402fec0",
      "s": "23c3a2beacabc1943d487a2e1d545e4c46c718b1e70e9d1c11a98828c9338927",
      "v": "9e"
    });
  }, true));
});

_test.nano_models.forEach(function (model) {
  test("[Nano " + model.letter + "] Transfer on network 5234 on Ethereum clone", (0, _test.zemu)(model, async (sim, eth) => {
    const tx = eth.signTransaction("44'/60'/0'/0/0", 'ED44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818808214728080');
    await expect(tx).rejects.toEqual(new _errors.EthAppPleaseEnableContractData("Please enable Contract data on the Ethereum app Settings"));
  }, true));
});