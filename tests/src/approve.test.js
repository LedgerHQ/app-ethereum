import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import { byContractAddress } from "@ledgerhq/hw-app-eth/erc20";
import Zemu from "@zondax/zemu";
import { TransportStatusError } from "@ledgerhq/errors";
import { expect } from "../jest";

const {NANOS_ELF_PATH, NANOX_ELF_PATH, sim_options_nanos, sim_options_nanox} = require("generic.js");

const ORIGINAL_SNAPSHOT_PATH_PREFIX = "snapshots/approve/";
const SNAPSHOT_PATH_PREFIX = "snapshots/tmp/";

const ORIGINAL_SNAPSHOT_PATH_NANOS = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanos/";
const ORIGINAL_SNAPSHOT_PATH_NANOX = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanox/";

const SNAPSHOT_PATH_NANOS = SNAPSHOT_PATH_PREFIX + "nanos/";
const SNAPSHOT_PATH_NANOX = SNAPSHOT_PATH_PREFIX + "nanox/";


test("Approve shiba tokens nanos", async () => {
  jest.setTimeout(100000);
  const sim = new Zemu(NANOS_ELF_PATH);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    let buffer = Buffer.from("058000002C8000003C800000010000000000000000F869458502540BE40082DA359495AD61B0A150D79219DCF64E1E6CC01F0B64C4CE80B844095EA7B3000000000000000000000000E592427A0AECE92DE3EDEE1F18E0157C05861564FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF018080", "hex");

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

    // Amount 1/3
    filename = "amount_1.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const amount_1 = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_amount_1 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_1).toEqual(expected_amount_1);

    // Amount 2/3
    filename = "amount_2.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const amount_2 = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_amount_2 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_2).toEqual(expected_amount_2);

    // Amount 3/3
    filename = "amount_3.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const amount_3 = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_amount_3 = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(amount_3).toEqual(expected_amount_3);

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
      Buffer.from([])
    );
  } finally {
    await sim.close();
  }
});

test("Approve shiba token nanox", async () => {
  jest.setTimeout(100000);
  const sim = new Zemu(NANOX_ELF_PATH);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();
    let buffer = Buffer.from("058000002C8000003C800000010000000000000000F869458502540BE40082DA359495AD61B0A150D79219DCF64E1E6CC01F0B64C4CE80B844095EA7B3000000000000000000000000E592427A0AECE92DE3EDEE1F18E0157C05861564FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF018080", "hex");

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
      Buffer.from([])
    );
  } finally {
    await sim.close();
  }
});