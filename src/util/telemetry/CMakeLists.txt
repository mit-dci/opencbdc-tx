project(telemetry)

add_library(telemetry telemetry.cpp
                      format.cpp)

target_link_libraries(telemetry serialization)
