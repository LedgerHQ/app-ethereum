import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import { byContractAddress } from "@ledgerhq/hw-app-eth/erc20";
import Zemu from "@zondax/zemu";
import { TransportStatusError } from "@ledgerhq/errors";
import { expect } from "../jest";

const {NANOS_ELF_PATH, NANOX_ELF_PATH, sim_options_nanos, sim_options_nanox, TIMEOUT} = require("generic.js");

const ORIGINAL_SNAPSHOT_PATH_PREFIX = "snapshots/approve/";
const SNAPSHOT_PATH_PREFIX = "snapshots/tmp/";

const ORIGINAL_SNAPSHOT_PATH_NANOS = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanos/";
const ORIGINAL_SNAPSHOT_PATH_NANOX = ORIGINAL_SNAPSHOT_PATH_PREFIX + "nanox/";

const SNAPSHOT_PATH_NANOS = SNAPSHOT_PATH_PREFIX + "nanos/";
const SNAPSHOT_PATH_NANOX = SNAPSHOT_PATH_PREFIX + "nanox/";


test("Approve DAI tokens nanos", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_ELF_PATH);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    // Token provisioning
    let buffer = Buffer.from("034441496B175474E89094C44DA98B954EEDEAC495271D0F00000012000000013045022100B3AA979633284EB0F55459099333AB92CF06FDD58DC90E9C070000C8E968864C02207B10EC7D6609F51DDA53D083A6E165A0ABF3A77E13250E6F260772809B49AFF5", "hex");
    let tx = transport.send(0xe0, 0x0a, 0x00, 0x00, buffer);

    // Send transaction
    buffer = Buffer.from("058000002C8000003C800000010000000000000000F869468506A8B15E0082EBEB946B175474E89094C44DA98B954EEDEAC495271D0F80B844095EA7B30000000000000000000000007D2768DE32B0B80B7A3454C06BDAC94A69DDC7A9FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF018080", "hex");
    tx = transport.send(0xe0, 0x04, 0x00, 0x00, buffer);
    let filename;

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());

    // Review tx
    filename = "review.png";
    await sim.snapshot(SNAPSHOT_PATH_NANOS + filename);
    const review = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_review = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(review).toEqual(expected_review);

    // Type
    filename = "type.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOS + filename);
    const type = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOS + filename);
    const expected_type = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOS + filename);
    expect(type).toEqual(expected_type);

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
      Buffer.from([37, 146, 36, 53, 17, 57, 107, 101, 164, 250, 167, 53, 165, 71, 46, 169, 155, 60, 224, 247, 242, 51, 142, 171, 66, 98, 6, 115, 11, 192, 221, 197, 127, 22, 27, 192, 248, 97, 6, 77, 132, 13, 228, 244, 48, 76, 253, 25, 165, 113, 1, 126, 98, 223, 125, 143, 112, 207, 96, 92, 15, 2, 85, 147, 182, 144, 0])
    );
  } finally {
    await sim.close();
  }
});

test("Approve DAI token nanox", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOX_ELF_PATH);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();

    // Token provisioning
    let buffer = Buffer.from("034441496B175474E89094C44DA98B954EEDEAC495271D0F00000012000000013045022100B3AA979633284EB0F55459099333AB92CF06FDD58DC90E9C070000C8E968864C02207B10EC7D6609F51DDA53D083A6E165A0ABF3A77E13250E6F260772809B49AFF5", "hex");
    let tx = transport.send(0xe0, 0x0a, 0x00, 0x00, buffer);

    // Send transaction
    buffer = Buffer.from("058000002C8000003C800000010000000000000000F869468506A8B15E0082EBEB946B175474E89094C44DA98B954EEDEAC495271D0F80B844095EA7B30000000000000000000000007D2768DE32B0B80B7A3454C06BDAC94A69DDC7A9FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF018080", "hex");
    tx = transport.send(0xe0, 0x04, 0x00, 0x00, buffer);
    let filename;

    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot());
    // Review tx
    filename = "review.png";
    await sim.snapshot(SNAPSHOT_PATH_NANOX + filename);
    const review = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_review = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(review).toEqual(expected_review);

    // Type
    filename = "type.png";
    await sim.clickRight(SNAPSHOT_PATH_NANOX + filename);
    const type = Zemu.LoadPng2RGB(SNAPSHOT_PATH_NANOX + filename);
    const expected_type = Zemu.LoadPng2RGB(ORIGINAL_SNAPSHOT_PATH_NANOX + filename);
    expect(type).toEqual(expected_type);

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
      Buffer.from([37, 146, 36, 53, 17, 57, 107, 101, 164, 250, 167, 53, 165, 71, 46, 169, 155, 60, 224, 247, 242, 51, 142, 171, 66, 98, 6, 115, 11, 192, 221, 197, 127, 22, 27, 192, 248, 97, 6, 77, 132, 13, 228, 244, 48, 76, 253, 25, 165, 113, 1, 126, 98, 223, 125, 143, 112, 207, 96, 92, 15, 2, 85, 147, 182, 144, 0])
    );
  } finally {
    await sim.close();
  }
});