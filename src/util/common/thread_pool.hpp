// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef OPENCBDC_TX_SRC_COMMON_THREAD_POOL_H_
#define OPENCBDC_TX_SRC_COMMON_THREAD_POOL_H_

#include "blocking_queue.hpp"

#include <atomic>
#include <thread>
#include <vector>

namespace cbdc {
    class thread_pool {
      public:
        thread_pool() = default;
        ~thread_pool();

        thread_pool(const thread_pool&) = delete;
        auto operator=(const thread_pool&) -> thread_pool& = delete;
        thread_pool(thread_pool&&) = delete;
        auto operator=(thread_pool&&) -> thread_pool& = delete;

        void push(const std::function<void()>& fn);

      private:
        struct thread_type {
            std::thread m_thread;
            std::atomic_bool m_running{false};
            blocking_queue<std::function<void()>> m_queue;
        };

        std::mutex m_mut;
        std::vector<std::shared_ptr<thread_type>> m_threads;

        static void thread_loop(const std::shared_ptr<thread_type>& thr);
    };
}

#endif
