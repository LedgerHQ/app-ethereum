import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu, nano_models } from './test.fixture';

nano_models.forEach(function(model) {
    test('[Nano ' + model.letter + '] Approve DAI tokens', zemu(model, async (sim, eth) => {

      const tx = eth.signTransaction(
        "44'/60'/1'/0/0",
        'F869468506A8B15E0082EBEB946B175474E89094C44DA98B954EEDEAC495271D0F80B844095EA7B30000000000000000000000007D2768DE32B0B80B7A3454C06BDAC94A69DDC7A9FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF018080',
      );

      let clicks;
      if (model.letter === 'S') clicks = 8;
      else clicks = 6;
      await waitForAppScreen(sim);
      await sim.navigateAndCompareSnapshots('.', model.name + '_approve_dai_tokens', [clicks, -1, 0]);

      await expect(tx).resolves.toEqual({
        "r": "92243511396b65a4faa735a5472ea99b3ce0f7f2338eab426206730bc0ddc57f",
        "s": "161bc0f861064d840de4f4304cfd19a571017e62df7d8f70cf605c0f025593b6",
        "v": "25",
      });
    }));
});
