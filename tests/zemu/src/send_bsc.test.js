import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu, nano_models } from './test.fixture';

nano_models.forEach(function(model) {
    test('[Nano ' + model.letter + '] Transfer bsc', zemu(model, async (sim, eth) => {

      const tx = eth.signTransaction(
        "44'/60'/1'/0/0",
        'EB0185012A05F200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF6181880388080',
      );

      await waitForAppScreen(sim);
      let clicks;
      if (model.letter === 'S') clicks = 13;
      else clicks = 7;
      await sim.navigateAndCompareSnapshots('.', model.name + '_transfer_bsc', [clicks, -1, 0]);

      await expect(tx).resolves.toEqual({
        "r": "f667cc34e9815df4f052fb3463cdbe355fff5c1acf4e919b3539806521a059ad",
        "s": "6b35492b7108d9d9e1cc7aede536ed6b3173197b56dd873cbc3b43e041d6f407",
        "v": "93",
      });
    }));
});
