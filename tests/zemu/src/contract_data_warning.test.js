import 'core-js/stable';
import 'regenerator-runtime/runtime';
import { TransportStatusError } from "@ledgerhq/errors";
import { waitForAppScreen, zemu, nano_models } from './test.fixture';
import Zemu from '@zondax/zemu';

nano_models.forEach(function(model) {
    test('[Nano ' + model.letter + '] Try to blind sign with setting disabled', zemu(model, async (sim, eth) => {
      // we can't use eth.signTransaction because it detects that contract data is disabled and fails early
      let transport = await sim.getTransport();
      let buffer = Buffer.from("058000002c8000003c800000010000000000000000f849208506fc23ac008303dc3194f650c3d88d12db855b8bf7d11be6c55a4e07dcc980a4a1712d6800000000000000000000000000000000000000000000000000000000000acbc7018080", "hex");
      let tx = transport.send(0xe0, 0x04, 0x00, 0x00, buffer);

      await expect(tx).rejects.toEqual(new TransportStatusError(0x6a80));

      await Zemu.sleep(1000);
      await waitForAppScreen(sim);
      let clicks;
      if (model.letter === 'S') clicks = [1, 0];
      else clicks = [0];
      await sim.navigateAndCompareSnapshots('.', model.name + '_try_to_blind_sign_with_setting_disabled', clicks);
    }));
});
