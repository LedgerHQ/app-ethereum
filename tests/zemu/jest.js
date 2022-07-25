export default jest;
export const { expect, test } = global;

export const sim_options_s = {
  model: "nanos",
  logging: true,
  start_delay: 2000,
  X11: true,
  custom: "",
};

export const sim_options_x = {
  model: "nanox",
  logging: true,
  start_delay: 2000,
  X11: true,
  custom: "",
};

export const Resolve = require("path").resolve;
export const NANOS_ELF_PATH = Resolve("elfs/ethereum_nanos.elf");
export const NANOX_ELF_PATH = Resolve("elfs/ethereum_nanox.elf");