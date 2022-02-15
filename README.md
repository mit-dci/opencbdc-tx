![CI Status](https://github.com/mit-dci/opencbdc-tx/actions/workflows/ci.yml/badge.svg)
[![Contributor Covenant](https://img.shields.io/badge/Contributor%20Covenant-2.0-4baaaa.svg)](docs/code_of_conduct.md)

# Introduction

OpenCBDC is a technical research project focused on answering open questions surrounding central bank digital currencies (CBDCs).

This repository includes the core transaction processor for a hypothetical, general purpose central bank digital currency (CBDC).
Initially, this work was derived from Project Hamilton (a collaboration between the MIT Digital Currency Initiative (DCI) and the Federal Reserve Bank of Boston (FRBB)).

For higher-level conceptual explanations, as well as findings and conclusions related to this code, see [our research paper](https://dci.mit.edu/opencbdc).

Initially, we focused our work on achieving high transaction throughput, low latency, and resilience against multiple geographical datacenter outages without significant downtime or any data loss.
The design decisions we made to achieve these goals will help inform policy makers around the world about the spectrum of tradeoffs and available options for CBDC design.

# Architecture

We explored two system architectures for transaction settlement, both based on an [unspent transaction output (UTXO)](https://en.wikipedia.org/wiki/Unspent_transaction_output) data model and transaction format.
Both architectures implement the same schema representing an [unspent hash set (UHS)](https://lists.linuxfoundation.org/pipermail/bitcoin-dev/2018-May/015967.html) abstraction.
One architecture provides [linearizabilty](https://en.wikipedia.org/wiki/linearizability) of transactions, whereas the other only provides [serializability](https://en.wikipedia.org/wiki/Serializability).
By relaxing the ordering constraint, the peak transaction throughput supported by the system scales horizontally with the number of nodes, but the transaction history is unavailable making the system harder to audit retroactively.
Both architectures handle multiple geo-distributed datacenter outages with a [recovery time objective (RTO)](https://en.wikipedia.org/wiki/Disaster_recovery#Recovery_Time_Objective) of under ten seconds and a [recovery point objective (RPO)](https://en.wikipedia.org/wiki/Disaster_recovery#Recovery_Point_Objective) of zero.

1. "Atomizer" architecture
    - Materializes a total ordering of all transactions settled by the system in a linear sequence of batches.
    - Requires vertical scaling as peak transaction throughput is limited by the performance of a single system component.
    - Maximum demonstrated throughput ~100K transactions per second.
    - Geo-replicated latency ~3 seconds.
1. "Two-phase commit" architecture
    - Transaction history is not materialized and only a relative ordering is assigned between directly related transactions.
    - Combines [two-phase commit (2PC)](https://en.wikipedia.org/wiki/Two-phase_commit_protocol) and [conservative two-phase locking (C2PL)](https://en.wikipedia.org/wiki/Conservative_two-phase_locking) to create a system without a single bottlenecked component where peak transaction throughput scales horizontally with the number of nodes.
    - Maximum demonstrated throughput ~1.2M transactions per second.
    - Geo-replicated latency <1 second.

Read the [architecture guide](docs/architecture.md) for a detailed description of the system components and implementation of each architecture.

# Contributing

If you are interested in participating in a technical working group, [sign up](https://dci.mit.edu/opencbdc-interest) to receive updates and learn more!

For more information on how to contribute, please see our [Contribution Guide](docs/contributing.md)!

# Get the Code

1. [Install Git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git)
1. Clone the repository (including submodules)
    - `git clone --recurse-submodules https://github.com/mit-dci/opencbdc-tx`

# Run the Code

The easiest way to compile the code and run the system locally is using [Docker](https://www.docker.com).

## Setup Docker

* [Install Docker](https://docs.docker.com/get-docker/)
* [Install docker-compose](https://docs.docker.com/compose/install/)

### Recommended post-install steps
- [Enable the Docker daemon](https://docs.docker.com/engine/install/linux-postinstall/#configure-docker-to-start-on-boot)
  - It is recommended to read the other instructions here to customize other features you may find useful
- [Enable the ability to run docker as a non-root user](https://docs.docker.com/engine/install/linux-postinstall/#manage-docker-as-a-non-root-user)
- Alternatively, advanced users may wish to run Docker in [rootless mode](https://docs.docker.com/engine/security/rootless/)

## Build the container

```terminal
# docker-compose -f docker-compose-2pc.yml build
```

## Launch the System
**Note:** You will need to both run the system and interact with it. To do this, you should run the system in **detached** mode (in the background), and then you can launch a second container in which to run commands. This is detailed below.

Additionally, you can start the atomizer architecture by passing `--file docker-compose-atomizer.yml` instead.

1. Run the System
   ```terminal
   # docker-compose -f docker-compose-2pc.yml up -d
   ```
   **Note** run `docker-compose -f docker-compose-2pc.yml stop` to stop the container (but leave it to view logs, or start it again later). To stop and delete it, run `docker-compose -f docker-compose-2pc.yml down`.
2. Launch a container in which to run wallet commands (use `--network atomizer-network` instead of `--network 2pc-network` if using the atomizer architecture)
   ```terminal
   # docker run --network 2pc-network -ti opencbdc-tx /bin/bash
   ```

## Setup test wallets and test them

The following commands are all performed from within the second container we started in the previous step.
In each of the below commands, you should pass `atomizer-compose.cfg` instead of `2pc-compose.cfg` if you started the atomizer architecture.

* Mint new coins (e.g., 10 new UTXOs each with a value of 5 atomic units of currency)
  ```terminal
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool0.dat wallet0.dat mint 10 5
  [2021-08-17 15:11:57.686] [WARN ] Existing wallet file not found
  [2021-08-17 15:11:57.686] [WARN ] Existing mempool not found
  4bc23da407c3a8110145c5b6c38199c8ec3b0e35ea66bbfd78f0ed65304ce6fa
  ```

  If using the atomizer architecture, you'll need to sync the wallet after:
  ```terminal
  # ./build/src/uhs/client/client-cli atomizer-compose.cfg mempool0.dat wallet0.dat sync
  ```

* Inspect the balance of a wallet
  ```terminal
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool0.dat wallet0.dat info
  Balance: $0.50, UTXOs: 10, pending TXs: 0
  ```

* Make a new wallet
  ```terminal
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool1.dat wallet1.dat newaddress
  [2021-08-17 15:13:16.148] [WARN ] Existing wallet file not found
  [2021-08-17 15:13:16.148] [WARN ] Existing mempool not found
  usd1qrw038lx5n4wxx3yvuwdndpr7gnm347d6pn37uywgudzq90w7fsuk52kd5u
  ```

* Send currency from one wallet to another (e.g., 30 atomic units of currency)
  ```terminal
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool0.dat wallet0.dat send 30 usd1qrw038lx5n4wxx3yvuwdndpr7gnm347d6pn37uywgudzq90w7fsuk52kd5u
  tx_id:
  cc1f7dc708be5b07e23e125cf0674002ff8546a9342928114bc97031d8b96e75
  Data for recipient importinput:
  cc1f7dc708be5b07e23e125cf0674002ff8546a9342928114bc97031d8b96e750000000000000000d0e4f689b550f623e9370edae235de50417860be0f2f8e924eca9f402fcefeaa1e00000000000000
  Sentinel responded: Confirmed
  ```

  If using the atomizer architecture, you'll need to sync the sending wallet after:
  ```terminal
  # ./build/src/uhs/client/client-cli atomizer-compose.cfg mempool0.dat wallet0.dat sync
  ```

* Check that the currency is no longer available in the sending wallet
  ```terminal
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool0.dat wallet0.dat info
  Balance: $0.20, UTXOs: 4, pending TXs: 0
  ```

* Import coins to the receiving wallet
  ```terminal
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool1.dat wallet1.dat importinput cc1f7dc708be5b07e23e125cf0674002ff8546a9342928114bc97031d8b96e750000000000000000d0e4f689b550f623e9370edae235de50417860be0f2f8e924eca9f402fcefeaa1e00000000000000
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool1.dat wallet1.dat sync
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool1.dat wallet1.dat info
  Balance: $0.30, UTXOs: 1, pending TXs: 0
  ```
