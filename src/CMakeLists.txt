project(sys)

include_directories(.)

add_compile_options(-Wconversion)

add_subdirectory(uhs)
add_subdirectory(util)
add_subdirectory(3pc)
