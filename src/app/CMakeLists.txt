add_executable(codexbar)

target_sources(codexbar PRIVATE
    main.cpp
    TrayIcon.cpp
    IconRenderer.cpp
    MenuWidget.cpp
)

target_link_libraries(codexbar PRIVATE
    codexbar-core
    Qt6::Widgets
    KF6::StatusNotifierItem
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
)

install(TARGETS codexbar ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${CMAKE_SOURCE_DIR}/resources/codexbar.desktop DESTINATION ${KDE_INSTALL_APPDIR})
