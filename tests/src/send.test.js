import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu } from './test.fixture';
import { TransportStatusError } from "@ledgerhq/errors";

test('[Nano S] Transfer Ether on Ethereum app', zemu("nanos", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF6181880018080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_transfer_ethereum', [8, 0]);

  await expect(tx).resolves.toEqual({
    "r": "6f389d15320f0501383526ed03de917c14212716f09a262dbc98431086a5db49",
    "s": "0dc994b7b97230bb35fdf6fec2f4d8ff4cfb8bfeb2a652c364c738ff033c05dd",
    "v": "26",
  });
}));

test('[Nano S] Transfer amount >= 2^87 Eth on Ethereum app should fail', zemu("nanos", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'f83f268e02cc9be5c53ea44bd43c289dcddc82520894dac17f958d2ee523a2206206994597c13d831ec7928db8b0861b8f7fe5df83cd553a829878000080018080',
  );

  await expect(tx).rejects.toEqual(new TransportStatusError(0x6807));
}));

test('[Nano S] Transfer Ether on network 5234 on Ethereum app', zemu("nanos", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'ED44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818808214728080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_transfer_ethereum_5234_network', [9, 0]);

  await expect(tx).resolves.toEqual({
    "r": "07a7982dfd16360c96a03467877d0cf9c36f799deff4dace250cdb18e28a3b90",
    "s": "773318a93da2e32c1cf308ddd6add1e8c0d285973e541520a05fb4dc720e4fb1",
    "v": "2908",
  });
}));

test('[Nano X] Transfer Ether on Ethereum app', zemu("nanox", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF6181880018080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanox_transfer_ethereum', [4, 0]);

  await expect(tx).resolves.toEqual({
    "r": "6f389d15320f0501383526ed03de917c14212716f09a262dbc98431086a5db49",
    "s": "0dc994b7b97230bb35fdf6fec2f4d8ff4cfb8bfeb2a652c364c738ff033c05dd",
    "v": "26",
  });
}));

test('[Nano X] Transfer Ether on network 5234 on Ethereum app', zemu("nanox", async (sim, eth) => {

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'ED44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818808214728080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanox_transfer_ethereum_5234_network', [5, 0]);

  await expect(tx).resolves.toEqual({
    "r": "07a7982dfd16360c96a03467877d0cf9c36f799deff4dace250cdb18e28a3b90",
    "s": "773318a93da2e32c1cf308ddd6add1e8c0d285973e541520a05fb4dc720e4fb1",
    "v": "2908",
  });
}));
