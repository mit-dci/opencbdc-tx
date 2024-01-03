# PArSEC User Guide

This guide runs through setting up the PArSEC system with the EVM runner, and then interacting with it using Hardhat and MetaMask.
This assumes that the system has been set up as described in the [README](https://github.com/mit-dci/opencbdc-tx#readme).

## Prerequisites

Make sure you have [NodeJS installed](https://github.com/nodesource/distributions/blob/master/README.md).
We recommend using `v18` or `v20`, as Hardhat [actively supports these versions](https://github.com/nodejs/release#release-schedule) currently.
We also provide a [.nvmrc](../.nvmrc).
## Running the Server
### Run with Docker

Spawns an agent running the EVM runner by default.
Lua runner may be used by modifying the --runner_type flag in the command keyed by agent0, in the file [`docker-compose-parsec.yml`](https://github.com/mit-dci/opencbdc-tx/blob/trunk/docker-compose-parsec.yml).

### Start it up
```console
# docker compose -f docker-compose-parsec.yml up -d
```

The agent is now available on `http://localhost:8080/`.


### Run Without Docker (MacOS / Ubuntu)
If you are using MacOS or an Ubuntu-like distribution, you can run the system without Docker.

Build and run the system:
```console
$ ./scripts/configure.sh  # only necessary on initial setup
$ ./scripts/build.sh
$ ./scripts/parsec-run-local.sh [OPTIONS]  # use --help flag for help
```
The agent is now available on the specified IP address and port (defaults to localhost:8888).

## Using EVM Clients
If you are using the EVM runner, you can interact with the system using Hardhat, MetaMask, and any Ethereum JSON-RPC compatible client!
Follow below for a demo.

### Setup Hardhat environment

Go to a new separate directory and run the following:

```console
$ mkdir parsec-playground && cd parsec-playground
$ npm -y init   # `-y` keeps all defaults
$ npm install @nomicfoundation/hardhat-ethers ethers
$ npm install hardhat
```

Finish with initializing Hardhat using:

```console
$ npx hardhat
```

Select to create a JavaScript project.
All default values are sufficient for instantiating this project.

Note: This will create new subdirectories and files in this directory.
These can all be overwritten for the purpose of this guide.
However, to use the Hardhat compiler, all Solidity files should be stored in the
`contracts` subdirectory.

Copy the example [hardhat.config.js](../scripts/hardhat.config.js) into this directory.

Edit the `url:` value in the `hardhat.config.js` file to correspond with the url of the agent RPC server.
Using Docker this will be `http://localhost:8080/`.
If running outside of Docker, use the IP and port specified when running `./scripts/parsec-run-local.sh` (default is `http://127.0.0.1:8888/`).

Note: the value in the `accounts` array is the private key of one of the hard-coded, pre-minted accounts in PArSEC/EVM.

You should now be able to use the Hardhat console.

```console
$ npx hardhat console
```

For more information about Hardhat and smart contract deployment,
browse the [Hardhat getting started guide](https://hardhat.org/hardhat-runner/docs/getting-started#quick-start).


### Using the MetaMask Web3 Client

Install [MetaMask](https://metamask.io/) as a browser add-on.
After initialization with seed phrases and all, [add a custom network](https://metamask.zendesk.com/hc/en-us/articles/360043227612-How-to-add-a-custom-network-RPC).

| Parameter       | Value                                     |
| --------------- | ------------------------------------------|
| Network name    | `OpenCBDC`                                |
| Chain ID        | `0xcbdc`                                  |
| New RPC URL     | `http://<agent IP address>:<agent port>/` |
| Currency symbol | `CBDC`                                    |

The agent IP address and agent port are the IP address and port specified for the agent when spawning the system.
Using Docker, this defaults to {IP: localhost, port: 8080}
Running outside of Docker, this defaults to {IP: localhost, port:8888}.
### Funding MetaMask

Once you have MetaMask set up, there will be "Account 1" with a trucated address up top, with a button to copy the address.
You can use that to send yourself some coins:

To issue yourself native tokens, execute the following commands:
Note: the `>` symbol is the Hardhat prompt.
```console
$ npx hardhat console

> const signers = await ethers.getSigners()
undefined
> await signers[0].sendTransaction({to:"0x08293b196E8F1c5552e455CFD10B642EC7a809A7", value:ethers.toBeHex(ethers.parseUnits("500.99"))});
{
    ...
}
```

Replace `0x08293b196E8F1c5552e455CFD10B642EC7a809A7` with your MetaMask address.
You should see the balance appear after a few moments.

### Try deploying en ERC20 token!

An example ERC20 token contract is provided at [opencbdc-tx/contracts/MITCoin.sol](../contracts/MITCoin.sol).
Use this file as a basis, or create one [from scratch](https://dev.to/yakult/a-concise-hardhat-tutorial-part-2-writing-erc20-2jpm).
Follow the below directions to deploy your token.

Note: To compile the example contract, `@openzeppelin/contracts` is a required install.
```console
$ npm install @openzeppelin/contracts
```
Create a deploy script for the token. An example is provided in [opencbdc-tx/scripts/deploy.js](../scripts/deploy.js).
As written, this script requires the name of the ERC20 token to be changed to match the name of the token which is to be deployed.

Note: When the Hardhat project was initialized, a `scripts/deploy.js` file was created.
It corresponds to the file `contracts/Lock.sol` that was also created upon initialization.
For the purposes of this guide, that script can be overwritten, and we do not require `contracts/Lock.sol`.

To deploy [opencbdc-tx/contracts/MITCoin.sol](../contracts/MITCoin.sol), spawn the system using above instructions, then run:

```console
$ cp <path_to_local_opencbdc-tx_clone>/contracts/MITCoin.sol contracts
$ npx hardhat compile
$ npx hardhat run <path_to_local_opencbdc-tx_clone>/scripts/deploy.js
Deploying contracts with the account: 0x01A151CC5ED14d110cc0e6b64360913DE9f453F1
Contract Address: 0x610d7e7AF709BA7e235214Bd56af888Cd5FDb477
```

*Take note of the Contract Address.*
Unless manually modified, the token name for the provided example contract is `MITCoin`.

The token is now deployed on the network!
Issue some to your metamask account:

*The below example assumes that the ERC20 has a function `mint(address, amount)`*


```console
$ npx hardhat console
> const yourToken = await (await ethers.getContractFactory("yourTokenName")).attach("yourContractAddress");
undefined
> await yourToken.mint("your metamask address", ethers.parseUnits("5000"))
{
  hash: '0x12056b3043c7201c4475eacfebcb4b7d4a64fba25c3b3e8262dbe5e42d215cd8',
  ...
}
```

In MetaMask, you can now click `Import tokens`, and paste the Contract Address reported above; the other two fields should populate automatically.

This is the end of the guide, but we encourage you to try deploying new and unique contracts to the system!
