async function main() {
    const [deployer] = await ethers.getSigners();

    console.log("Deploying contracts with the account:", deployer.address);

    const weiAmount = (await deployer.getBalance()).toString();

    console.log("Account balance:", (await ethers.utils.formatEther(weiAmount)));

    // fill in your token name here
    const Token = await ethers.getContractFactory("MITCoin");
    const token = await Token.deploy();

    const info = await token.deployTransaction.wait();
    console.log("Contract Information:", info);
  }

  main()
    .then(() => process.exit(0))
    .catch((error) => {
      console.error(error);
      process.exit(1);
  });
