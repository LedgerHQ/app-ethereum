import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu, nano_models } from './test.fixture';

nano_models.forEach(function(model) {
    test('[Nano ' + model.letter + '] Deposit ETH on compound, blind sign', zemu(model, async (sim, eth) => {
      let clicks;
      // LNS does not have EIP712 & ENS settings
      if (model.letter === 'S') clicks = 3;
      else clicks = 5;
        // Enable blind-signing
        await sim.navigateAndCompareSnapshots('.', model.name + '_enable_blind_signing', [-2, 0, 0, clicks, 0]);

      const tx = eth.signTransaction(
        "44'/60'/1'/0/0",
        'f8924685028fa6ae008306599594cc9a0b7c43dc2a5f023bb9b738e45b0ef6b06e0488016345785d8a0000b864474cf53d0000000000000000000000007d2768de32b0b80b7a3454c06bdac94a69ddc7a900000000000000000000000070bc641723fad48be2df6cf63dc6270ee2f897430000000000000000000000000000000000000000000000000000000000000000018080',
      );

      await waitForAppScreen(sim);
      if (model.letter === 'S') clicks = 11;
      else clicks = 9;
      await sim.navigateAndCompareSnapshots('.', model.name + '_deposit_eth_compound_blind', [clicks, -1, 0]);

      await expect(tx).resolves.toEqual({
        "r": "b5ae3a011eb50e7d1fe9f5e6f6d91ca9f4dfca5f73805fc4866d49e72ead2f5c",
        "s": "3c6e55db5925586bb58e434b58b2c04756f662131597f98c1aa2418b16992b81",
        "v": "26",
      });
    }));
});
