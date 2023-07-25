## Introduction
[Uniswap](https://en.wikipedia.org/wiki/Uniswap) is a large decentralized exchange that uses a set of EVM smart contracts to facilitate trading of Tokens on Ethereum and other EVM-compatible systems.

In this document we demonstrate PArSEC's EVM-compatibility and its implementation of the [Ethereum JSON-RPC API](https://ethereum.org/en/developers/docs/apis/json-rpc/) by deploying the [Uniswap V3 protocol](https://blog.uniswap.org/uniswap-v3) on PArSEC, setting up a Uniswap liquidity Pool, and executing an exchange of one test ERC20 token for another.

## Resources
- Uniswap V3
    - [Uniswap V3 SDK Overview](https://docs.uniswap.org/sdk/v3/overview) - The *Guides* section covers some portions of the example here and more 
    - [Uniswap V3 Development Book](https://uniswapv3book.com) - a good intro to general concepts and the necessary math for a developer
    - [Uniswap V3 New Chain Deployments - repo](https://github.com/Uniswap/v3-new-chain-deployments) - goes beyond this demo
    - [Uniswap V3 Deploy - repo](https://github.com/Uniswap/deploy-v3) - tool for deploying Uniswap contracts
- Tools for EVM-compatible Systems
    - [ethers.js (v6)](https://docs.ethers.org/v6/) - a javascript library for interacting with Ethereum and other EVM-compatible systems
    - [hardhat](https://hardhat.org/hardhat-runner/docs/getting-started#overview) - A development environment for Ethereum and other EVM-compatible systems
    - [hardhat-ethers](https://hardhat.org/hardhat-runner/plugins/nomicfoundation-hardhat-ethers) - a hardhat plugin for ethers.js which adds useful methods to the *ethers* object
    - NOTE: Refer to the [PArSEC User Guide's](../parsec_user_guide.md) Prerequisites section for recommended tool versions

## System Prerequisites
Install the following for your platform:
- [Node.js (Javascript runtime environment)](https://nodejs.org/en/download)
- [npm (Javascript package manager)](https://www.npmjs.com/package/npm)

## Step 0: Running the PArSEC server
Refer to the [PArSEC User Guide](../parsec_user_guide.md) on how to start the PArSEC server.

## Step 1: Setting up the PArSEC client environment
### Set up a new Node package as a test environment
#### Set up test directory
Run the following in a Linux shell & keep open (from now referred to as the ***client shell***)
(***client shell***)
```shell
mkdir uniswap_test
cd uniswap_test

mkdir contracts
cp <path_to_opencbdc-tx>/docs/uniswap_demo/MyTokens.sol contracts
cp <path_to_opencbdc-tx>/scripts/hardhat.config.js .
```

#### Review the [hardhat.config.js](../../scripts/hardhat.config.js) copied into the test directory
- Confirm that the PArSEC Agent endpoint matches the *url*
- Private Key corresponding to a pre-minted account (native currency) in PArSEC is reflected in the *accounts* list.
This account is used throughout this demo.

#### Install node.js packages
(***client shell***)
```shell
npm -y init # Takes the defaults
npm install @nomicfoundation/hardhat-ethers ethers
npm install hardhat
npm install @openzeppelin/contracts 
npm install @uniswap/v3-sdk @uniswap/sdk-core @uniswap/v3-periphery
npm install bignumber.js
```
    
## Step 2: Deploying ERC20 Contracts
Compile test ERC20 contracts in [MyTokens.sol](MyTokens.sol) (copied in previous step) (***client shell***):
```shell
# Compiles all solidity files in ./contracts
npx hardhat compile
```
Start the Hardhat console and keep it open:
```shell
npx hardhat console
```
Confirm that the major version of ethers.js is V6. This demo is not compatible with ethers.js V5 (***client shell*** - running hardhat console)
```javascript
> ethers.version   // e.g. '6.6.5'
```
    
Deploy test ERC20 tokens
```javascript
signers = await ethers.getSigners()
ownerAddr = signers[0].address   // Take note of this address

// Deploy ERC20 Tokens:
const weth  = await ethers.deployContract('WETHToken', [ethers.parseEther('10000')]);
const wfoo  = await ethers.deployContract('WFOOToken', [ethers.parseEther('10000')]);
const wbar  = await ethers.deployContract('WBARToken', [ethers.parseEther('20000')]);

// Note these addresses for uniswap v3 contract deployments:
const wethAddr = await weth.getAddress();
const wfooAddr = await wfoo.getAddress();
const wbarAddr = await wbar.getAddress();

// Confirm balances:
ethers.formatEther(await weth.balanceOf(ownerAddr));
ethers.formatEther(await wfoo.balanceOf(ownerAddr));
ethers.formatEther(await wbar.balanceOf(ownerAddr));
```

## Step 3: Deploying Uniswap V3 Contracts - (PART I)
In a separate shell and directory, install the Uniswap V3 deployment scripts.
(Alternatively, clone from the [deploy-v3 github repo](https://github.com/Uniswap/deploy-v3/))
```shell
npm -y init   # Takes the defaults
npm install deploy-v3

# Confirm that the script can run:
node node_modules/deploy-v3/dist/index.js --help
```


Run the deploy command:
```Shell
OWNER_ADDR="0x..."   # ownerAddr from above
WETH_ADDR="0x..."    # wethAddr from above
PRIV_KEY="0x..."     # owner private key from hardhat.config.js

rm -f state.json     # In case index.js is run more than once

# NOTE: json-rpc endpoint matches url in hardhat.config.js
node node_modules/deploy-v3/dist/index.js \
 --private-key $PRIV_KEY \
 --json-rpc http://127.0.0.1:8888 \
 --weth9-address $WETH_ADDR \
 --native-currency-label CBDC \
 --owner-address $OWNER_ADDR
```
Note particular contract addresses (needed later):
```
...
Step 1 complete [
  {
    message: 'Contract UniswapV3Factory deployed',
    address: '0x...',   # Take note of this value
    hash: '0x...'
  }
]
...

Step 9 complete [
  {
    message: 'Contract NonfungiblePositionManager deployed',
    address: '0x...',   # Take note of this value
    hash: '0x...'
  }
]
...

Deployment Succeeded
```

## Step 4: Deploying Uniswap V3 Contracts - (PART II)

### Back to the ***client shell*** (still in hardhat console)
```javascript
// Copy contract addresses from the deploy script output:
const NONFUNGIBLE_POSITION_MANAGER_CONTRACT_ADDR = '0x...' // NonfungiblePositionManager contract address
const UNISWAPV3FACTORY_ADDR = '0x...'                      // UniswapV3Factory contract address
```

### Manually deploy the V3 SwapRouter
This step is needed as the uniswap deploy script deploys only the V2 router.
(See [Github issue](https://github.com/Uniswap/deploy-v3/issues/12))
```javascript
const swapRouterArtifact = require("@uniswap/v3-periphery/artifacts/contracts/SwapRouter.sol/SwapRouter.json");
const swapRouterFactory = new ethers.ContractFactory(swapRouterArtifact.abi, swapRouterArtifact.bytecode, signers[0]);
const swapRouter = await swapRouterFactory.deploy(UNISWAPV3FACTORY_ADDR, wethAddr);
const SWAP_ROUTER_V3_ADDR=await swapRouter.getAddress();
```

## Step 5: Creating and Funding a Uniswap V3 Pool
In the ***client shell*** (in Hardhat console)

### Deploy a Uniswap V3 Pool Contract for pair of test ERC20 contracts
```javascript
// Fee is in 100th of a basis point (i.e. 0.01% = 100, 0.3% = 3000)
// NOTE that the swap factory createPool() method accepts only a set of possible values
const POOL_FEE = 3000; //0xBB8

uniswapv3factory_artifact = require("@uniswap/v3-core/artifacts/contracts/UniswapV3Factory.sol/UniswapV3Factory.json");
const uniswapFactory = new ethers.Contract(
  UNISWAPV3FACTORY_ADDR,
  uniswapv3factory_artifact.abi,
  signers[0]
  );

pooltx = await uniswapFactory.createPool(wfooAddr, wbarAddr, POOL_FEE);
poolreceipt = await pooltx.wait();
poolAddr = await uniswapFactory.getPool(wfooAddr, wbarAddr, POOL_FEE);
```

#### Initialize the Pool contract with a starting Price (ratio of two assets):
```javascript
uniswapv3pool_artifact = require("@uniswap/v3-core/artifacts/contracts/UniswapV3Pool.sol/UniswapV3Pool.json");
const poolContract = new ethers.Contract(
  poolAddr,
  uniswapv3pool_artifact.abi,
  signers[0]
  );

// Calculate square root of price in Q64.96:
const BN = require('bignumber.js');
wfooAmt = 1.0;
wbarAmt = 1.0;
sqrt_wfoopx = Math.sqrt(wbarAmt/wfooAmt);
sqrtPriceX96 = BigInt( BN(2).pow(96).multipliedBy(sqrt_wfoopx).toString(10) )
await poolContract.initialize(sqrtPriceX96);
```

#### Examine the deployed Pool contract:
```javascript
await poolContract.factory()
await poolContract.token0()
await poolContract.token1()
await poolContract.fee()
await poolContract.tickSpacing() //60
await poolContract.liquidity() // BigNumber { value: "0" }
await poolContract.slot0();
```

### Allow Uniswap (the NonFungiblePositionManager Contract) to transfer our tokens
```javascript
await wfoo.approve(NONFUNGIBLE_POSITION_MANAGER_CONTRACT_ADDR, ethers.parseEther('15'));
await wbar.approve(NONFUNGIBLE_POSITION_MANAGER_CONTRACT_ADDR, ethers.parseEther('15'));

// Check the allowance:
await wfoo.allowance(ownerAddr, NONFUNGIBLE_POSITION_MANAGER_CONTRACT_ADDR)
await wbar.allowance(ownerAddr, NONFUNGIBLE_POSITION_MANAGER_CONTRACT_ADDR)
```

### Funding the Pool ("Minting" a position)
#### Instantiate a [Uniswap v3-sdk Pool](https://docs.uniswap.org/sdk/v3/reference/classes/Pool) object:
```javascript
// Make Uniswap core-sdk Token instances:
thisNetwork = await ethers.provider.getNetwork(); // OR // {chainId} = await ethers.provider.getNetwork()
thisChainId = Number(thisNetwork.chainId)
wfoo_decimals = Number(await wfoo.decimals());
wfoo_symbol = await wfoo.symbol();
wfoo_name = await wfoo.name();
wbar_decimals = Number(await wbar.decimals());
wbar_symbol = await wbar.symbol();
wbar_name = await wbar.name();

SDKCORE=require('@uniswap/sdk-core'); // OR // const { Token } = require('@uniswap/sdk-core');
const wfooToken = new SDKCORE.Token(thisChainId, wfooAddr, wfoo_decimals, wfoo_symbol, wfoo_name);
const wbarToken = new SDKCORE.Token(thisChainId, wbarAddr, wbar_decimals, wbar_symbol, wbar_name);

V3SDK = require('@uniswap/v3-sdk'); // OR // const { Pool, Position, nearestUsableTick } = require('@uniswap/v3-sdk');
poolFee = Number(await poolContract.fee());
poolSlot0 = await poolContract.slot0();
poolSqrtPriceX96 = poolSlot0[0];
poolTick = Number(poolSlot0[1]);
poolLiquidity = Number(await poolContract.liquidity());
const configuredPool = new V3SDK.Pool(
  wfooToken,
  wbarToken,
  poolFee,
  poolSqrtPriceX96.toString(),
  poolLiquidity.toString(),
  poolTick
);
```

#### Instantiate a [Uniswap v3-sdk Position](https://docs.uniswap.org/sdk/v3/reference/classes/Position) object:
See the [Uniswap V3 Development book](https://uniswapv3book.com) for an overview of what how a finite price range and liquidity level are represented and calculated in Uniswap V3.
Users can vary the liquidity and the tick range input parameters for the Position object and observe the changes in the calculated mint amounts (i.e. vary liquidity while holding tick range constant, and vice versa).
```javascript
poolTickSpacing = Number(await poolContract.tickSpacing());
positionToMint = new V3SDK.Position({
    pool: configuredPool,
    liquidity: ethers.parseEther('5').toString(),
    tickLower: V3SDK.nearestUsableTick(poolTick, poolTickSpacing) - poolTickSpacing * 2,
    tickUpper: V3SDK.nearestUsableTick(poolTick, poolTickSpacing) + poolTickSpacing * 2,
  });

// Examine Position Object:
positionToMint.mintAmounts
const wfooMintAmount = BigInt(String(positionToMint.mintAmounts.amount0));
const wbarMintAmount = BigInt(String(positionToMint.mintAmounts.amount1));

// These token amounts will be sent to Uniswap:
ethers.formatEther(wfooMintAmount)
ethers.formatEther(wbarMintAmount)
```

#### Create and send a transaction to commit liquidity to the pool:
```javascript
// Build Transaction object to Add Position 
mintOptions = {
    recipient: ownerAddr,
    deadline: Math.floor(Date.now() / 1000) + 60 * 20,
    slippageTolerance: new SDKCORE.Percent(50, 10_000),
  };

// get calldata for minting a position
callParams = V3SDK.NonfungiblePositionManager.addCallParameters(
    positionToMint,
    mintOptions
);

// build transaction
MAX_FEE_PER_GAS = '100000000000';
MAX_PRIORITY_FEE_PER_GAS = '100000000000';
const addPosTx = {
    data: callParams.calldata,
    to: NONFUNGIBLE_POSITION_MANAGER_CONTRACT_ADDR,
    value: callParams.value,
    from: ownerAddr,
    maxFeePerGas: MAX_FEE_PER_GAS,
    maxPriorityFeePerGas: MAX_PRIORITY_FEE_PER_GAS,
}

// Examine state before adding liquidity:
const initLiquidity = await poolContract.liquidity() // BigNumber { value: "0" }
const initWETHbal = await weth.balanceOf(ownerAddr);
const initWFOObal = await wfoo.balanceOf(ownerAddr);
const initWBARbal = await wbar.balanceOf(ownerAddr);
// Execute Add Position Transaction
addpos_resp = await signers[0].sendTransaction( addPosTx );
addpos_receipt = await addpos_resp.wait();

// Examine state after adding liquidity:
const postLiquidity = await poolContract.liquidity()
const postWETHbal = await weth.balanceOf(ownerAddr);
const postWFOObal = await wfoo.balanceOf(ownerAddr);
const postWBARbal = await wbar.balanceOf(ownerAddr);

// State before adding liquidity:
ethers.formatEther(initLiquidity);
ethers.formatEther(initWETHbal);
ethers.formatEther(initWFOObal);
ethers.formatEther(initWBARbal);

// State after adding liquidity:
ethers.formatEther(postLiquidity);
ethers.formatEther(postWETHbal);
ethers.formatEther(postWFOObal);
ethers.formatEther(postWBARbal);

// Confirm that the mint amounts calculated by the Position object has been deducted
initWFOObal == postWFOObal + wfooMintAmount; // true
initWBARbal == postWBARbal + wbarMintAmount; // true
```

## Step 6: Executing Swaps:
### Allow the Uniswap SwapRouterV3 contract to transfer owner's funds
```javascript
const swapRouterArtifact = require("@uniswap/v3-periphery/artifacts/contracts/SwapRouter.sol/SwapRouter.json");
const swapRouterContract = new ethers.Contract(
  SWAP_ROUTER_V3_ADDR,
  swapRouterArtifact.abi,
  signers[0]
  );

await weth.approve(SWAP_ROUTER_V3_ADDR, ethers.parseEther('1'));
await wfoo.approve(SWAP_ROUTER_V3_ADDR, ethers.parseEther('1'));
// Check the allowance:
await weth.allowance(ownerAddr, SWAP_ROUTER_V3_ADDR);
await wfoo.allowance(ownerAddr, SWAP_ROUTER_V3_ADDR);
```
### Swap a small amount of WFOO for WBAR
```javascript
wfooAmountIn = ethers.parseEther('0.00001')
const execSwapParams = {
    tokenIn: wfooAddr,
    tokenOut: wbarAddr,
    fee: POOL_FEE,
    recipient: ownerAddr,
    deadline: Math.floor(Date.now() / 1000) + (60 * 10),
    amountIn: wfooAmountIn,
    amountOutMinimum: 0,
    sqrtPriceLimitX96: 0,
  };

// Check Before Balances:
liquidity0 = await poolContract.liquidity()
wethbal0 = await weth.balanceOf(ownerAddr)
wfoobal0 = await wfoo.balanceOf(ownerAddr)
wbarbal0 = await wbar.balanceOf(ownerAddr)

// SWAP
swap_resp0 = await swapRouterContract.exactInputSingle(
    execSwapParams, {gasLimit: ethers.toBeHex(1000000)} );
swap_receipt0 = await swap_resp0.wait();

// Check state After:
liquidity1 = await poolContract.liquidity()
wethbal1 = await weth.balanceOf(ownerAddr)
wfoobal1 = await wfoo.balanceOf(ownerAddr)
wbarbal1 = await wbar.balanceOf(ownerAddr)

wfoodelta1 = wfoobal0 - wfoobal1;
wbardelta1 = wbarbal1 - wbarbal0;

// Swap conversion “Price”: (WBAR received) / (WFOO paid):
// Note that the conversion ratio is close to the initial price (wbar/wfoo = 1.0)
// and, less than 1.0 as the liquidity demand for WBAR has lowered the value of
// WFOO in relation to WBAR
swapPrice0 = Number((wbardelta1 * 10000n) / wfoodelta1) / 10000
```

### Swap a larger amount of WFOO for WBAR
```javascript
wfooAmountIn = ethers.parseEther('0.02')
const execBigSwapParams = {
    tokenIn: wfooAddr,
    tokenOut: wbarAddr,
    fee: POOL_FEE,
    recipient: ownerAddr,
    deadline: Math.floor(Date.now() / 1000) + (60 * 10),
    amountIn: wfooAmountIn,
    amountOutMinimum: 0,
    sqrtPriceLimitX96: 0,
  };
swap_resp1 = await swapRouterContract.exactInputSingle(
    execBigSwapParams, {gasLimit: ethers.toBeHex(1000000)} );
swap_receipt1 = await swap_resp1.wait();

// Check state After:
const liquidity2 = await poolContract.liquidity()
const wethbal2 = await weth.balanceOf(ownerAddr)
const wfoobal2 = await wfoo.balanceOf(ownerAddr)
const wbarbal2 = await wbar.balanceOf(ownerAddr)

wfoodelta2 = wfoobal1 - wfoobal2;
wbardelta2 = wbarbal2 - wbarbal1;

// Swap conversion “Price”: (WBAR received) / (WFOO paid):
// Less of WBAR is received compared to the previous smaller swap
swapPrice1 = Number((wbardelta2 * 10000n) / wfoodelta2) / 10000
```
