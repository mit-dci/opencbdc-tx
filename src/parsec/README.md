## Parallel Architecture for Scalably Executing smart Contracts ("PArSEC")

This directory houses the architecture-specific code for a generic virtual machine layer capable of performing parallel executions of smart contracts.

The architecture is composed of two layers:
1. A distributed key-value data store with [ACID](https://en.wikipedia.org/wiki/ACID) database properties
    - This back-end data store is not constrained to any type of data and is agnostic to the execution later.
1. A generic virtual machine layer that executes programs (i.e., smart contracts) and uses the distributed key-value data store to record state
    - This computation layer defines the data models and transaction semantics.
    - We have implemented the Ethereum Virtual Machine EVM and a Lua based virtual machine as two working examples.

- This architecture enables parallel execution of smart contracts which can be scaled horizontally where keys are independent.
- Unmodified smart contracts from the Ethereum ecosystem can be deployed directly onto our EVM implementation.

Read the [PArSEC Architecture Guide](../../docs/parsec_architecture.md) for more details.
