"use strict";

require("core-js/stable");

require("regenerator-runtime/runtime");

var _zemu = _interopRequireDefault(require("@zondax/zemu"));

var _errors = require("@ledgerhq/errors");

var _test = require("./test.fixture");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// Only LNX
const model = _test.nano_models[1];
const set_plugin = (0, _test.apdu_as_string)('e01600007301010645524337323160f80121c31a0d46b5279700f9df786054aa5ee542842e0e0000000000000001000147304502202e2282d7d3ea714da283010f517af469e1d59654aaee0fc438f017aa557eaea50221008b369679381065bbe01135723a4f9adb229295017d37c4d30138b90a51cf6ab6');
const provide_nft_info = (0, _test.apdu_as_string)('e01400007001010752617269626c6560f80121c31a0d46b5279700f9df786054aa5ee500000000000000010001473045022025696986ef5f0ee2f72d9c6e41d7e2bf2e4f06373ab26d73ebe326c7fd4c7a6602210084f6b064d8750ae68ed5dd012296f37030390ec06ff534c5da6f0f4a4460af33');
const sign_first = (0, _test.apdu_as_string)('e004000096058000002c8000003c800000000000000000000000f88a0a852c3ce1ec008301f5679460f80121c31a0d46b5279700f9df786054aa5ee580b86442842e0e0000000000000000000000006cbcd73cd8e8a42844662f0a0e76d7f79afd933d000000000000000000000000c2907efcce4011c491bbeda8a0fa63ba7aab596c000000000000000000000000000000000000000000000000');
const sign_more = (0, _test.apdu_as_string)('e00480000b0000000000112999018080');
test('[Nano ' + model.letter + '] Transfer ERC-721', (0, _test.zemu)(model, async (sim, eth) => {
  const current_screen = sim.getMainMenuSnapshot();
  await (0, _test.send_apdu)(eth.transport, set_plugin);
  await (0, _test.send_apdu)(eth.transport, provide_nft_info);
  await (0, _test.send_apdu)(eth.transport, sign_first);
  let sign_promise = (0, _test.send_apdu)(eth.transport, sign_more);
  await (0, _test.waitForAppScreen)(sim, current_screen);
  await sim.navigateAndCompareSnapshots('.', model.name + '_erc721_transfer', [8, -1, 0]);
  await sign_promise;
}));
test('[Nano ' + model.letter + '] Transfer ERC-721 w/o NFT_PROVIDE_INFORMATION', (0, _test.zemu)(model, async (sim, eth) => {
  const current_screen = sim.getMainMenuSnapshot();
  await (0, _test.send_apdu)(eth.transport, set_plugin);
  await (0, _test.send_apdu)(eth.transport, sign_first);
  let sign_promise = (0, _test.send_apdu)(eth.transport, sign_more);
  await (0, _test.waitForAppScreen)(sim, current_screen);
  await sim.navigateAndCompareSnapshots('.', model.name + '_erc721_transfer_wo_info', [8, -1, 0]);
  await sign_promise;
}));
test('[Nano ' + model.letter + '] Transfer ERC-721 w/o SET_PLUGIN', (0, _test.zemu)(model, async (sim, eth) => {
  const current_screen = sim.getMainMenuSnapshot();
  await (0, _test.send_apdu)(eth.transport, provide_nft_info);
  let sign_tx = (0, _test.send_apdu)(eth.transport, sign_first);
  await expect(sign_tx).rejects.toEqual(new _errors.TransportStatusError(0x6a80));
}));