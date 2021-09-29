import 'core-js/stable';
import 'regenerator-runtime/runtime';
import { expect } from "../jest";
import { TransportStatusError } from "@ledgerhq/errors";
import { waitForAppScreen, zemu } from './test.fixture';
import Zemu from '@zondax/zemu';

test('[Nano S] Try to blind sign with setting disabled', zemu("nanos", async (sim, eth) => {
  // disable blind signing
  await sim.navigateAndCompareSnapshots('.', 'nanos_disable_blind_signing', [-2, 0, 0, 3, 0]);

  // we can't use eth.signTransaction because it detects that contract data is disabled and fails early
  let transport = await sim.getTransport();
  let buffer = Buffer.from("058000002c8000003c800000010000000000000000f849208506fc23ac008303dc3194f650c3d88d12db855b8bf7d11be6c55a4e07dcc980a4a1712d6800000000000000000000000000000000000000000000000000000000000acbc7018080", "hex");
  let tx = transport.send(0xe0, 0x04, 0x00, 0x00, buffer);

  await expect(tx).rejects.toEqual(new TransportStatusError(0x6a80));

  await Zemu.sleep(1000);
  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_try_to_blind_sign_with_setting_disabled', [1, 0]);
}));

test('[Nano X] Try to blind sign with setting disabled', zemu("nanox", async (sim, eth) => {
  // disable blind signing
  await sim.navigateAndCompareSnapshots('.', 'nanox_disable_blind_signing', [-2, 0, 0, 3, 0]);

  // we can't use eth.signTransaction because it detects that contract data is disabled and fails early
  let transport = await sim.getTransport();
  let buffer = Buffer.from("058000002c8000003c800000010000000000000000f849208506fc23ac008303dc3194f650c3d88d12db855b8bf7d11be6c55a4e07dcc980a4a1712d6800000000000000000000000000000000000000000000000000000000000acbc7018080", "hex");
  let tx = transport.send(0xe0, 0x04, 0x00, 0x00, buffer);

  await expect(tx).rejects.toEqual(new TransportStatusError(0x6a80));

  await Zemu.sleep(1000);
  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanox_try_to_blind_sign_with_setting_disabled', [0]);
}));