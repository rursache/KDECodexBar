#include "ClaudeProvider.h"
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>

ClaudeProvider::ClaudeProvider(QObject *parent)
    : Provider(ProviderID::Claude, parent)
    , m_session(nullptr)
    , m_fetching(false)
{
}

void ClaudeProvider::refresh() {
    if (m_fetching) return;

    // Check if claude is installed (simple check)
    QString claudePath = QStandardPaths::findExecutable("claude");
    if (claudePath.isEmpty()) {
        setState(ProviderState::Error);
        emit dataChanged();
        return;
    }

    m_fetching = true;
    m_buffer.clear();
    cleanup();

    m_session = new PtySession(this);
    connect(m_session, &PtySession::dataRead, this, &ClaudeProvider::onPtyData);
    connect(m_session, &PtySession::processExited, this, &ClaudeProvider::onProcessExited);

    if (!m_session->start("claude", {})) {
        setState(ProviderState::Error);
        cleanup();
        m_fetching = false;
        emit dataChanged();
    }
}

void ClaudeProvider::onPtyData(const QByteArray &data) {
    m_buffer.append(QString::fromUtf8(data));
    
    // Auto-send /usage if we see a prompt
    if (!m_buffer.contains("/usage") && (m_buffer.contains("Ready to code") || m_buffer.contains(">"))) {
         m_session->write("/usage\n");
    }

    parseOutput(m_buffer);
}

void ClaudeProvider::onProcessExited(int exitCode) {
    Q_UNUSED(exitCode);
    cleanup();
    m_fetching = false;
}

void ClaudeProvider::cleanup() {
    if (m_session) {
        m_session->close();
        m_session->deleteLater();
        m_session = nullptr;
    }
}

void ClaudeProvider::parseOutput(const QString &output) {
    bool found = false;
    UsageSnapshot snap = snapshot(); // Get current snapshot

    // Remove ANSI codes
    static QRegularExpression ansiRegex(R"(\x1B\[[0-9;]*[a-zA-Z])");
    QString clean = output;
    clean.remove(ansiRegex);

    // Extract Session
    static QRegularExpression sessionRegex(R"(Current session\s+(\d+)%\s+(used|left))", QRegularExpression::CaseInsensitiveOption);
    auto match = sessionRegex.match(clean);
    if (match.hasMatch()) {
        double val = match.captured(1).toDouble();
        QString type = match.captured(2).toLower();
        if (type == "used") {
            snap.session.used = val;
            snap.session.total = 100.0;
        } else {
            snap.session.used = 100.0 - val;
            snap.session.total = 100.0;
        }
        found = true;
    }

    // Extract Weekly
    static QRegularExpression weeklyRegex(R"(Current week\s+\(all models\)\s+(\d+)%\s+(used|left))", QRegularExpression::CaseInsensitiveOption);
    auto weeklyMatch = weeklyRegex.match(clean);
    if (weeklyMatch.hasMatch()) {
        double val = weeklyMatch.captured(1).toDouble();
        QString type = weeklyMatch.captured(2).toLower();
        if (type == "used") {
            snap.weekly.used = val;
            snap.weekly.total = 100.0;
        } else {
            snap.weekly.used = 100.0 - val;
            snap.weekly.total = 100.0;
        }
        found = true;
    }

    if (found) {
        setSnapshot(snap);
        setState(ProviderState::Active);
        emit dataChanged();
    }
}
