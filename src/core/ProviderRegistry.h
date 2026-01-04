#pragma once

#include "Provider.h"
#include <QObject>
#include <QVector>

class ProviderRegistry : public QObject {
  Q_OBJECT
public:
  explicit ProviderRegistry(QObject *parent = nullptr);

  void registerProvider(Provider *provider);
  QVector<Provider *> providers() const;
  Provider *provider(ProviderID id) const;

private:
  QVector<Provider *> m_providers;
};
