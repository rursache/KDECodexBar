cmake_minimum_required(VERSION 3.16)

project(codexbar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
)

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMInstallIcons)

find_package(KF6 6.0 REQUIRED COMPONENTS
    CoreAddons
    StatusNotifierItem
    Config
    I18n
    WindowSystem
)

# Version: prefer CMake variable (set by packaging), then git, then fallback
if(NOT APP_VERSION)
    execute_process(
        COMMAND git describe --tags --always
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE APP_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT APP_VERSION)
    set(APP_VERSION "unknown")
endif()
add_compile_definitions(APP_VERSION="${APP_VERSION}")

add_subdirectory(src/core)
add_subdirectory(src/app)
