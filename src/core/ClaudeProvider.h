#pragma once

#include "Provider.h"
#include "PtySession.h"
#include <QRegularExpression>
#include <QTimer>

class ClaudeProvider : public Provider {
    Q_OBJECT
public:
    explicit ClaudeProvider(QObject *parent = nullptr);

    void refresh() override;

private slots:
    void onPtyData(const QByteArray &data);
    void onProcessExited(int exitCode);

private:
    void sendStatus();
    void parseOutput(const QString &output);
    void cleanup();

    PtySession *m_session;
    QTimer m_debounce;
    QTimer m_timeout;
    QString m_buffer;
    bool m_fetching;
    bool m_statusSent;
    int m_arrowsSent;
};
