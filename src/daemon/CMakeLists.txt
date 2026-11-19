add_executable(lazylookd
  main.cc
)

target_link_libraries(lazylookd
  PRIVATE lazylook_core
)
