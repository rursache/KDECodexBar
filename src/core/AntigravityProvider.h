#pragma once

#include "Provider.h"
#include <QNetworkAccessManager>
#include <QProcess>

class AntigravityProvider : public Provider {
    Q_OBJECT
public:
    explicit AntigravityProvider(QObject *parent = nullptr);
    ~AntigravityProvider() override;

    void refresh() override;

private slots:
    void onUserStatusReply(QNetworkReply *reply);
    void onCommandModelConfigReply(QNetworkReply *reply);

private:
    struct ProcessInfo {
        int pid;
        int extensionPort;
        QString csrfToken;
        QString commandLine;
    };

    struct LimitInfo {
        double usedPct;
        QString resetTime;
    };

    QNetworkAccessManager *m_nam;
    bool m_isFetching;

    // Helper steps
    void detectProcess();
    void findPorts(const ProcessInfo &info);
    void probePorts(const QList<int> &ports, const ProcessInfo &info);
    void fetchUserStatus(int port, const QString &token);
    void fetchCommandModelConfig(int port, const QString &token);

    // Utilities
    static ProcessInfo parseProcessLine(const QString &line);
    static QList<int> parseLsofOutput(const QString &output);
};
