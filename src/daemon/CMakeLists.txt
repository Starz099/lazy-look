add_executable(lazyfinderd
  main.cc
)

target_link_libraries(lazyfinderd
  PRIVATE lazyfinder_core
)
