set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick QuickControls2 Widgets DBus Network REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(lazylook
  main.cc
  desktop_launcher.cc
  desktop_launcher.hh
  search_controller.cc
  search_controller.hh
  results_model.cc
  results_model.hh
)

qt_add_qml_module(lazylook
  URI LazyLook
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/SearchBox.qml
    qml/ResultsList.qml
    qml/ResultDelegate.qml
    qml/SettingsSheet.qml
    qml/StatusBar.qml
    qml/OnboardingView.qml
    qml/Icon.qml
)

target_link_libraries(lazylook
  PRIVATE
    lazylook_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::DBus
    Qt6::Network
)
