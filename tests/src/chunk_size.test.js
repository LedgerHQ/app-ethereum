import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import { byContractAddress } from "@ledgerhq/hw-app-eth/erc20";
import Zemu from "@zondax/zemu";
import { TransportStatusError } from "@ledgerhq/errors";

const {NANOS_PATH, sim_options_nanos} = require("generic.js");

test("Check chunk-size edge case", async () => {
  jest.setTimeout(100000);
  const sim = new Zemu(NANOS_PATH);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();
    const eth = new Eth(transport);

    // Send transaction
    // Couldn't find the transaction, but this is a special raw tx that checks that what was fixed by https://github.com/LedgerHQ/ledgerjs/pull/606 still works.
    let tx = eth.signTransaction(
        "44'/60'/0'/0/0",
        "f8c282abcd8609184e72a00082271094aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa820300b858bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb25a0eb7170e671a8dd4607248d4850ac3daec22f6acea72cd0e4fe26adb0e8e08095a02f36ae65347b134cdfc0809292a34fa59c3e3e2eda0283b65d190fe8823a35d3"
      );

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    // Review tx
    await sim.clickRight();
    // Data Present
    await sim.clickRight();
    // Amount
    await sim.clickRight();
    // Address 1/3
    await sim.clickRight();
    // Address 2/3
    await sim.clickRight();
    // Address 3/3
    await sim.clickRight();
    // Chain ID
    await sim.clickRight();
    // Max Fees
    await sim.clickRight();
    // Accept
    await sim.clickBoth();

    await expect(tx).resolves.toEqual({
      r: "610f4e866a960467e139387d21a922e836c6ae09af548940aa8ceaa04f720ab4",
      s: "1e9f5e10bf4667c3a8b094856af1d1a98faa544331f654158255fe87cbb55737",
      v: "6d",
    });

  } finally {
    await sim.close();
  }
});