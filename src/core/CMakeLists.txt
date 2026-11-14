add_library(lazyfinder_core STATIC
  lazyfinder.cc
)

target_include_directories(lazyfinder_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
