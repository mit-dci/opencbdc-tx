# PARSEC/EVM Load generator contracts

This folder contains the contracts used in the PARSEC/EVM Load generator

Run this to get started:

```
npm install -g pnpm
pnpm install
```

## Compile the contracts, generate headers and copy them

This will (re)compile the contracts and generate C++ headers for them to be used
in the load generator

```
pnpm run build
```

The final header will be placed in `tools/bench/parsec/evm/contracts.hpp`
