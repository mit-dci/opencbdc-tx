# Programmability User Guide

This README runs you through setting up the 3PC system with the EVM runner, and then interacting with it using Hardhat and MetaMask.

## Prerequisites

Make sure you have [Docker installed](https://docs.docker.com/engine/install/ubuntu/).

Make sure you have [NodeJS v14+ installed](https://github.com/nodesource/distributions/blob/master/README.md).

## Clone the repository

First, clone OpenCBDC locally, and pull the programmability branch:

```
git clone --recurse-submodules https://github.com/mit-dci/opencbdc-tx opencbdc-tx

# This is the working programmability branch.
# This step will be obsolete after this branch is merged with trunk.
git pull origin programmability
```

Next, run the configure script to install the necessary libraries and resources.

```
./scripts/configure.sh
```

## Running the Server
### Run with Docker

Spawns an agent running the EVM runner by default. Lua runner may be used by modifying the --runner_type
flag in the agent0 command argument, in the file `docker-compose-3pc.yml`.

### Usage pre-merge (current state)
The Dockerfile uses a remote image as the base image (`ghcr.io/mit-dci/opencbdc-tx-base:latest`). This will be updated when this branch is merged into trunk.
Until then, we use a workaround which is to rebuild the base image, and
tag it with the name of the remote repository, as the remote base image does
not have 3PC dependencies.

```
docker build --target base . -t ghcr.io/mit-dci/opencbdc-tx-base:latest
```

### Build 3PC containers

```
docker compose -f docker-compose-3pc.yml build
```

### Start it up

```
# will build containers if not already built
docker compose -f docker-compose-3pc.yml up -d
```

The agent is now available on `http://localhost:8080/`


### Run Without Docker (MacOS / Ubuntu)
If you are using MacOS or an Ubuntu-like distribution, you can run the system without Docker.

Build and run the system:
```
./scripts/build.sh
./scripts/3pc-run-local.sh [OPTIONS]  # use --help flag for help
```
The agent is now available on the specified IP address and port (defaults to localhost:8888).

## Using EVM Clients
If you are using the EVM runner, you can interact with the system using Hardhat, MetaMask, and any Ethereum JSON-RPC compatible client! Follow below for a demo.

### Setup hardhat environment

Go to a new separate folder and run `npm init` and `npm install --save-dev hardhat`:

```
mkdir ~/3pc-playground && cd ~/3pc-playground
npm init
npm install --save-dev hardhat @nomiclabs/hardhat-waffle
```

Finish with initializing hardhat using:

```
npx hardhat
```

Select to create a javascript project.

If you are using Docker, edit your hardhat.config.js to look like this:

```
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
      url: `http://localhost:8080`,
      accounts: ["32a49a8408806e7a2862bca482c7aabd27e846f673edc8fb0000000000000000"]
    }
  }
};
```

If you are running without Docker, edit hardhat.config.js to look like this:
```
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
      url: `http://*YOUR SPECIFIED IP*:*YOUR SPECIFIED PORT*`,
      accounts: ["32a49a8408806e7a2862bca482c7aabd27e846f673edc8fb0000000000000000"]
    }
  }
};
```
* Note: the value in the `accounts` array is the private key of one of the hard-coded, pre-minted accounts in 3PC/EVM.

You should now be able to use hardhat through for instance

```
npx hardhat console
```

Look at more fun stuff with hardhat and compiling and deploying contracts [here](https://hardhat.org/hardhat-runner/docs/getting-started#quick-start)


### Using the MetaMask Web3 Client

Install [MetaMask](https://metamask.io/) as a browser add-on. After initialization with seed phrases and all, [add a custom network](https://metamask.zendesk.com/hc/en-us/articles/360043227612-How-to-add-a-custom-network-RPC).

If you are using Docker, use these parameters:

| Parameter       | Value                    |
| --------------- | ------------------------ |
| Network name    | `OpenCBDC`               |
| Chain ID        | `0xcbdc`                 |
| New RPC URL     | `http://localhost:8080/` |
| Currency symbol | `CBDC`                   |

If you are not using Docker, use these parameters:

| Parameter       | Value                                     |
| --------------- | ------------------------------------------|
| Network name    | `OpenCBDC`                                |
| Chain ID        | `0xcbdc`                                  |
| New RPC URL     | `http://<specified IP>:<specified port>/` |
| Currency symbol | `CBDC`                                    |

### Funding MetaMask

Once you have MetaMask set up, there will be "Account 1" with a trucated address up top, with a button to copy the address. You can use that to send yourself some coins:

```
npx hardhat console

> const signers = await ethers.getSigners()
undefined
> await signers[0].sendTransaction({to:"0x08293b196E8F1c5552e455CFD10B642EC7a809A7", value:ethers.utils.parseUnits("500.99").toHexString()});
{
    ...
}
```

Note that the `>` symbol is the hardhat prompt. The two commands to issue are `const signers = await ethers.getSigners()` and then `await signers[0].sendTransaction(...)`

Replace `0x08293b196E8F1c5552e455CFD10B642EC7a809A7` by your MetaMask address.

You should see the balance appear after a few moments.

### Try deploying en ERC20 token!

Modify the provided contracts/MITCoin.sol file to deploy your own ERC20 token using the below directions, or create one [from scratch](https://dev.to/yakult/a-concise-hardhat-tutorial-part-2-writing-erc20-2jpm).

First, update the provided deploy script (`scripts/deploy.js`) to reflect the new ERC20 token.

Spawn the system using above instructions, then run

```
npx hardhat compile
npx hardhat run <deploy-script>.js
```

*Take note of the token address*

The token is now deployed on the network! Issue some to your metamask account:

*The below example assumes that the ERC20 has a function `mint(address, amount)`*

```
npx hardhat console
> const yourToken = await (await ethers.getContractFactory("*yourTokenName*")).attach("*yourTokenAddress noted above*");
undefined
> await cbdcToken.mint("*your metamask address*", ethers.utils.parseUnits("5000"))
{
  hash: '0x12056b3043c7201c4475eacfebcb4b7d4a64fba25c3b3e8262dbe5e42d215cd8',
  ...
}
>
```
