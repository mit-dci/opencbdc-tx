# 2PC & Atomizer User Guide
NOTE: The easiest way to compile the code and run the system locally is using [Docker](https://www.docker.com).

## Setup Docker

* [Install Docker](https://docs.docker.com/get-docker/)
* [Install docker-compose](https://docs.docker.com/compose/install/)

Don't forget to run the docker daemon!

## Build the containers

Building with Docker utilizes multi-stage builds. In order to run an architecture you will need to build each architecture independently if building locally.

**Note:** We have pre-built images available [here](#launch-the-system-with-a-pre-built-image) if you would rather pull the images from GitHub packages over building them locally.

1. Build 2PC architecture:
    ```terminal
    $ cd opencbdc-tx                                              # change to the project directory
    $ sudo -s                                                     # open a root shell (needed for docker)
    $ docker build --target twophase  -t opencbdc-tx-twophase .   # build the container
    ```
1. Build atomizer architecture:
    ```terminal
    $ cd opencbdc-tx                                              # change to the project directory
    $ sudo -s                                                     # open a root shell (needed for docker)
    $ docker build --target atomizer  -t opencbdc-tx-atomizer .   # build the container
    ```

## Launch the System

**Note:** You will need to both run the system and interact with it; you can either use two shells, or you can add the `--detach` flag when launching the system (note that it will then remain running till you stop it, e.g., with `docker stop`).

_The commands below will build a new image every time that you run it.
You can remove the `--build` flag after the image has been built to avoid rebuilding.
To run the system with our pre-built image proceed to the [next section](#launch-the-system-with-a-pre-built-image) for the commands to run._

1. Run the System
    1. 2PC architecture:
        ```terminal
        $ docker compose --file docker-compose-2pc.yml up --build
        ```
    2. Atomizer architecture:
        ```terminal
        $ docker compose --file docker-compose-atomizer.yml up --build
        ```
2. Launch a container in which to run wallet commands
    1. 2PC architecture:
        ```terminal
        $ docker run --network 2pc-network -ti opencbdc-tx-twophase /bin/bash
        ```
    2. Atomizer architecture:
        ```terminal
        $ docker run --network atomizer-network -ti opencbdc-tx-atomizer /bin/bash
        ```

## Launch the System With a Pre-built Image

We publish new docker images for all commits to `trunk`.
You can find the images [in the Github Container Registry](https://github.com/mit-dci/opencbdc-tx/pkgs/container/opencbdc-tx).

**Note:** You must use `docker compose` (not `docker-compose`) for this approach to work or you will need to pull the image manually `docker pull ghcr.io/mit-dci/opencbdc-tx-twophase` or `docker pull ghcr.io/mit-dci/opencbdc-tx-atomizer`.

1. Run the system
    1. 2PC architecture:
        ```terminal
        $ docker compose --file docker-compose-2pc.yml --file docker-compose-prebuilt-2pc.yml up --no-build
        ```
    1. Atomizer architecture:
        ```terminal
        $ docker compose --file docker-compose-atomizer.yml --file docker-compose-prebuilt-atomizer.yml up --no-build
        ```
1. Launch a container in which to run wallet commands
    1. 2PC architecture:
        ```terminal
        $ docker run --network 2pc-network -ti ghcr.io/mit-dci/opencbdc-tx-twophase /bin/bash
        ```
    1. Atomizer architecture:
        ```terminal
        $ docker run --network atomizer-network -ti ghcr.io/mit-dci/opencbdc-tx-atomizer /bin/bash
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

* Send currency from the first wallet to the second wallet created in the previous step (e.g., 30 atomic units of currency)
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

* Import coins to the receiving wallet using the string after `importinput` from the currency transfer step above
  ```terminal
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool1.dat wallet1.dat importinput cc1f7dc708be5b07e23e125cf0674002ff8546a9342928114bc97031d8b96e750000000000000000d0e4f689b550f623e9370edae235de50417860be0f2f8e924eca9f402fcefeaa1e00000000000000
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool1.dat wallet1.dat sync
  # ./build/src/uhs/client/client-cli 2pc-compose.cfg mempool1.dat wallet1.dat info
  Balance: $0.30, UTXOs: 1, pending TXs: 0
  ```

