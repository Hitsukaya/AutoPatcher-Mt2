cmake_minimum_required(VERSION 3.16)
project(nyx2_launcher LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-webview2 CONFIG REQUIRED)

add_executable(nyx2_launcher WIN32
    src/main.cpp
    src/ui.cpp
    src/launcher.cpp
    src/patcher.cpp
    src/version.cpp
    src/manifest.cpp
    src/comparator.cpp
    src/file_ops.cpp
    src/http_client.cpp
    app.rc
)

target_compile_features(nyx2_launcher PRIVATE cxx_std_17)

set_target_properties(nyx2_launcher PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_include_directories(nyx2_launcher PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(nyx2_launcher PRIVATE
    nlohmann_json::nlohmann_json
    unofficial::webview2::webview2
    winhttp
    comctl32
    uxtheme
    gdiplus
    shell32
)