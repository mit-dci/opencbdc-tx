async function main() {
    const [deployer] = await ethers.getSigners();

    console.log("Deploying contracts with the account:", deployer.address);

    // fill in your token name here
    const token = await ethers.deployContract("MITCoin", []);
    console.log("Contract Address:", await token.getAddress());
  }

  main()
    .then(() => process.exit(0))
    .catch((error) => {
      console.error(error);
      process.exit(1);
  });

