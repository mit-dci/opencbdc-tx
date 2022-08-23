// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_BLOCKING_QUEUE_H_
#define OPENCBDC_TX_SRC_COMMON_BLOCKING_QUEUE_H_

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace cbdc {
    /// Thread-safe producer-consumer FIFO queue supporting multiple
    /// concurrent producers and consumers.
    /// \tparam type of object stored in the queue.
    template<typename T, typename Q>
    class blocking_queue_internal {
      public:
        blocking_queue_internal() = default;

        blocking_queue_internal(const blocking_queue_internal&) = delete;
        auto operator=(const blocking_queue_internal&)
            -> blocking_queue_internal& = delete;

        blocking_queue_internal(blocking_queue_internal&&) = delete;
        auto operator=(blocking_queue_internal&&)
            -> blocking_queue_internal& = delete;

        /// \brief Destructor.
        ///
        /// Clears the queue and unblocks any waiting consumers.
        ~blocking_queue_internal() {
            clear();
        }

        /// Pushes an element onto the queue and notifies at most one waiting
        /// consumer.
        /// \param item object to push onto the queue.
        auto push(const T& item) -> size_t {
            auto sz = [&]() {
                std::unique_lock<std::mutex> lck(m_mut);
                m_buffer.push(item);
                m_wake = true;
                return m_buffer.size();
            }();
            m_cv.notify_one();
            return sz;
        }

        /// \brief Pops an element from the queue.
        ///
        /// Blocks if the queue is empty. Unblocks on destruction or \ref
        /// clear without returning an element.
        /// \param item object into which to move the popped element.
        /// \return true on success, false if interrupted by \ref clear() or
        ///         destruction.
        [[nodiscard]] auto pop(T& item) -> bool {
            {
                std::unique_lock<std::mutex> lck(m_mut);
                if(m_buffer.empty()) {
                    m_cv.wait(lck, [&] {
                        return m_wake;
                    });
                }

                bool popped{false};
                if(!m_buffer.empty()) {
                    item = std::move(first_item<T, Q>());
                    m_buffer.pop();
                    popped = true;
                    m_wake = !m_buffer.empty();
                }

                return popped;
            }
        }

        /// Clears the queue and unblocks waiting consumers.
        void clear() {
            {
                std::unique_lock<std::mutex> lck(m_mut);
                m_buffer = decltype(m_buffer)();
                m_wake = true;
            }
            m_cv.notify_all();
        }

        /// Removes the wakeup flag for consumers. Must be called after
        /// \ref clear() before re-using the queue. All consumers must have
        /// returned from \ref pop() before calling this method.
        void reset() {
            std::unique_lock l(m_mut);
            m_wake = false;
        }

      private:
        template<typename TT, typename QQ>
        auto first_item() ->
            typename std::enable_if<std::is_same<QQ, std::queue<TT>>::value,
                                    const TT&>::type {
            return m_buffer.front();
        }

        template<typename TT, typename QQ>
        auto first_item() ->
            typename std::enable_if<!std::is_same<QQ, std::queue<TT>>::value,
                                    const TT&>::type {
            return m_buffer.top();
        }

        Q m_buffer;
        std::mutex m_mut;
        std::condition_variable m_cv;
        bool m_wake{false};
    };

    template<typename T>
    using blocking_queue = blocking_queue_internal<T, std::queue<T>>;

    template<typename T, typename C = std::less<T>>
    using blocking_priority_queue
        = blocking_queue_internal<T,
                                  std::priority_queue<T, std::vector<T>, C>>;
}

#endif // OPENCBDC_TX_SRC_COMMON_BLOCKING_QUEUE_H_
