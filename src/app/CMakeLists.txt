add_executable(kdecodexbar)

target_sources(kdecodexbar PRIVATE
    main.cpp
    TrayIcon.cpp
    IconRenderer.cpp
    SettingsDialog.cpp
    MenuWidget.cpp
)

target_link_libraries(kdecodexbar PRIVATE
    kdecodexbar-core
    Qt6::Widgets
    KF6::StatusNotifierItem
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
)

target_include_directories(kdecodexbar PRIVATE ${CMAKE_SOURCE_DIR}/src/core)

install(TARGETS kdecodexbar ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${CMAKE_SOURCE_DIR}/resources/codexbar.desktop DESTINATION ${KDE_INSTALL_APPDIR} RENAME kdecodexbar.desktop)
