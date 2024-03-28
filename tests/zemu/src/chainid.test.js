import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu, nano_models } from './test.fixture';

nano_models.forEach(function(model) {
    test('[Nano ' + model.letter + '] Transfer on network 112233445566 on Ethereum', zemu(model, async (sim, eth) => {

      const tx = eth.signTransaction(
        "44'/60'/1'/0/0",
        'f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf6181880851a21a278be8080',
      );

      await waitForAppScreen(sim);
      let clicks;
      if (model.letter === 'S') clicks = 13;
      else clicks = 7;
      await sim.navigateAndCompareSnapshots('.', model.name + '_transfer_112233445566_network', [clicks, -1, 0]);

      await expect(tx).resolves.toEqual({
        "r": "509981d8dfb66757e25ff47c009b9b5bc5db0f169473e4735f5212b144f1c069",
        "s": "5db989d81025de3c846e41a9ce01a3f9fd0982e2d827f1b88ffc95d73a48d04c",
        "v": "344344f19f",
      });
    }));
});

nano_models.forEach(function(model) {
    test('[Nano ' + model.letter + '] Transfer on palm network on Ethereum', zemu(model, async (sim, eth) => {

      const tx = eth.signTransaction(
        "44'/60'/1'/0/0",
        'f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf61818808502a15c308d8080',
      );

      await waitForAppScreen(sim);
      let clicks;
      if (model.letter === 'S') clicks = 13;
      else clicks = 7;
      await sim.navigateAndCompareSnapshots('.', model.name + '_transfer_palm_network', [clicks, -1, 0]);

      await expect(tx).resolves.toEqual({
        "r": "946700c4972b3da24ddaa95e590ad25a8f905da62e2bd053285a4cc17f93f490",
        "s": "3698e84564e58477a49f7a9cea572ef5d672a5538db08f3ee42df5eb75a1b907",
        "v": "0542b8613d",
      });
    }));
});
