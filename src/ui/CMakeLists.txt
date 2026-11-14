find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1)

add_executable(lazyfinder
  main.cc
)

target_link_libraries(lazyfinder
  PRIVATE
    lazyfinder_core
    PkgConfig::GTK4
    PkgConfig::ADWAITA
)
