require("@nomicfoundation/hardhat-ethers");

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
      // This URL is the PArSEC agent Node endpoint
      // NOTE: "localhost" (instead of 127.0.0.1) may work on some systems
      url: "http://127.0.0.1:8888/",
      // Private key corresponding to a pre-minted PArSEC account
      accounts: ["32a49a8408806e7a2862bca482c7aabd27e846f673edc8fb0000000000000000"]
    }
  }
};

