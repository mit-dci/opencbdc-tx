// Copyright (c) 2022 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "thread_pool.hpp"

namespace cbdc {
    thread_pool::~thread_pool() {
        std::unique_lock l(m_mut);
        for(auto& t : m_threads) {
            t->m_queue.clear();
            if(t->m_thread.joinable()) {
                t->m_thread.join();
            }
        }
    }

    void thread_pool::push(const std::function<void()>& fn) {
        std::unique_lock l(m_mut);
        auto launched = false;
        for(auto& thr : m_threads) {
            if(thr->m_running) {
                continue;
            }

            thr->m_running = true;
            thr->m_queue.push(fn);
            launched = true;
            break;
        }
        if(launched) {
            return;
        }

        auto thr = std::make_shared<thread_type>();
        m_threads.emplace_back(thr);
        thr->m_running = true;
        thr->m_thread = std::thread([thr]() {
            thread_loop(thr);
        });
        thr->m_queue.push(fn);
    }

    void thread_pool::thread_loop(const std::shared_ptr<thread_type>& thr) {
        auto f = std::function<void()>();
        while(thr->m_queue.pop(f)) {
            f();
            thr->m_running = false;
        }
    }
}
