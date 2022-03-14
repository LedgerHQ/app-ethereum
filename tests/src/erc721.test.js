import "core-js/stable";
import "regenerator-runtime/runtime";
import Zemu from '@zondax/zemu';
import { TransportStatusError } from "@ledgerhq/errors";
import { waitForAppScreen, zemu, nano_models, apdu_as_string, send_apdu } from './test.fixture';

// Only LNX
const model = nano_models[1];

const set_plugin = apdu_as_string('e01600007301010645524337323160f80121c31a0d46b5279700f9df786054aa5ee542842e0e0000000000000001000147304502202e2282d7d3ea714da283010f517af469e1d59654aaee0fc438f017aa557eaea50221008b369679381065bbe01135723a4f9adb229295017d37c4d30138b90a51cf6ab6');
const provide_nft_info = apdu_as_string('e01400007001010752617269626c6560f80121c31a0d46b5279700f9df786054aa5ee500000000000000010001473045022025696986ef5f0ee2f72d9c6e41d7e2bf2e4f06373ab26d73ebe326c7fd4c7a6602210084f6b064d8750ae68ed5dd012296f37030390ec06ff534c5da6f0f4a4460af33');
const sign_first = apdu_as_string('e004000096058000002c8000003c800000000000000000000000f88a0a852c3ce1ec008301f5679460f80121c31a0d46b5279700f9df786054aa5ee580b86442842e0e0000000000000000000000006cbcd73cd8e8a42844662f0a0e76d7f79afd933d000000000000000000000000c2907efcce4011c491bbeda8a0fa63ba7aab596c000000000000000000000000000000000000000000000000');
const sign_more = apdu_as_string('e00480000b0000000000112999018080');

test('[Nano ' + model.letter + '] Transfer ERC-721', zemu(model, async (sim, eth) => {
    const current_screen = sim.getMainMenuSnapshot();
    await send_apdu(eth.transport, set_plugin);
    await send_apdu(eth.transport, provide_nft_info);
    await send_apdu(eth.transport, sign_first);
    let sign_promise = send_apdu(eth.transport, sign_more);

    await waitForAppScreen(sim, current_screen);
    await sim.navigateAndCompareSnapshots('.', model.name + '_erc721_transfer', [8, -1, 0]);

    await sign_promise;
}));

test('[Nano ' + model.letter + '] Transfer ERC-721 w/o NFT metadata', zemu(model, async(sim, eth) => {
    const current_screen = sim.getMainMenuSnapshot();
    await send_apdu(eth.transport, set_plugin);
    let sign_tx = send_apdu(eth.transport, sign_first);

    await expect(sign_tx).rejects.toEqual(new TransportStatusError(0x6a80));
}));

test('[Nano ' + model.letter + '] Transfer ERC-721 w/o plugin loaded', zemu(model, async (sim, eth) => {
    const current_screen = sim.getMainMenuSnapshot();
    let nft_info = send_apdu(eth.transport, provide_nft_info);

    await expect(nft_info).rejects.toEqual(new TransportStatusError(0x6985));
}));
