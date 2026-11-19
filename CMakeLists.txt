cmake_minimum_required(VERSION 3.28)

project(lazyfinder
  VERSION 0.1.0
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_custom_target(link_compile_commands ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    "${CMAKE_BINARY_DIR}/compile_commands.json"
    "${CMAKE_SOURCE_DIR}/compile_commands.json"
  VERBATIM
)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

include(GNUInstallDirs)

install(TARGETS lazyfinder lazyfinderd
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(EXISTS "${CMAKE_SOURCE_DIR}/models/embeddinggemma-2-Q8_0.gguf")
  install(FILES "${CMAKE_SOURCE_DIR}/models/embeddinggemma-2-Q8_0.gguf"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/lazyfinder/models
  )
endif()

install(FILES resources/lazyfinder.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES resources/icons/lazyfinder.svg
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

set(CPACK_PACKAGE_NAME "lazyfinder")
set(CPACK_PACKAGE_VENDOR "LazyFinder")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Instant AI & Hybrid Desktop Search for Linux")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "mayankjod016@gmail.com")

# RPM Settings (Fedora, RHEL, openSUSE)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtdeclarative, vulkan-loader")
set(CPACK_RPM_PACKAGE_SUGGESTS "ffmpeg, mpv, tesseract")
set(CPACK_RPM_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-x86_64.rpm")

# Debian / Ubuntu Settings (.deb)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "LazyFinder <mayankjod016@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6widgets6, libqt6qml6, libqt6quick6, libqt6quickcontrols2-6, libqt6dbus6, libqt6network6, libvulkan1, libsqlite3-0")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "ffmpeg, mpv, tesseract-ocr")
set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-amd64.deb")

# Generic Archive
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-x86_64")

include(CPack)
