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
const NANOS_PATH = Resolve("elfs/ethereum_nanos.elf");
const NANOX_PATH = Resolve("elfs/ethereum_nanox.elf");

module.exports = {
    NANOS_PATH,
    NANOX_PATH,
    sim_options_nanos,
    sim_options_nanox
}