include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(POPPLER_CPP IMPORTED_TARGET poppler-cpp)

set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  llama
  GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
  GIT_TAG        v1.0.0
  GIT_SHALLOW    TRUE
)

FetchContent_Declare(
  sqlite_vec
  URL https://github.com/asg017/sqlite-vec/releases/download/v0.1.9/sqlite-vec-0.1.9-amalgamation.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(llama sqlite_vec)

add_library(sqlite_vec STATIC
  ${sqlite_vec_SOURCE_DIR}/sqlite-vec.c
)
target_compile_definitions(sqlite_vec PRIVATE SQLITE_CORE SQLITE_VEC_STATIC)
target_include_directories(sqlite_vec PUBLIC ${sqlite_vec_SOURCE_DIR})
target_link_libraries(sqlite_vec PUBLIC PkgConfig::SQLITE3 m)

add_library(lazyfinder_core STATIC
  lazyfinder.cc
  embedder.cc
  database.cc
  extractor.cc
  indexer.cc
)

target_compile_definitions(lazyfinder_core
  PUBLIC
    SQLITE_CORE
    SQLITE_VEC_STATIC
)

if(POPPLER_CPP_FOUND)
  target_compile_definitions(lazyfinder_core PUBLIC HAVE_POPPLER)
  target_link_libraries(lazyfinder_core PUBLIC PkgConfig::POPPLER_CPP)
endif()

target_include_directories(lazyfinder_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(lazyfinder_core
  PUBLIC
    llama
    sqlite_vec
    PkgConfig::SQLITE3
)

