#pragma once

#include "Provider.h"
#include <QProcess>
#include <QVariant>
#include <QJsonObject>

class CodexProvider : public Provider {
    Q_OBJECT
public:
    explicit CodexProvider(QObject *parent = nullptr);
    ~CodexProvider() override;

    void refresh() override;

private slots:
    void onProcessStarted();
    void onReadyReadStandardOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void sendRequest(const QString &method, const QJsonObject &params = QJsonObject());
    void sendNotification(const QString &method, const QJsonObject &params = QJsonObject());
    void sendPayload(const QJsonObject &payload);
    void handleMessage(const QJsonObject &message);
    void handleRpcResult(int id, const QJsonValue &result);

    QProcess *m_process = nullptr;
    int m_nextId = 1;
    QByteArray m_buffer;
    
    // State tracking for the simple sequential flow
    enum class State {
        Idle,
        Starting,
        Initializing,
        FetchingLimits,
        Finished
    };
    State m_internalState = State::Idle;
    
    int m_initializeId = -1;
    int m_fetchLimitsId = -1;
};
