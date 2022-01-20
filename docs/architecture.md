# Overview

This document describes the high-level operation of both of the system architectures.
It describes each of the system components and how they interact with each other.
It also describes the data model implemented by both architectures and provides some discussion of its limitations.
The intention is to provide a more accurate and lower-level picture than the description in our research paper so that it is clearer what functionality to expect when reading the code.

# Data model

## Transactions

Our data model decouples transaction validation from execution. Transactions pass through three steps toward completion in the system.

1. Transaction-local validation (see `cbdc::transaction::full_tx` and `cbdc::transaction::validation`)
    - The transaction is checked for invariants that do not relate to the current state of the system.
    - For example, cryptographic signatures, input and output values and duplicate inputs can be checked during this step.
1. Compaction (see `cbdc::transaction::compact_tx`)
    - The transaction is converted to a list of input and output commitments.
      The commitments are cryptographic hashes (SHA256 in our implementation) that uniquely represent an unspent output in the system, without needing to store the underlying output data.
1. Ordering and Settlement
    - The compact transaction is ordered with respect to other transactions, the inputs are checked to ensure they are unspent, and the input and output hashes are atomically destroyed and created, respectively.
    - This step is implemented differently in the two-phase commit architecture versus the atomizer architecture.

Breaking transaction processing up in the above way has a number of interesting implications.
First, the transaction format and validation is completely abstracted from the settlement layer.
This enables us to support varying formats and validation rules without requiring changes to large portions of the system.
However, it requires that all transaction invariants (aside from whether coins are available to spend) can be checked without reference to the system state.
It also requires that outputs can be uniquely committed to by a cryptographic hash.

The primary trade-off from an end-user perspective is that the transaction protocol is interactive between sender and receiver.
Since only commitments to outputs are retained by the system, the sender of funds must give the output information to the recipient.
The system presently does not have an in-band solution for communicating raw output information, so this process happens out-of-band.
A transaction protocol that does not require out-of-band communication between transacting parties while storing only output commitments in the database is an open area of research.

### Sentinel

Sentinels in our system implement both transaction-local validation and compaction.
Users interact directly with the sentinel to send transactions to the system for settlement.

#### Execute Transaction

Sentinels have a single RPC `execute_transaction` that performs transaction-local validation.
If the transaction is valid, the sentinel compacts the transaction and forwards it to the UHS implementation for ordering and settlement.
The sentinel responds to the user with the result of validation, and whether the transaction was settled to completion.

## Unspent Hash Set (UHS)

Both architectures implement an unspent hash set (UHS).
This data structure provides atomic creation and deletion of a list of input and output commitments in a transaction.
If one or more of the inputs to a transaction do not exist in the UHS, the transaction is aborted without mutating the set.

# UHS Implementations

## Two-Phase Commit

The two-phase commit architecture implements a UHS using conservative two-phase locking distributed among multiple shards using the two-phase commit protocol.
Shards each store a subset of the output commitments in the overall UHS.
Commitments are paired with a lock which reserves an output for spending during the execution of the 2PC/C2PL protocol.
Coordinators receive validated compact transactions from sentinels and orchestrate their execution between the shards.

### Coordinator

Coordinators receive compact transactions from sentinels and make RPCs to shards to verify that the transaction inputs are available to spend.
Once a coordinator has locked all of the transaction's inputs, the coordinator instructs the shards to complete the transaction by atomically deleting the locked inputs and creating the new outputs.
For fault tolerance, coordinators are replicated in Raft clusters.
If the leader coordinator fails during the two-phase commit protocol, the new leader recovers the transaction from the last successful step of the protocol.
For scalability, there can be multiple coordinator clusters running simultaneously.
Coordinators execute multiple transactions in a batch to amortize the cost of replication.

#### Execute Transaction (see `cbdc::coordinator::controller::execute_transaction`)

