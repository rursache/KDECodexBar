#include "ProviderRegistry.h"
#include "CodexProvider.h"
#include "ClaudeProvider.h"

ProviderRegistry::ProviderRegistry(QObject *parent) : QObject(parent) {
    m_providers.append(new CodexProvider(this));
    m_providers.append(new ClaudeProvider(this));
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
