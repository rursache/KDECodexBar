#pragma once

#include "Provider.h"
#include "PtySession.h"
#include <QRegularExpression>

class ClaudeProvider : public Provider {
    Q_OBJECT
public:
    explicit ClaudeProvider(QObject *parent = nullptr);

    void refresh() override;

private slots:
    void onPtyData(const QByteArray &data);
    void onProcessExited(int exitCode);

private:
    void parseOutput(const QString &output);
    void cleanup();

    PtySession *m_session;
    QString m_buffer;
    bool m_fetching;
};
