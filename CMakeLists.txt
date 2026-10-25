cmake_minimum_required(VERSION 3.16)
project(octobrew VERSION 0.4.0 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)   # src/gui/macos.mm: o AppKit, para o notificador fora do Dock
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Threads REQUIRED)
# Qt6 (macOS: brew install qtbase; Ubuntu: apt install qt6-base-dev). A 6.4
# (a do Ubuntu 24.04) é a mais antiga suportada.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# O núcleo do octobrew-tui, do subtree em tui/ (git subtree): o brew, o JSON,
# as notícias, i18n, preferências e o modelo da lista. C++ padrão, sem Qt.
add_library(octobrew_core STATIC
    tui/src/core/process.cpp
    tui/src/core/json.cpp
    tui/src/core/news.cpp
    tui/src/core/brew.cpp
    tui/src/core/i18n.cpp
    tui/src/core/settings.cpp
    tui/src/model/listmodel.cpp
)
target_include_directories(octobrew_core PUBLIC tui/src/core tui/src/model)
target_link_libraries(octobrew_core PUBLIC Threads::Threads)

# A interface (Qt6 Widgets), compartilhada pelo aplicativo e pelos testes.
add_library(octobrew_gui STATIC
    src/gui/texts.cpp
    src/gui/pages.cpp
    src/gui/packagemodel.cpp
    src/gui/transactiondialog.cpp
    src/gui/askpass.cpp
    src/gui/preferences.cpp
    src/gui/notifier.cpp
    src/gui/mainwindow.cpp
    resources/resources.qrc
)
if(APPLE)
    target_sources(octobrew_gui PRIVATE src/gui/macos.mm)
    target_link_libraries(octobrew_gui PUBLIC "-framework AppKit")
else()
    target_sources(octobrew_gui PRIVATE src/gui/macos_stub.cpp)
endif()
target_include_directories(octobrew_gui PUBLIC src/gui)
target_link_libraries(octobrew_gui PUBLIC octobrew_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent)
# Fonte única da versão: project(... VERSION ...); usada no --version e no Sobre.
target_compile_definitions(octobrew_gui PUBLIC OCTOBREW_VERSION="${PROJECT_VERSION}")

# No macOS, um bundle OctoBrew.app, com o ícone do polvo.
set(app_icon ${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/OctoBrew.icns)
set_source_files_properties(${app_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
add_executable(octobrew MACOSX_BUNDLE WIN32 src/main.cpp ${app_icon})
target_link_libraries(octobrew PRIVATE octobrew_gui)
set_target_properties(octobrew PROPERTIES
    OUTPUT_NAME OctoBrew
    MACOSX_BUNDLE_BUNDLE_NAME OctoBrew
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.cleberalves40.octobrew
    MACOSX_BUNDLE_ICON_FILE OctoBrew.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_COPYRIGHT "GPL-2.0-or-later")
if(NOT APPLE)
    set_target_properties(octobrew PROPERTIES OUTPUT_NAME octobrew)
endif()

# O diálogo de senha que o sudo chama (SUDO_ASKPASS). No macOS, dentro do
# bundle, ao lado do executável (Contents/MacOS), onde o OctoBrew o procura.
add_executable(octobrew-askpass src/askpass_main.cpp)
target_link_libraries(octobrew-askpass PRIVATE octobrew_gui)
# O notificador da barra de menus, também dentro do bundle.
add_executable(octobrew-notifier src/notifier_main.cpp)
target_link_libraries(octobrew-notifier PRIVATE octobrew_gui)
if(APPLE)
    set_target_properties(octobrew-askpass octobrew-notifier PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "$<TARGET_BUNDLE_CONTENT_DIR:octobrew>/MacOS")
    add_dependencies(octobrew octobrew-askpass octobrew-notifier)
endif()

foreach(target octobrew_core octobrew_gui octobrew octobrew-askpass octobrew-notifier)
    target_compile_options(${target} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>)
endforeach()

install(TARGETS octobrew BUNDLE DESTINATION . RUNTIME DESTINATION bin)
if(NOT APPLE)
    install(TARGETS octobrew-askpass octobrew-notifier RUNTIME DESTINATION bin)
endif()

# Testes (QtTest, sem tela: QT_QPA_PLATFORM=offscreen). O brew falso e as
# amostras do brew 7 vêm dos testes do octobrew-tui (tui/tests).
option(OCTOBREW_BUILD_TESTS "Compila os testes" OFF)
if(OCTOBREW_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    foreach(test tst_model tst_mainwindow tst_transactions tst_homebrew tst_macos tst_integration)
        add_executable(${test} tests/${test}.cpp)
        target_include_directories(${test} PRIVATE tui/tests)
        target_link_libraries(${test} PRIVATE octobrew_gui Qt6::Test)
        target_compile_options(${test} PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>)
        add_test(NAME ${test} COMMAND ${test})
        set_tests_properties(${test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
    # Todo texto da GUI com o português e o espanhol.
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
        add_test(NAME translations COMMAND ${Python3_EXECUTABLE} -I ${CMAKE_CURRENT_SOURCE_DIR}/tests/check-translations.py
                 ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()
