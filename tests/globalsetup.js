import Zemu from "@zondax/zemu";
import fsExtra from "fs-extra";

const catchExit = async () => {
  process.on("SIGINT", () => {
    Zemu.stopAllEmuContainers(function () {
      process.exit();
    });
  });
};

module.exports = async () => {
  await catchExit();
  await Zemu.checkAndPullImage();
  await Zemu.stopAllEmuContainers();
  fsExtra.emptyDirSync("snapshots/tmp")
};