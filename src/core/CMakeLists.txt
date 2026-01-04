add_library(codexbar-core STATIC)

target_sources(codexbar-core PRIVATE
    Provider.cpp
    ProviderRegistry.cpp
    CodexProvider.cpp
    ClaudeProvider.cpp
    GeminiProvider.cpp
    PtySession.cpp
)

target_link_libraries(codexbar-core PUBLIC
    Qt6::Core
    Qt6::Network
    util
)

target_include_directories(codexbar-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
