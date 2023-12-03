project(py_runner)

add_library(py_runner impl.cpp
                        py_server.cpp
                        pyutil.cpp
                        pybuffer.cpp)


target_link_libraries(py_runner ${PYTHON_VERSION})

include_directories(/usr/include/${PYTHON_VERSION})