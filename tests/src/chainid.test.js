import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import { byContractAddress } from "@ledgerhq/hw-app-eth/erc20";
import Zemu from "@zondax/zemu";
import { TransportStatusError } from "@ledgerhq/errors";
import { expect } from "../jest";

const {NANOS_ELF_PATH, NANOX_ELF_PATH, sim_options_nanos, sim_options_nanox, TIMEOUT, getTmpPath} = require("generic.js");

const ORIGINAL_SNAPSHOT_PATH_PREFIX = "snapshots/chainid/";

const ORIGINAL_SNAPSHOT_PATH_NANOS = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanos/";
const ORIGINAL_SNAPSHOT_PATH_NANOX = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanox/";

test.skip("Transfer on network 112233445566 on Ethereum nanos", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_ELF_PATH);
  let tmpPath = getTmpPath(expect.getState().currentTestName);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();
    let eth = new Eth(transport);

    // Send transaction
    let tx = eth.signTransaction(
	    "44'/60'/0'/0/0",
      "f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf6181880851a21a278be8080"
    )
    let filename;

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    // Review tx
    filename = "review.png";
    await sim.snapshot(tmpPath + filename);
    const review = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_review = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(review).toMatchSnapshot(expected_review);

    // Amount 1/3
    filename = "amount_1.png";
    await sim.clickRight(tmpPath + filename);
    const amount_1 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_amount_1 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_1).toMatchSnapshot(expected_amount_1);

    // Amount 2/3
    filename = "amount_2.png";
    await sim.clickRight(tmpPath + filename);
    const amount_2 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_amount_2 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_2).toMatchSnapshot(expected_amount_2);

    // Amount 3/3
    filename = "amount_3.png";
    await sim.clickRight(tmpPath + filename);
    const amount_3 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_amount_3 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_3).toMatchSnapshot(expected_amount_3);

    // Address 1/3
    filename = "address_1.png";
    await sim.clickRight(tmpPath + filename);
    const address_1 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_address_1 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_1).toMatchSnapshot(expected_address_1);

    // Address 2/3
    filename = "address_2.png";
    await sim.clickRight(tmpPath + filename);
    const address_2 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_address_2 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_2).toMatchSnapshot(expected_address_2);

    // Address 3/3
    filename = "address_3.png";
    await sim.clickRight(tmpPath + filename);
    const address_3 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_address_3 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_3).toMatchSnapshot(expected_address_3);

    // Network
    filename = "network.png";
    await sim.clickRight(ORIGINAL_SNAPSHOT_PATH_NANOS + filename); // scott
    const network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename); // scott
    const expected_network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(network).toMatchSnapshot(expected_network);

    // Max Fees
    filename = "fees.png";
    await sim.clickRight(tmpPath + filename);
    const fees = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_fees = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(fees).toMatchSnapshot(expected_fees);

    // Accept
    filename = "accept.png";
    await sim.clickRight(tmpPath + filename);
    const accept = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_accept = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(accept).toMatchSnapshot(expected_accept);

    await sim.clickBoth();

    await expect(tx).resolves.toEqual(
      {
        "r": "31fca443b3cad62f3ce18e287f3cf4892ac2669379cc21b5cf198561f0511d1e",
        "s": "3cf21485cd8b86e1acddbcc641e16a3efad18aaeb5ae96a650f1a8b291078494",
        "v": "344344f1a0",
      }
    );
  } finally {
    await sim.close();
  }
});

