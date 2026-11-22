add_executable(test_extractor
  test_extractor.cc
)
target_link_libraries(test_extractor
  PRIVATE lazylook_core
)
add_test(NAME ExtractorTests COMMAND test_extractor)

add_executable(test_database
  test_database.cc
)
target_link_libraries(test_database
  PRIVATE lazylook_core
)
add_test(NAME DatabaseTests COMMAND test_database)

add_executable(test_config
  test_config.cc
)
target_link_libraries(test_config
  PRIVATE lazylook_core
)
add_test(NAME ConfigTests COMMAND test_config)

add_executable(test_embedder
  test_embedder.cc
)
target_link_libraries(test_embedder
  PRIVATE lazylook_core
)
add_test(NAME EmbedderTests COMMAND test_embedder)

add_executable(test_indexer_watcher
  test_indexer_watcher.cc
)
target_link_libraries(test_indexer_watcher
  PRIVATE lazylook_core
)
add_test(NAME IndexerWatcherTests COMMAND test_indexer_watcher)

add_executable(test_temporal_parser
  test_temporal_parser.cc
)
target_link_libraries(test_temporal_parser
  PRIVATE lazylook_core
)
add_test(NAME TemporalParserTests COMMAND test_temporal_parser)
