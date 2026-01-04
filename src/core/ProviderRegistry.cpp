#include "ProviderRegistry.h"
#include "CodexProvider.h"
#include "ClaudeProvider.h"
#include "GeminiProvider.h"

ProviderRegistry::ProviderRegistry(QObject *parent) : QObject(parent)
{
    // Register known providers
    // 1. Codex
    registerProvider(new CodexProvider(this));
    
    // 2. Claude
    registerProvider(new ClaudeProvider(this));
    
    // 3. Gemini
    registerProvider(new GeminiProvider(this));
}

void ProviderRegistry::registerProvider(Provider *provider) {
    if (provider) {
        provider->setParent(this);
        m_providers.append(provider);
    }
}

QVector<Provider *> ProviderRegistry::providers() const { return m_providers; }

Provider *ProviderRegistry::provider(ProviderID id) const {
    for (auto *p : m_providers) {
        if (p->id() == id)
            return p;
    }
    return nullptr;
}
