import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import { byContractAddress } from "@ledgerhq/hw-app-eth/erc20";
import Zemu from "@zondax/zemu";
import { TransportStatusError } from "@ledgerhq/errors";
import { expect } from "../jest";

const {NANOS_ELF_PATH, NANOX_ELF_PATH, sim_options_nanos, sim_options_nanox, TIMEOUT} = require("generic.js");

const ORIGINAL_SNAPSHOT_PATH_PREFIX = "snapshots/send/";
const SNAPSHOT_PATH_PREFIX = "snapshots/tmp/";

const ORIGINAL_SNAPSHOT_PATH_NANOS = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanos/";
const ORIGINAL_SNAPSHOT_PATH_NANOX = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanox/";

const SNAPSHOT_PATH_NANOS = SNAPSHOT_PATH_PREFIX + "nanos/";
const SNAPSHOT_PATH_NANOX = SNAPSHOT_PATH_PREFIX + "nanox/";

test("Transfer Ether on Ethereum app nanos", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_ELF_PATH);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    let buffer = Buffer.from("058000002C8000003C800000010000000000000000EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF6181880018080", "hex");

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
      Buffer.from([ 38, 111, 56, 157, 21, 50, 15, 5, 1, 56, 53, 38, 237, 3, 222, 145, 124, 20, 33, 39, 22, 240, 154, 38, 45, 188, 152, 67, 16, 134, 165, 219, 73, 13, 201, 148, 183, 185, 114, 48, 187, 53, 253, 246, 254, 194, 244, 216, 255, 76, 251, 139, 254, 178, 166, 82, 195, 100, 199, 56, 255, 3, 60, 5, 221, 144, 0])
    );
  } finally {
    await sim.close();
  }
});

test("Transfer on network 5234 on Ethereum nanos", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_ELF_PATH);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    let buffer = Buffer.from("058000002C8000003C800000010000000000000000EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818808214728080", "hex");

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

    // Network
    filename = "network.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const network = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(network).toEqual(expected_network);

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
      Buffer.from("08f3449bbc245669e26dd076986e11aa3117e2405ffe2ddc7a7e220f81326fbd91150515605c78119367be311345e9ff40c4e4ddb9ec0fd81f37035c3828f4c8b29000", 'hex')
    );
  } finally {
    await sim.close();
  }
});

test("Transfer Ether on Ethereum nanox", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOX_ELF_PATH);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();
    let buffer = Buffer.from("058000002C8000003C800000010000000000000000EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF6181880018080", "hex");

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
      Buffer.from([ 38, 111, 56, 157, 21, 50, 15, 5, 1, 56, 53, 38, 237, 3, 222, 145, 124, 20, 33, 39, 22, 240, 154, 38, 45, 188, 152, 67, 16, 134, 165, 219, 73, 13, 201, 148, 183, 185, 114, 48, 187, 53, 253, 246, 254, 194, 244, 216, 255, 76, 251, 139, 254, 178, 166, 82, 195, 100, 199, 56, 255, 3, 60, 5, 221, 144, 0])
    );
  } finally {
    await sim.close();
  }
});


test("Transfer on network 5234 on Ethereum nanox", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOX_ELF_PATH);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();

    let buffer = Buffer.from("058000002C8000003C800000010000000000000000EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818808214728080", "hex");

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

    // Network
    filename = "network.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOX + filename);
    const network = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_network = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(network).toEqual(expected_network);

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
      Buffer.from("08f3449bbc245669e26dd076986e11aa3117e2405ffe2ddc7a7e220f81326fbd91150515605c78119367be311345e9ff40c4e4ddb9ec0fd81f37035c3828f4c8b29000", 'hex')
    );
  } finally {
    await sim.close();
  }
});