Coordinators have one RPC used by sentinels to execute a transaction in the UHS.
The coordinator adds the transaction to the current batch.
Coordinators have a configurable number of threads executing batches in parallel.
Once a thread is available the batch is sealed and assigns a random identifier to the batch.
The coordinator then replicates the contents of the batch.
The coordinator determines which shards will need to participate in the batch execution based on the transaction inputs and outputs.
For each participating shard, the coordinator makes a "lock" RPC containing the subset of input and output hashes in the batch pertaining to the shard.
The shards lock the given input hashes if they exist and attaches the batch ID to the lock, preventing other coordinators or batches from using the lock in a different context.
The shards respond with a message identifying which inputs in the batch were successfully locked.
The coordinator combines the responses from each shard to determine which transactions in the batch have all their inputs locked and can be completed, and which have missing inputs and need to be aborted.

The coordinator replicates its decision about which transactions to complete or abort and issues an "apply" RPC to the participating shards.
The apply RPC indicates which transactions in the batch to complete (by deleting the input and adding the output hashes to the UHS), and which to abort (by unlocking any previously locked input hashes associated with the transaction).
The shards respond to apply to indicate that it has completed successfully.
The coordinator replicates that all of the apply RPCs have completed and sends a "discard" RPC to each participating shard, indicating that the batch is complete and can be forgotten by the shards.
Once all of the shards have responded to the discard RPC, the coordinator replicates that the batch is complete and forgets about it.
Finally, the coordinator responds to each of the sentinels that issued the execute transaction RPCs for transactions in the batch indicating whether the transaction completed or was aborted.

If the leader coordinator fails, the new leader retrieves all the in-progress batches from the replicated state machine and restarts the batch from the last known completed step of the batch execution.
Only the current raft leader in the coordinator cluster actively executes batches, issues RPCs to shards, and listens for incoming RPCs from sentinels.

### Locking Shard

Shards store a subset of the UHS based on a fixed range of hash prefixes.
Like coordinators, shards are replicated in raft clusters and only the leader processes RPCs.
Shards combine each UHS element with a lock labelled with an identifier to determine which transaction batch holds the lock, if any.
Shards listen on two separate RPC interfaces, one for coordinators allowing mutation of the UHS, and another providing a read-only view of the UHS for end-users.
If a shard receives the same RPC more than once consecutively (i.e., two consecutive "lock" RPCs with the same batch ID), the shard responds with the previous response without performing additional mutations of the UHS.

#### Lock (see `cbdc::locking_shard::locking_shard::lock_outputs`)

Coordinators use the lock RPC to atomically lock multiple UHS elements on the shard.
The shard iterates over each transaction in the batch and searches for its input hashes in the UHS.
If the hash is present and unlocked, the shard locks the hash and labels the lock with the batches unique ID.
The shard stores the transactions provided in the batch so they do not have to be re-sent for the subsequent apply RPC from the coordinator.
The shard responds to the coordinator indicating which transactions in the batch had all their inputs locked successfully.

#### Apply (see `cbdc::locking_shard::locking_shard::apply_outputs`)

The apply RPC references the batch ID of a previous lock RPC and includes a list of booleans indicating which transactions in the batch should be completed or aborted.
The shard retrieves the transactions saved from the lock RPC and unlocks any previously locked hashes if the transaction is to be aborted.
If a transaction is to be completed, the shard deletes the locked input hashes from the UHS and adds the output hashes.
The shard also saves the transaction ID of each completed transaction in the batch if the transaction ID is within its hash prefix range.
The shard responds to the coordinator indicating that the apply command has completed.

#### Discard (see `cbdc::locking_shard::locking_shard::discard_dtx`)

The discard RPC indicates to the shard that a particular batch has completed on all other shards and can now be forgotten.
The shard deletes the response to the prior apply RPC associated with the given batch ID.
If the batch ID has already been discarded, the RPC has no effect.

#### Query UHS ID (see `cbdc::locking_shard::locking_shard::check_unspent`)

End-users use this RPC to query a read-only snapshot of the UHS.
The RPC indicates whether a given hash is currently present in the shard's snapshot of the UHS.

#### Query TX ID (see `cbdc::locking_shard::locking_shard::check_tx_id`)

End-users use this RPC to whether a given transaction ID has been completed on the shard.

## Atomizer

