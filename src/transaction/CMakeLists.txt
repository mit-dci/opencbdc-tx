project(transaction)

add_library(transaction transaction.cpp
                        messages.cpp
                        validation.cpp
                        wallet.cpp)
