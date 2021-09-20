import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu } from './test.fixture';

test('[Nano S] Approve DAI tokens', zemu("nanos", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'F869468506A8B15E0082EBEB946B175474E89094C44DA98B954EEDEAC495271D0F80B844095EA7B30000000000000000000000007D2768DE32B0B80B7A3454C06BDAC94A69DDC7A9FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF018080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_approve_dai_tokens', [7, 0]);

  await tx;
}));

test('[Nano X] Approve DAI tokens', zemu("nanox", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'F869468506A8B15E0082EBEB946B175474E89094C44DA98B954EEDEAC495271D0F80B844095EA7B30000000000000000000000007D2768DE32B0B80B7A3454C06BDAC94A69DDC7A9FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF018080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanox_approve_dai_tokens', [5, 0]);

  await tx;
}));
