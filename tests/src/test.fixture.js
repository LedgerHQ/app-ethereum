import Zemu from '@zondax/zemu';
import Eth from '@ledgerhq/hw-app-eth';

const transactionUploadDelay = 60000;

export async function waitForAppScreen(sim) {
    await sim.waitUntilScreenIsNot(sim.getMainMenuSnapshot(), transactionUploadDelay);
}

const simOptionsNanoS = {
    model: 'nanos',
    logging: true,
    X11: true,
    startDelay: 5000,
    custom: '',
};

const simOptionsNanoX = {
    model: 'nanox',
    logging: true,
    X11: true,
    startDelay: 5000,
    custom: '',
};

const Resolve = require('path').resolve;

const APP_PATH_NANOS = Resolve('elfs/ethereum_nanos.elf');
const APP_PATH_NANOX = Resolve('elfs/ethereum_nanox.elf');

export function zemu(device, func) {
    return async () => {
        jest.setTimeout(100000);
        let zemu_args;
        let sim_options;
        if(device === "nanos"){
            zemu_args = [APP_PATH_NANOS];
            sim_options = simOptionsNanoS;
        }
        else{
            zemu_args = [APP_PATH_NANOX];
            sim_options = simOptionsNanoX;
        }
        const sim = new Zemu(...zemu_args);
        try {
            await sim.start(sim_options);
            const transport = await sim.getTransport();
            await func(sim, new Eth(transport));
        } finally {
            await sim.close();
        }
    };
}
