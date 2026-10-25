cmake_minimum_required(VERSION 3.16)
project(octobrew_tui VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# FTXUI (macOS: brew install ftxui). Sem ele no sistema, como no Ubuntu do CI,
# OCTOBREW_TUI_FETCH_FTXUI baixa a 6.1.9, a versão mais antiga suportada (a
# mesma do octopkg-tui); o Homebrew tem a 7.x.
option(OCTOBREW_TUI_FETCH_FTXUI "Baixa o FTXUI se não estiver instalado" OFF)
find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
    if(NOT OCTOBREW_TUI_FETCH_FTXUI)
        message(FATAL_ERROR "FTXUI não encontrado: brew install ftxui, ou -DOCTOBREW_TUI_FETCH_FTXUI=ON")
    endif()
    include(FetchContent)
    FetchContent_Declare(ftxui
        URL https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v6.1.9.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ftxui)
endif()

# Núcleo: o brew, o JSON, as notícias, i18n, preferências e o modelo da lista. C++
# padrão, sem dependências.
add_library(octobrew_core STATIC
    src/core/process.cpp
    src/core/json.cpp
    src/core/news.cpp
    src/core/brew.cpp
    src/core/i18n.cpp
    src/core/settings.cpp
    src/model/listmodel.cpp
)
target_include_directories(octobrew_core PUBLIC src/core src/model)
target_link_libraries(octobrew_core PUBLIC Threads::Threads)

# Interface (FTXUI), compartilhada pelo executável e pelos testes de tela.
add_library(octobrew_ui STATIC
    src/tui/icons.cpp
    src/tui/theme.cpp
    src/tui/view.cpp
    src/tui/app.cpp
)
target_include_directories(octobrew_ui PUBLIC src/tui)
target_link_libraries(octobrew_ui PUBLIC octobrew_core ftxui::component ftxui::dom ftxui::screen)
# Fonte única da versão: project(... VERSION ...); usada no --version e no Sobre.
target_compile_definitions(octobrew_ui PUBLIC OCTOBREW_TUI_VERSION="${PROJECT_VERSION}")

add_executable(octobrew-tui src/main.cpp)
target_link_libraries(octobrew-tui PRIVATE octobrew_ui)

foreach(target octobrew_core octobrew_ui octobrew-tui)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
    )
endforeach()

install(TARGETS octobrew-tui RUNTIME DESTINATION bin)

# Testes (Catch2 3). test_integration roda contra o brew real, só lendo.
option(OCTOBREW_TUI_BUILD_TESTS "Compila os testes" OFF)
if(OCTOBREW_TUI_BUILD_TESTS)
    find_package(Catch2 3 REQUIRED)
    enable_testing()
    foreach(test test_core test_model test_view test_integration)
        add_executable(${test} tests/${test}.cpp)
        target_link_libraries(${test} PRIVATE octobrew_ui Catch2::Catch2WithMain)
        target_compile_options(${test} PRIVATE
            $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
        )
        add_test(NAME ${test} COMMAND ${test})
    endforeach()
    # Sem o brew, todos os casos da integração são ignorados e o Catch2 sai com 4.
    set_tests_properties(test_integration PROPERTIES SKIP_RETURN_CODE 4)
endif()