The atomizer architecture implements a UHS by ordering transactions into a linear sequence of batches called "blocks".
A replicated service called an "atomizer" collects notifications about valid transactions from shards and emits a new block at a fixed time interval.
Shards apply the block to their own subset of the UHS atomically.
Like in the two-phase commit architecture, shards store a fixed subset of the overall UHS based on a prefix of the hash.
Unlike in two-phase commit, the shards are not replicated in a state machine and redundancy is achieved via multiple shards with overlapping prefix ranges.
An archiving service provides long-term storage of historical blocks for future auditing and to allow shards to catch up to the most recent block in the event of a failure.
A service called the "watchtower" indexes recent blocks so that end-users can query the status of recent transactions.

### Shard

#### Digest Transaction

Sentinels call this RPC to notify shards about a new transaction.
Sentinels issue the RPC to shards that hold input hashes relevant to the given transaction, selecting one shard for each hash if multiple shards overlap on the same prefix.
The shard checks whether the transaction inputs are present in its UHS and issues a "digest transaction notification" RPC to the atomizer service.
If a transaction contains an input hash which is not present in its UHS, the shard calls the "digest error" RPC on the watchtower with the transaction ID and which UHS elements could not be found.

#### Digest Block

The atomizer calls this RPC to notify shards about a new block.
If the block is contiguous with the previously digested block, the shard atomically applies the transactions to its UHS, deleting input hashes and adding output hashes.
If the block is not contiguous (because the shard has missed previous block notifications), the shard first contacts the archiver to download and digest the missing blocks, before applying the current block.

### Atomizer

The atomizer is a replicated service using raft.
At regular time intervals, the leader atomizer replicated and broadcasts a block to other subscribed services.
Only the leader atomizer processes RPCs and broadcasts blocks.
If the leader fails, the new leader continues generating blocks, carrying on from the most recently created block.

#### Digest Transaction Notification

Shards call this RPC to notify the atomizer service about a transaction it has received from sentinels.
The notification contains the compact transaction, a list of which input hashes the shard attests are presently available to spend in its UHS, and the block height the attestations are valid for.
Once the atomizer has received a notification attesting to every input for a given transaction, the atomizer adds the transaction to the current block and marks the inputs as spent in a cache.
The atomizer maintains the spend input cache for a fixed number of prior blocks.
This allows the atomizer to accept attestations from shards with a stale block height as the atomizer can check the spent input cache to ensure the attestations are still valid despite not being from the most recent view of the UHS.
If a transaction does not receive a full set of notifications for each input hash before being evicted from the cache, the atomizer calls the "digest error" RPC on the watchtower with the transaction ID and which UHS elements did not receive timely notifications.
Similarly, if a shard sends a transaction notification for which its inputs are already in the spent input cache, the atomizer submits this error to the watchtower.

#### Get Block

Archivers use this RPC to retrieve missed blocks from the atomizer.
If the atomizer leader fails after replicating a new block but before broadcasting it, the archiver will need to request the block from the new leader.
The archiver can use this RPC to request blocks at a specific height.

#### Prune Block

Archivers use this RPC to notify the atomizer cluster that all block including and below the given block height have been persisted for long-term storage.
This allows the atomizer to delete historic blocks from memory.

### Archiver

#### Digest Block

The atomizer service calls this RPC to notify the archiver about a new block.
If the block is not contiguous with the previously digested block, the archiver calls the "get block" RPC on the atomizer cluster to first digest any missed blocks.
The archiver persists the block for long-term storage and retrieval by other services.

#### Get Block

Shards and watchtowers call this RPC to retrieve historic blocks when they receive a block from the atomizer that is not contiguous with their most recently digested block.

### Watchtower

#### Digest Block

The atomizer service calls this RPC to notify the watchtower about a new block.
If the block is not contiguous with the previously digested block, the watchtower calls the "get block" RPC on the archiver to first digest any missed blocks.
The watchtower iterates over the transactions in the block and builds an index of recently executed transactions.
The index is time-limited based on a fixed block height and discards old transactions in the index once they have expired.

#### Query Transaction Status

End-users call this RPC to determine whether a given UHS element has recently been spent or created.
It also returns errors related to a given UHS element that prevented transaction execution.

#### Digest Error

Shards and the atomizer service call this RPC to report an error with a transaction to the watchtower.
The watchtower maintains a time-limited index of recent errors associated with a particular UHS element.

