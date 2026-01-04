add_library(kdecodexbar-core STATIC)

target_sources(kdecodexbar-core PRIVATE
    Provider.cpp
    ProviderRegistry.cpp
    CodexProvider.cpp
    ClaudeProvider.cpp
    GeminiProvider.cpp
    AntigravityProvider.cpp
    PtySession.cpp
)

target_link_libraries(kdecodexbar-core PUBLIC
    Qt6::Core
    Qt6::Network
    util
)

target_include_directories(kdecodexbar-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
