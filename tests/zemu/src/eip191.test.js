import "core-js/stable";
import "regenerator-runtime/runtime";
import { waitForAppScreen, zemu, nano_models, apdu_as_string, send_apdu } from './test.fixture';

const testgroup = "EIP-191";

nano_models.forEach(function(model) {
    test("[Nano " + model.letter + "] "+ testgroup +" Metamask test", zemu(model, async (sim, eth) => {

        const tx = eth.signPersonalMessage(
            "44'/60'/0'/0/0",
            Buffer.from("Example `personal_sign` message").toString("hex")
        );

        await waitForAppScreen(sim);

        const rclicks = (model.letter == 'S') ? 4 : 3;
        await sim.navigateAndCompareSnapshots('.', model.name + '_eip191_metamask', [rclicks, -1, 0]);

        await expect(tx).resolves.toEqual({
            "v": 28,
            "r": "916099cf0d9c21911c85f0770a47a9696a8189e78c259cf099749748c507baae",
            "s": "0d72234bc0ac2e94c5f7a5f4f9cd8610a52be4ea55515a85b9703f1bb158415c"
        });
    }));


    test("[Nano " + model.letter + "] "+ testgroup +" non-ASCII test", zemu(model, async (sim, eth) => {

        const tx = eth.signPersonalMessage(
            "44'/60'/0'/0/0",
            "9c22ff5f21f0b81b113e63f7db6da94fedef11b2119b4088b89664fb9a3cb658"
        );

        await waitForAppScreen(sim);

        const rclicks = (model.letter == 'S') ? 8 : 4;
        await sim.navigateAndCompareSnapshots('.', model.name + '_eip191_nonascii', [rclicks, -1, 0]);

        await expect(tx).resolves.toEqual({
            "v": 28,
            "r": "64bdbdb6959425445d00ff2536a7018d2dce904e1f7475938fe4221c3c72500c",
            "s": "7c9208e99b6b9266a73aae17b73472d06499746edec34fd47a9dab42f06f2e42"
        });
    }));


    test("[Nano " + model.letter + "] "+ testgroup +" OpenSea test", zemu(model, async (sim, eth) => {

        const tx = eth.signPersonalMessage(
            "44'/60'/0'/0/0",
            Buffer.from("Welcome to OpenSea!\n\nClick to sign in and accept the OpenSea Terms of Service: https://opensea.io/tos\n\nThis request will not trigger a blockchain transaction or cost any gas fees.\n\nYour authentication status will reset after 24 hours.\n\nWallet address:\n0x9858effd232b4033e47d90003d41ec34ecaeda94\n\nNonce:\n2b02c8a0-f74f-4554-9821-a28054dc9121").toString("hex")
        );

        await waitForAppScreen(sim);

        if (model.letter == 'S')
        {
            await sim.navigateAndCompareSnapshots('.', model.name + '_eip191_opensea', [1, 5, 1, 6, 0, 1, -1, 0]);
        }
        else
        {
            await sim.navigateAndCompareSnapshots('.', model.name + '_eip191_opensea', [1, 5, 1, 2, 1, -1, 0]);
        }

        await expect(tx).resolves.toEqual({
            "v": 28,
            "r": "61a68c986f087730d2f6ecf89d6d1e48ab963ac461102bb02664bc05c3db75bb",
            "s": "5714729ef441e097673a7b29a681e97f6963d875eeed2081f26b0b6686cd2bd2"
        });
    }));
});
