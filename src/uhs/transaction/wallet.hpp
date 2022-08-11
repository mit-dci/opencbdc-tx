// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_TRANSACTION_WALLET_H_
#define OPENCBDC_TX_SRC_TRANSACTION_WALLET_H_

#include "uhs/transaction/transaction.hpp"
#include "util/common/config.hpp"
#include "util/common/hashmap.hpp"
#include "util/common/random_source.hpp"

#include <atomic>
#include <fstream>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <secp256k1.h>
#include <set>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace cbdc::transaction {
    /// \brief Cryptographic wallet for digital currency assets and secrets.
    ///
    /// Stores unspent transaction outputs (UTXOs), and public/private key
    /// pairs for Pay-to-Public-Key transaction attestations.
    class wallet {
      public:
        /// \brief Constructor.
        ///
        /// Initializes the randomization engine for key shuffling.
        wallet();

        /// \brief Constructor.
        ///
        /// \param log (optional) logger to write debugging to.
        explicit wallet(std::shared_ptr<logging::log> log);

        /// Initializes the randomization engine for key shuffling.
        void init();

        /// \brief Mints new spendable outputs.
        ///
        /// Generates the specified number spendable outputs, each with the
        /// specified value, and returns a transaction with the result to
        /// submit to the transaction processing system.
        /// \param n_outputs number of new spendable outputs to create.
        /// \param output_val value of the amount to associate with each output
        ///                   in the base unit of the currency.
        /// \return the transaction which mints the new coins.
        auto mint_new_coins(size_t n_outputs, uint32_t output_val) -> full_tx;

        /// \brief Generates a new send transaction with a set value.
        ///
        /// Generates a transaction to transfer a specified amount from this
        /// wallet to a recipient's target address. Returns the transaction
        /// for wallet clients to submit to the processing system and
        /// syndicate to recipients.
        /// \param amount the amount to send in the base unit of the currency.
        /// \param payee the destination address of the transfer.
        /// \param sign_tx true if the wallet should sign this transaction.
        /// \return the completed transaction.
        /// \see \ref export_send_inputs
        auto send_to(uint32_t amount, const pubkey_t& payee, bool sign_tx)
            -> std::optional<full_tx>;

        /// \brief Generates a new send transaction with the specified number
        ///        of inputs and outputs.
        ///
        /// This method is best-effort: it will return an empty optional if the
        /// method cannot use the specified number of inputs to create the
        /// specified number of outputs. Likewise, the method will return an
        /// empty optional if the wallet contains fewer UTXOs than the
        /// specified input count.
        /// \param input_count number of inputs to include in the Tx.
        ///                    Must be >0.
        /// \param output_count number of outputs to include in the Tx.
        ///                     Must be >0.
        /// \param payee destination pubkey for the Tx outputs.
        /// \param sign_tx true if the transaction should be signed.
        /// \return the completed transaction.
        auto send_to(size_t input_count,
                     size_t output_count,
                     const pubkey_t& payee,
                     bool sign_tx) -> std::optional<full_tx>;

        /// \brief Generates a transaction sending multiple outputs of a set
        ///        value.
        ///
        /// Generates a transaction to transfer a specified number of outputs
        /// with a fixed amount from this wallet to a recipient's target
        /// address. Returns the transaction for wallet clients to submit to
        /// the processing system and syndicate to recipients.
        /// \param output_count number of outputs to send to the recipient.
        /// \param value the amount to send per outputs in the base unit of the
        ///              currency.
        /// \param payee the destination address of the transfer.
        /// \param sign_tx true if the wallet should sign this transaction.
        /// \return the completed transaction.
        /// \see \ref export_send_inputs
        auto fan(size_t output_count,
                 uint32_t value,
                 const pubkey_t& payee,
                 bool sign_tx) -> std::optional<transaction::full_tx>;

        /// \brief Extracts the transaction data that recipients need from
        ///        senders to confirm pending transfers.
        ///
        /// Returns the output of the send transaction intended for the target
        /// recipient, converted to an input. Senders transmit this data to
        /// recipients so that they can confirm the transaction on their end
        /// and spend the outputs.
        /// \param send_tx the send transaction generated by this wallet's \ref
        ///                send_to function.
        /// \param payee the destination address of the transfer.
        /// \return inputs to transmit to the send transaction recipient, or
        ///         empty vector if there is no transaction output with the
        ///         corresponding payee information.
        static auto export_send_inputs(const full_tx& send_tx,
                                       const pubkey_t& payee)
            -> std::vector<input>;

        /// Generates a new public key at which this wallet can receive
        /// payments via \ref send_to.
        /// \return a new public key.
        auto generate_key() -> pubkey_t;

        /// \brief Marks the wallet as having pre-seeded outputs to spend.
        ///
        /// Given that the outputs are deterministic, the wallet will create
        /// them on-the-fly while spending them.
        /// \param privkey the private key to use for the seeded outputs.
        /// \param value the value of each seeded output.
        /// \param begin_seed the start index to use for the seeded outputs.
        /// \param end_seed the end index to use for the seeded outputs.
        /// \return true if setting the seed parameters succeeded.
        auto seed(const privkey_t& privkey,
                  uint32_t value,
                  size_t begin_seed,
                  size_t end_seed) -> bool;

        /// \brief Marks the wallet as having read-only pre-seeded outputs to
        ///        spend.
        ///
        /// This method marks the wallet as having pre-seeded outputs which
        /// can only generate unsigned transactions. It is called from \ref
        /// seed – where \ref seed also provides the wallet with the private
        /// key and makes the outputs spendable. The shard seeder to uses this
        /// read-only variant to allow for calls to \ref
        /// create_seeded_transaction.
        /// \param witness_commitment the witness commitment to use for the
        ///                           seeded outputs.
        /// \param value the value to use for the seeded outputs.
        /// \param begin_seed the start index to use for the seeded outputs.
        /// \param end_seed the end index to use for the seeded outputs.
        void seed_readonly(const hash_t& witness_commitment,
                           uint32_t value,
                           size_t begin_seed,
                           size_t end_seed);

        /// \brief Confirms a transaction.
        ///
        /// 1. Adds new outputs. Adds to the total balance of this wallet the
        ///    balance of each of the transaction's outputs with a witness
        ///    program commitment matching one of this wallet's witness
        ///    programs. Converts those outputs to inputs, and stores them in
        ///    the wallet as spendable UTXOs.
        /// 2. Subtracts the spent outputs. Subtracts the balance for each of
        ///    the transaction's inputs that are UTXOs stored in this wallet,
        ///    and drops those UTXOs from the wallet.
        /// \param tx the transaction to confirm.
        void confirm_transaction(const full_tx& tx);

        /// \brief Retrieves the spending-keypairs for a transaction
        ///
        /// Returns the keypairs (one per input, in-order) needed to authorize
        /// spending the transaction's inputs.
        ///
        /// \param tx the transaction to fetch spending keys for
        /// \return the list of keypairs (or std::nullopt if any output is
        ///         unspendable)
        auto spending_keys(const full_tx& tx) const
            -> std::optional<std::vector<std::pair<privkey_t, pubkey_t>>>;

        /// Signs each of the transaction's inputs using Schnorr signatures.
        /// \param tx the transaction whose inputs to sign.
        void sign(full_tx& tx) const;

        /// Checks if the input is spendable by the current wallet.
        /// \param in the input to check.
        auto is_spendable(const input& in) const -> bool;

        /// Returns the total balance of the wallet, e.g. the sum total value
        /// of all the UTXOs this wallet contains.
        /// \return the wallet balance in the base unit of the currency.
        auto balance() const -> uint64_t;

        /// Returns the number of UTXOs stored in this wallet.
        /// \return number of UTXOs.
        auto count() const -> size_t;

        /// Save the state of the wallet to a binary data file.
        /// \param wallet_file path to wallet file location.
        void save(const std::string& wallet_file) const;

        /// Overwrites the current state of the wallet with data loaded from a
        /// file saved via the Wallet::save function.
        /// \param wallet_file path to wallet file location.
        void load(const std::string& wallet_file);

        /// \brief Creates a new transaction from seeded outputs.
        ///
        /// Creates a new transaction that receives a spendable input from the
        /// seed set based on the parameters passed in a preceding call to the
        /// \ref seed function. Used from the shard-seeder tool to consolidate
        /// this logic in one place.
        /// \param seed_idx the index in the seed set for which to generate the
        ///                 transaction.
        /// \returns the generated transaction.
        auto create_seeded_transaction(size_t seed_idx)
            -> std::optional<full_tx>;

        /// Given a set of credit inputs, add the UTXOs and update the wallet's
        /// balance.
        /// \param credits the inputs to add to the wallet's set of UTXOs.
        void confirm_inputs(const std::vector<input>& credits);

      private:
        /// Locks access to m_utxos and m_balance (the sum of the UTXOs).
        /// \warning Do not lock simultaneously with m_keys_mut.
        mutable std::shared_mutex m_utxos_mut;

        /// Stores the current set of spendable inputs.
        std::map<out_point, input> m_utxos_set;
        /// Stores the blinds associated with a spendable input
        size_t m_seed_from{0};
        size_t m_seed_to{0};
        uint32_t m_seed_value{0};
        hash_t m_seed_witness_commitment{0};
        /// Queue of spendable inputs, oldest first.
        std::list<input> m_spend_queue;

        /// Locks access to m_keys and related members m_pubkeys and
        /// m_witness_programs.
        /// \warning Do not lock simultaneously with m_utxos_mut.
        mutable std::shared_mutex m_keys_mut;
        std::unordered_map<pubkey_t,
                           privkey_t,
                           hashing::const_sip_hash<pubkey_t>>
            m_keys;
        std::vector<pubkey_t> m_pubkeys;
        std::default_random_engine m_shuffle;
        std::shared_ptr<logging::log> m_log;

        // TODO: currently this map grows unbounded, we need to garbage
        //       collect it
        std::unordered_map<hash_t, pubkey_t, hashing::const_sip_hash<hash_t>>
            m_witness_programs;

        /// Creates a new input from the seed set based on the parameters
        /// passed in a preceding call to the \ref seed function.
        /// \param seed_idx the index in the seed set to generate the input
        ///                 for.
        /// \returns the generated input to use in a transaction.
        auto create_seeded_input(size_t seed_idx)
            -> std::optional<transaction::input>;

        struct GensDeleter {
            explicit GensDeleter(secp256k1_context* ctx) : m_ctx(ctx) {}

            void operator()(secp256k1_bulletproofs_generators* gens) const {
                secp256k1_bulletproofs_generators_destroy(m_ctx, gens);
            }

            secp256k1_context* m_ctx;
        };

        /// should be twice the bitcount of the range-proof's upper bound
        ///
        /// e.g., if proving things in the range [0, 2^64-1], it should be 128.
        static const inline auto generator_count = 128;

        std::unique_ptr<secp256k1_bulletproofs_generators, GensDeleter>
            m_generators{
                secp256k1_bulletproofs_generators_create(m_secp.get(),
                                                         generator_count),
                GensDeleter(m_secp.get())};

        static const inline auto m_secp
            = std::unique_ptr<secp256k1_context,
                              decltype(&secp256k1_context_destroy)>(
                secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                         | SECP256K1_CONTEXT_VERIFY),
                &secp256k1_context_destroy);

        static const inline auto m_random_source
            = std::make_unique<random_source>(config::random_source);

        /// Given a set of credit inputs and a set of debits, add and remove
        /// respective the UTXOs and update the wallet's balance.
        /// \param credits the inputs to add to the wallet's set of UTXOs.
        /// \param debits the inputs to remove from the wallet's set of UTXOs.
        void update_balance(const std::vector<input>& credits,
                            const std::vector<input>& debits);

        auto accumulate_inputs(uint64_t amount)
            -> std::optional<std::pair<full_tx, uint64_t>>;
    };
}

#endif // OPENCBDC_TX_SRC_TRANSACTION_WALLET_H_
