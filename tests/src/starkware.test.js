import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu } from './test.fixture';
import { byContractAddressAndChainId } from '@ledgerhq/hw-app-eth/erc20'
import { BigNumber } from "bignumber.js";

test('[Nano S] Transfer Ether on Ethereum app', zemu("nanos", async (sim, eth) => {

  // Provide USDT token info to the app
  const usdt_info = byContractAddressAndChainId("0xdac17f958d2ee523a2206206994597c13d831ec7", 1);
  await eth.provideERC20TokenInformation(usdt_info);

  // Provide Stark quantum
  const quantization = new BigNumber(1);
  await eth.starkProvideQuantum_v2(
    "0xdac17f958d2ee523a2206206994597c13d831ec7",
    "erc20",
    quantization,
    null
    )

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'f8b5018a0472698b413b43200000825208940102030405060708090a0b0c0d0e0f1011121314872bd72a24874000b8842505c3d9010101010101010102020202020202020303030303030303040404040404040402ce625e94458d39dd0bf3b45a843544dd4a14b8169045a3a3d15aa564b936c500000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000030d40808080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanos_starkware_usdt_deposit', [13, 0]);

  await expect(tx).resolves.toEqual({
    "r": "14c368c0d32e399470d6113cf796c5f4cd70300766337d8b0ba71ecad21b3d52",
    "s": "4207c027959e84fc2242a1f4fd955603f137ba28f67268ffc91fef5d65071b0a",
    "v": "1c",
  });
}));

test('[Nano X] Transfer Ether on Ethereum app', zemu("nanox", async (sim, eth) => {

  // Provide USDT token info to the app
  const usdt_info = byContractAddressAndChainId("0xdac17f958d2ee523a2206206994597c13d831ec7", 1);
  await eth.provideERC20TokenInformation(usdt_info);

  // Provide Stark quantum
  const quantization = new BigNumber(1);
  await eth.starkProvideQuantum_v2(
    "0xdac17f958d2ee523a2206206994597c13d831ec7",
    "erc20",
    quantization,
    null
    )

  const tx = eth.signTransaction(
    "44'/60'/1'/0/0",
    'f8b5018a0472698b413b43200000825208940102030405060708090a0b0c0d0e0f1011121314872bd72a24874000b8842505c3d9010101010101010102020202020202020303030303030303040404040404040402ce625e94458d39dd0bf3b45a843544dd4a14b8169045a3a3d15aa564b936c500000000000000000000000000000000000000000000000000000000000000010000000000000000000000000000000000000000000000000000000000030d40808080',
  );

  await waitForAppScreen(sim);
  await sim.navigateAndCompareSnapshots('.', 'nanox_starkware_usdt_deposit', [9, 0]);

  await expect(tx).resolves.toEqual({
    "r": "14c368c0d32e399470d6113cf796c5f4cd70300766337d8b0ba71ecad21b3d52",
    "s": "4207c027959e84fc2242a1f4fd955603f137ba28f67268ffc91fef5d65071b0a",
    "v": "1c",
  });
}));
