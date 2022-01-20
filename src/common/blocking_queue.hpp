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
    template<typename T>
    class blocking_queue {
      public:
        blocking_queue() = default;

        blocking_queue(const blocking_queue&) = delete;
        auto operator=(const blocking_queue&) -> blocking_queue& = delete;

        blocking_queue(blocking_queue&&) = delete;
        auto operator=(blocking_queue&&) -> blocking_queue& = delete;

        /// \brief Destructor.
        ///
        /// Clears the queue and unblocks any waiting consumers.
        ~blocking_queue() {
            clear();
        }

        /// Pushes an element onto the queue and notifies at most one waiting
        /// consumer.
        /// \param item object to push onto the queue.
        void push(const T& item) {
            {
                std::unique_lock<std::mutex> lck(m_mut);
                m_buffer.push(item);
                m_wake = true;
            }
            m_cv.notify_one();
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
                    item = std::move(m_buffer.front());
                    m_buffer.pop();
                    popped = true;
                }

                m_wake = !m_buffer.empty();

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

      private:
        std::queue<T> m_buffer;
        std::mutex m_mut;
        std::condition_variable m_cv;
        bool m_wake{false};
    };
}

#endif // OPENCBDC_TX_SRC_COMMON_BLOCKING_QUEUE_H_
