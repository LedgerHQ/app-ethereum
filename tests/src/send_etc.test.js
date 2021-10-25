import "core-js/stable";
import "regenerator-runtime/runtime";
import Eth from "@ledgerhq/hw-app-eth";
import Zemu from "@zondax/zemu";
import { EthAppPleaseEnableContractData } from "@ledgerhq/errors";
import { expect } from "../jest";

import { waitForAppScreen, NANOS_ETH_LIB, NANOX_ETH_LIB, NANOS_CLONE_ELF_PATH, NANOX_CLONE_ELF_PATH, sim_options_nanos, sim_options_nanox, TIMEOUT} from './test.fixture';

test("[Nano S] Transfer on Ethereum clone app", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_CLONE_ELF_PATH, NANOS_ETH_LIB);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    const eth = new Eth(transport);

    const tx = eth.signTransaction(
      "44'/60'/0'/0/0",
      'EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818803D8080',
    );

    await waitForAppScreen(sim);
    await sim.navigateAndCompareSnapshots('.', 'nanos_transfer_ethereum_clone', [8, 0]);

    await expect(tx).resolves.toEqual({
      "r": "60df850d297e355596f87dc313a742032de4b59c5579186b3d59bdf31402fec0",
      "s": "23c3a2beacabc1943d487a2e1d545e4c46c718b1e70e9d1c11a98828c9338927",
      "v": "9e",
    });
  } finally {
    await sim.close();
  }
});

test("[Nano S] Transfer on network 5234 on Ethereum clone", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOS_CLONE_ELF_PATH, NANOS_ETH_LIB);

  try {
    await sim.start(sim_options_nanos);

    let transport = await sim.getTransport();

    const eth = new Eth(transport);

    const tx = eth.signTransaction(
      "44'/60'/0'/0/0",
      'ED44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818808214728080',
    );

    await expect(tx).rejects.toEqual(new EthAppPleaseEnableContractData(
      "Please enable Contract data on the Ethereum app Settings"
    ));
  } finally {
    await sim.close();
  }
});

test("[Nano X] Transfer on Ethereum clone app", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOX_CLONE_ELF_PATH, NANOX_ETH_LIB);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();

    const eth = new Eth(transport);

    const tx = eth.signTransaction(
      "44'/60'/0'/0/0",
      'EB44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818803D8080',
    );

    await waitForAppScreen(sim);
    await sim.navigateAndCompareSnapshots('.', 'nanox_transfer_ethereum_clone', [4, 0]);

    await expect(tx).resolves.toEqual({
      "r": "60df850d297e355596f87dc313a742032de4b59c5579186b3d59bdf31402fec0",
      "s": "23c3a2beacabc1943d487a2e1d545e4c46c718b1e70e9d1c11a98828c9338927",
      "v": "9e",
    });
  } finally {
    await sim.close();
  }
});

test("[Nano X] Transfer on network 5234 on Ethereum clone", async () => {
  jest.setTimeout(TIMEOUT);
  const sim = new Zemu(NANOX_CLONE_ELF_PATH, NANOX_ETH_LIB);

  try {
    await sim.start(sim_options_nanox);

    let transport = await sim.getTransport();

    const eth = new Eth(transport);

    const tx = eth.signTransaction(
      "44'/60'/0'/0/0",
      'ED44850306DC4200825208945A321744667052AFFA8386ED49E00EF223CBFFC3876F9C9E7BF61818808214728080',
    );

    await expect(tx).rejects.toEqual(new EthAppPleaseEnableContractData(
      "Please enable Contract data on the Ethereum app Settings"
    ));
  } finally {
    await sim.close();
  }
});