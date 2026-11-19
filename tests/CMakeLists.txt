add_executable(test_extractor
  test_extractor.cc
)
target_link_libraries(test_extractor
  PRIVATE lazyfinder_core
)
add_test(NAME ExtractorTests COMMAND test_extractor)

add_executable(test_database
  test_database.cc
)
target_link_libraries(test_database
  PRIVATE lazyfinder_core
)
add_test(NAME DatabaseTests COMMAND test_database)
