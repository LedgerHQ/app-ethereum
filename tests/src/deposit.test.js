import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import { byContractAddress } from "@ledgerhq/hw-app-eth/erc20";
import Zemu from "@zondax/zemu";
import { TransportStatusError } from "@ledgerhq/errors";
import { expect } from "../jest";

const {NANOS_ELF_PATH, NANOX_ELF_PATH, sim_options_nanos, sim_options_nanox} = require("generic.js");

// Adapt this prefix.
const ORIGINAL_SNAPSHOT_PATH_PREFIX = "snapshots/deposit/";
const SNAPSHOT_PATH_PREFIX = "snapshots/tmp/";

const ORIGINAL_SNAPSHOT_PATH_NANOS = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanos/";
const ORIGINAL_SNAPSHOT_PATH_NANOX = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanox/";

const SNAPSHOT_PATH_NANOS = SNAPSHOT_PATH_PREFIX + "nanos/";
const SNAPSHOT_PATH_NANOX = SNAPSHOT_PATH_PREFIX + "nanox/";


test("Deposit ETH nanos", async () => {
  jest.setTimeout(100000);
  const sim = new Zemu(NANOS_ELF_PATH);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    let buffer = Buffer.from("058000002C8000003C800000010000000000000000F8914585055AE826008306599594CC9A0B7C43DC2A5F023BB9B738E45B0EF6B06E0487B1A2BC2EC50000B864474CF53D0000000000000000000000007D2768DE32B0B80B7A3454C06BDAC94A69DDC7A900000000000000000000000070BC641723FAD48BE2DF6CF63DC6270EE2F897430000000000000000000000000000000000", "hex");

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

    // Data present
    filename = "data_present.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const data_present = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_data_present = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(data_present).toEqual(expected_data_present);

    // Amount
    filename = "amount.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const amount_2 = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
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
      Buffer.from([ 38, 111, 56, 157, 21, 50, 15, 5, 1, 56, 53, 38, 237, 3, 222, 145, 124, 20, 33, 39, 22, 240, 154, 38, 45, 188, 152, 67, 16, 134, 165, 219, 73, 13, 201, 148, 183, 185, 114, 48, 187, 53, 253, 246, 254, 194, 244, 216, 255, 76, 251, 139, 254, 178, 166, 82, 195, 100, 199, 56, 255, 3, 60, 5, 221, 144, 0])
    );
  } finally {
    await sim.close();
  }
});

test("Deposit ETH nanox", async () => {
  jest.setTimeout(100000);
  const sim = new Zemu(NANOX_ELF_PATH);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();
    let buffer = Buffer.from("058000002C8000003C800000010000000000000000F8914585055AE826008306599594CC9A0B7C43DC2A5F023BB9B738E45B0EF6B06E0487B1A2BC2EC50000B864474CF53D0000000000000000000000007D2768DE32B0B80B7A3454C06BDAC94A69DDC7A900000000000000000000000070BC641723FAD48BE2DF6CF63DC6270EE2F897430000000000000000000000000000000000", "hex");

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

    // Data present
    filename = "data_present.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const data_present = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_data_present = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(data_present).toEqual(expected_data_present);

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
      Buffer.from([ 38, 111, 56, 157, 21, 50, 15, 5, 1, 56, 53, 38, 237, 3, 222, 145, 124, 20, 33, 39, 22, 240, 154, 38, 45, 188, 152, 67, 16, 134, 165, 219, 73, 13, 201, 148, 183, 185, 114, 48, 187, 53, 253, 246, 254, 194, 244, 216, 255, 76, 251, 139, 254, 178, 166, 82, 195, 100, 199, 56, 255, 3, 60, 5, 221, 144, 0])
    );
  } finally {
    await sim.close();
  }
});