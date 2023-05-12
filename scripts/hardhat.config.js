require("@nomiclabs/hardhat-waffle");

// You need to export an object to set up your config
// Go to https://hardhat.org/config/ to learn more

/**
 * @type import('hardhat/config').HardhatUserConfig
 */
module.exports = {
  solidity: {
    compilers: [
      {
        version: "0.8.17",
      },
    ],
  },
  defaultNetwork: "opencbdc",
  networks: {
    opencbdc: {
      url: "http://localhost:8888/",
      accounts: ["32a49a8408806e7a2862bca482c7aabd27e846f673edc8fb0000000000000000"]
    }
  }
};

