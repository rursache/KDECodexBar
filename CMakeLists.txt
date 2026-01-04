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

add_subdirectory(src/core)
add_subdirectory(src/app)