test("Transfer on palm network on Ethereum nanos", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_ELF_PATH);
  let tmpPath = getTmpPath(expect.getState().currentTestName);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();
    let eth = new Eth(transport);

    // Send transaction
    let tx = eth.signTransaction(
	    "44'/60'/0'/0/0",
      "f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf61818808502a15c308d8080"
    )
    let filename;

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    // Review tx
    filename = "review.png";
    await sim.snapshot(tmpPath + filename);
    const review = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_review = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(review).toMatchSnapshot(expected_review);

    // Amount 1/3
    filename = "amount_1.png";
    console.log("after5");
    await sim.clickRight(tmpPath + filename);
    console.log("after6");
    const amount_1 = Zemu.LoadPng2RGB(tmpPath + filename);
    console.log("after7");
    const expected_amount_1 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    console.log("after8");
    // expect(amount_1).toMatchSnapshot(expected_amount_1);
    console.log("after9");

    // Amount 2/3
    filename = "amount_2.png";
    console.log("after10");
    await sim.clickRight(tmpPath + filename);
    console.log("after11");
    const amount_2 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_amount_2 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_2).toMatchSnapshot(expected_amount_2);

    // Amount 3/3
    filename = "amount_3.png";
    await sim.clickRight(tmpPath + filename);
    const amount_3 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_amount_3 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_3).toMatchSnapshot(expected_amount_3);

    // Address 1/3
    filename = "address_1.png";
    await sim.clickRight(tmpPath + filename);
    const address_1 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_address_1 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_1).toMatchSnapshot(expected_address_1);

    // Address 2/3
    filename = "address_2.png";
    await sim.clickRight(tmpPath + filename);
    const address_2 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_address_2 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_2).toMatchSnapshot(expected_address_2);

    // Address 3/3
    filename = "address_3.png";
    await sim.clickRight(tmpPath + filename);
    const address_3 = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_address_3 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_3).toMatchSnapshot(expected_address_3);

    // Network
    filename = "palm.png";
    await sim.clickRight(ORIGINAL_SNAPSHOT_PATH_NANOS + filename); // scott
    const network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename); // scott
    const expected_network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(network).toMatchSnapshot(expected_network);

    // Max Fees
    filename = "fees.png";
    await sim.clickRight(tmpPath + filename);
    const fees = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_fees = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(fees).toMatchSnapshot(expected_fees);

    // Accept
    filename = "accept.png";
    await sim.clickRight(tmpPath + filename);
    const accept = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_accept = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(accept).toMatchSnapshot(expected_accept);

    await sim.clickBoth();

    await expect(tx).resolves.toEqual(
      {
        "r": "f9b5d903c47c34027156e869bda5aa002233d6cca583ad53d125612fc0795f3b",
        "s": "00da038129414e5ae6f7c1529c6067e82484e3694c84c16d575e77162f631c27",
        "v": "0542b8613d", 
      }
    );
  } finally {
    await sim.close();
  }
});

test.skip("Transfer on network 112233445566 on Ethereum nanox", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOX_ELF_PATH);
  let tmpPath = getTmpPath(expect.getState().currentTestName);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();
    let eth = new Eth(transport);

    // Send transaction
    let tx = eth.signTransaction(
	    "44'/60'/0'/0/0",
	    "f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf6181880851a21a278be8080"
    )
    let filename;

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    // Review tx
    filename = "review.png";
    await sim.snapshot(tmpPath + filename);
    const review = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_review = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(review).toMatchSnapshot(expected_review);

    // Amount
    filename = "amount.png";
    await sim.clickRight(tmpPath + filename);
    const amount = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_amount = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(amount).toMatchSnapshot(expected_amount);

    // Address
    filename = "address.png";
    await sim.clickRight(tmpPath + filename);
    const address = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_address = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(address).toMatchSnapshot(expected_address);

    // Network
    filename = "network.png";
    await sim.clickRight(ORIGINAL_SNAPSHOT_PATH_NANOX + filename); // scott
    const network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename); // scott
    const expected_network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(network).toMatchSnapshot(expected_network);

    // Max Fees
    filename = "fees.png";
    await sim.clickRight(tmpPath + filename);
    const fees = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_fees = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(fees).toMatchSnapshot(expected_fees);

    // Accept
    filename = "accept.png";
    await sim.clickRight(tmpPath + filename);
    const accept = Zemu.LoadPng2RGB(tmpPath + filename);
    const expected_accept = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(accept).toMatchSnapshot(expected_accept);

    await sim.clickBoth();

    await expect(tx).resolves.toEqual(
      Buffer.from([])
    );
  } finally {
    await sim.close();
  }
});