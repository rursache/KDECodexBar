#include "Provider.h"

Provider::Provider(ProviderID id, QObject *parent)
    : QObject(parent), m_id(id), m_state(ProviderState::Active) {}

ProviderID Provider::id() const { return m_id; }

QString Provider::name() const {
  switch (m_id) {
  case ProviderID::Codex:
    return "Codex";
  case ProviderID::Claude:
    return "Claude";
  case ProviderID::Gemini:
    return "Gemini";
  case ProviderID::Antigravity:
    return "Antigravity";
  default:
    return "Unknown";
  }
}

ProviderState Provider::state() const { return m_state; }

UsageSnapshot Provider::snapshot() const { return m_snapshot; }

void Provider::setSnapshot(const UsageSnapshot &snapshot) {
  m_snapshot = snapshot;
  emit dataChanged();
}

void Provider::setState(ProviderState state) {
  if (m_state != state) {
    m_state = state;
    emit stateChanged(state);
  }
}
