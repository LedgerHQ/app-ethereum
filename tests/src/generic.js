const sim_options_nanos = {
  model: "nanos",
  logging: true,
  start_delay: 2000,
  X11: true,
  custom: "",
};

const sim_options_nanox = {
  model: "nanox",
  logging: true,
  start_delay: 2000,
  X11: true,
  custom: "",
};

const Resolve = require("path").resolve;
const NANOS_ELF_PATH = Resolve("elfs/ethereum_nanos.elf");
const NANOX_ELF_PATH = Resolve("elfs/ethereum_nanox.elf");

const NANOS_ETH_LIB = { "Ethereum": NANOS_ELF_PATH };
const NANOX_ETH_LIB = { "Ethereum": NANOX_ELF_PATH };

const NANOS_CLONE_ELF_PATH = Resolve("elfs/ethereum_classic_nanos.elf");
const NANOX_CLONE_ELF_PATH = Resolve("elfs/ethereum_classic_nanox.elf");

const TIMEOUT = 1000000;

module.exports = {
    NANOS_ELF_PATH,
    NANOX_ELF_PATH,
    NANOS_ETH_LIB,
    NANOX_ETH_LIB,
    NANOS_CLONE_ELF_PATH,
    NANOX_CLONE_ELF_PATH,
    sim_options_nanos,
    sim_options_nanox,
    TIMEOUT,
}