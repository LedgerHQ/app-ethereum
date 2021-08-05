import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import { byContractAddress } from "@ledgerhq/hw-app-eth/erc20";
import Zemu from "@zondax/zemu";
import { TransportStatusError } from "@ledgerhq/errors";
import { expect } from "../jest";

const {NANOS_ELF_PATH, NANOX_ELF_PATH, sim_options_nanos, sim_options_nanox, TIMEOUT} = require("generic.js");

const ORIGINAL_SNAPSHOT_PATH_PREFIX = "snapshots/eip1559/";
const SNAPSHOT_PATH_PREFIX = "snapshots/eip1559/";

const ORIGINAL_SNAPSHOT_PATH_NANOS = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanos/";
const ORIGINAL_SNAPSHOT_PATH_NANOX = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanox/";

const SNAPSHOT_PATH_NANOS = SNAPSHOT_PATH_PREFIX + "nanos/";
const SNAPSHOT_PATH_NANOX = SNAPSHOT_PATH_PREFIX + "nanox/";

test("Transfer nanos eip1559", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_ELF_PATH);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    // From this test: https://github.com/ethereum/tests/blob/5d534e37b80e9310e8c7751f805ca481a451123e/GeneralStateTests/stEIP1559/outOfFunds.json#L35
    let buffer = Buffer.from("058000002c8000003c80000000000000000000000002f87001018502540be4008502540be40086246139ca800094cccccccccccccccccccccccccccccccccccccccc8000c001a0e07fb8a64ea3786c9a6649e54429e2786af3ea31c6d06165346678cf8ce44f9ba00e4a0526db1e905b7164a858fd5ebd2f1759e22e6955499448bd276a6aa62830", "hex");

    // Send transaction
    let tx = transport.send(0xe0, 0x04, 0x00, 0x00, buffer);
    let filename;

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    // Review tx
    filename = "review.png";
    await sim.snapshot(SNAPSHOT_PATH_NANOS + filename);
    const review = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_review = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(review).toEqual(expected_review);

    // Amount
    filename = "amount.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const amount = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_amount = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount).toEqual(expected_amount);

    // Address 1/3
    filename = "address_1.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const address_1 = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_address_1 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_1).toEqual(expected_address_1);

    // Address 2/3
    filename = "address_2.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const address_2 = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_address_2 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_2).toEqual(expected_address_2);

    // Address 3/3
    filename = "address_3.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const address_3 = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_address_3 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(address_3).toEqual(expected_address_3);

    // Max Fees
    filename = "fees.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const fees = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_fees = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(fees).toEqual(expected_fees);

    // Accept
    filename = "accept.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const accept = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_accept = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(accept).toEqual(expected_accept);

    await sim.clickBoth();

    await expect(tx).resolves.toEqual(
      Buffer.from([1, 61, 109, 250, 188, 108, 82, 55, 75, 250, 52, 203, 44, 67, 56, 86, 160, 188, 217, 72, 72, 112, 221, 27, 80, 36, 159, 113, 100, 165, 252, 224, 82, 5, 72, 167, 116, 221, 11, 99, 147, 13, 131, 203, 46, 26, 131, 111, 227, 239, 36, 68, 78, 139, 117, 139, 0, 88, 93, 154, 7, 108, 14, 152, 168, 144, 0]));
  } finally {
    await sim.close();
  }
});

test("Transfer nanox", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOX_ELF_PATH);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();

    // From this test: https://github.com/ethereum/tests/blob/5d534e37b80e9310e8c7751f805ca481a451123e/GeneralStateTests/stEIP1559/outOfFunds.json#L35
    let buffer = Buffer.from("058000002c8000003c80000000000000000000000002f87001018502540be4008502540be40086246139ca800094cccccccccccccccccccccccccccccccccccccccc8000c001a0e07fb8a64ea3786c9a6649e54429e2786af3ea31c6d06165346678cf8ce44f9ba00e4a0526db1e905b7164a858fd5ebd2f1759e22e6955499448bd276a6aa62830", "hex");

    // Send transaction
    let tx = transport.send(0xe0, 0x04, 0x00, 0x00, buffer);
    let filename;

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    // Review tx
    filename = "review.png";
    await sim.snapshot(SNAPSHOT_PATH_NANOX + filename);
    const review = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_review = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(review).toEqual(expected_review);

    // Amount
    filename = "amount.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOX + filename);
    const amount = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_amount = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(amount).toEqual(expected_amount);

    // Address
    filename = "address.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOX + filename);
    const address = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_address = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(address).toEqual(expected_address);

    // Max Fees
    filename = "fees.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOX + filename);
    const fees = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_fees = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(fees).toEqual(expected_fees);

    // Accept
    filename = "accept.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOX + filename);
    const accept = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_accept = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(accept).toEqual(expected_accept);

    await sim.clickBoth();

    await expect(tx).resolves.toEqual(
      Buffer.from([1, 61, 109, 250, 188, 108, 82, 55, 75, 250, 52, 203, 44, 67, 56, 86, 160, 188, 217, 72, 72, 112, 221, 27, 80, 36, 159, 113, 100, 165, 252, 224, 82, 5, 72, 167, 116, 221, 11, 99, 147, 13, 131, 203, 46, 26, 131, 111, 227, 239, 36, 68, 78, 139, 117, 139, 0, 88, 93, 154, 7, 108, 14, 152, 168, 144, 0]));
  } finally {
    await sim.close();
  }
});