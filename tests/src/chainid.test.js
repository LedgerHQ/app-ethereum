import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu } from './test.fixture';

test('[Nano S] Transfer on network 112233445566 on Ethereum', zemu("nanos", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf6181880851a21a278be8080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_transfer_112233445566_network', [9, 0]);

  await tx;
}));

test('[Nano S] Transfer on palm network on Ethereum', zemu("nanos", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf61818808502a15c308d8080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_transfer_palm_network', [9, 0]);

  await tx;
}));

test('[Nano X] Transfer on network 112233445566 on Ethereum', zemu("nanox", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf6181880851a21a278be8080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanox_transfer_112233445566_network', [5, 0]);

  await tx;
}));

test('[Nano X] Transfer on palm network on Ethereum', zemu("nanox", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'f044850306dc4200825208945a321744667052affa8386ed49e00ef223cbffc3876f9c9e7bf61818808502a15c308d8080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanox_transfer_palm_network', [5, 0]);

  await tx;
}));