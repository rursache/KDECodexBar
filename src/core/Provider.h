#pragma once

#include <QString>
#include <QDateTime>
#include <QMap>
#include <QObject>

enum class ProviderID {
    Codex,
    Claude,
    Gemini,
    Antigravity,
    Unknown
};

enum class ProviderState {
    Active,
    Error,
    Stale
};

struct UsageLimit {
    double used = 0.0;
    double total = 0.0;
    QString unit; // "tokens", "requests", etc.
    QString resetDescription; // e.g. "Resets in 3h 53m"
    
    // Helper to get percentage
    double percent() const {
        if (total <= 0) return 0.0;
        return (used / total) * 100.0;
    }
};

struct UsageSnapshot {
    UsageLimit session;
    UsageLimit weekly;
    QDateTime resetTime;
    QDateTime timestamp;
};

class Provider : public QObject {
    Q_OBJECT
public:
    explicit Provider(ProviderID id, QObject *parent = nullptr);
    virtual ~Provider() = default;

    ProviderID id() const;
    QString name() const;
    
    ProviderState state() const;
    UsageSnapshot snapshot() const;
    
    // Abstract method to be implemented by specific strategies
    virtual void refresh() = 0;

signals:
    void dataChanged();
    void stateChanged(ProviderState newState);

protected:
    void setSnapshot(const UsageSnapshot &snapshot);
    void setState(ProviderState state);

private:
    ProviderID m_id;
    ProviderState m_state;
    UsageSnapshot m_snapshot;
};
