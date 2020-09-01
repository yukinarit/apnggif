cmake_minimum_required(VERSION 3.3)
project(apnggif)

file(GLOB ZLIB ${CMAKE_CURRENT_LIST_DIR}/zlib/*.c)
file(GLOB LIBPNG ${CMAKE_CURRENT_LIST_DIR}/libpng/*.c)

find_package(Python REQUIRED COMPONENTS Development Interpreter)
message(STATUS "Python include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Python libraries: ${Python_LIBRARIES}")

add_library(apnggif
  apng2gif.cpp
  py.cpp
  ${ZLIB}
  ${LIBPNG})
target_include_directories(apnggif PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/zlib
  ${CMAKE_CURRENT_LIST_DIR}/libpng
  ${Python_INCLUDE_DIRS})
target_compile_definitions(apnggif PUBLIC NDEBUG)
target_compile_options(apnggif PUBLIC -fPIC)
target_link_directories(apnggif PUBLIC ${Python_LIBRARY_DIRS})
target_link_libraries(apnggif ${Python_LIBRARIES})
set_target_properties(apnggif PROPERTIES
    PREFIX ""
    OUTPUT_NAME "apnggif"
    LINKER_LANGUAGE C)
