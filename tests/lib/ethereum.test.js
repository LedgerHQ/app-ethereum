"use strict";

require("core-js/stable");

require("regenerator-runtime/runtime");

var _hwAppEth = _interopRequireDefault(require("@ledgerhq/hw-app-eth"));

var _erc = require("@ledgerhq/hw-app-eth/erc20");

var _zemu = _interopRequireDefault(require("@zondax/zemu"));

var _errors = require("@ledgerhq/errors");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

test("Test USDT Lend on Compound", async () => {
  jest.setTimeout(100000);
  const sim = new _zemu.default(NANOS_PATH);

  try {
    await sim.start(sim_options);
    let transport = await sim.getTransport();
    const eth = new _hwAppEth.default(transport); // Send transaction
    // https://etherscan.io/tx/0xa0f067369c92d240e253ac3bcb7b7aa56952f7e6feb4c2d93239713fb4e2c40e

    let tx = eth.signTransaction("44'/60'/0'/0/0", "F8A95F850684EE180082CA2D944297394C20800E8A38A619A243E9BBE7681FF24E80B844A9059CBB000000000000000000000000D99A9739CF8B0BF66A7C99D9E14510C3146358C60000000000000000000000000000000000000000000002F0DEB79238F100000025A04CABB0BD9DFDC55605915450F9C7E0E3311996071D9F722BFA12E09C9EF8A23FA03DD645E1DA9B19448A327CC5ADCF89FA228C3642528D60258D9AE32A5B07F7A2");
    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    await sim.clickRight();
    await sim.clickRight();
    await sim.clickRight();
    await sim.clickRight();
    await sim.clickBoth();
    await expect(tx).resolves.toEqual({
      r: "e8c2b5b956b34386e68c5cc5bfc76aac158706430fcf1c71c9f6eb7d9dd690c8",
      s: "064005410a778b047fdd4afcb1e712217c297e5b0a2190f88b9e56ac745fae69",
      v: "26"
    });
  } finally {
    await sim.close();
  }
});