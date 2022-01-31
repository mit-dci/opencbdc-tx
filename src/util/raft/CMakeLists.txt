project(raft)

add_library(raft console_logger.cpp
                 state_manager.cpp
                 log_store.cpp
                 node.cpp
                 serialization.cpp
                 messages.cpp
                 index_comparator.cpp)
