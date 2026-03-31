#include "ClaudeProvider.h"
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

ClaudeProvider::ClaudeProvider(QObject *parent)
    : Provider(ProviderID::Claude, parent)
    , m_session(nullptr)
    , m_fetching(false)
    , m_statusSent(false)
    , m_arrowsSent(0)
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
    m_statusSent = false;
    m_arrowsSent = 0;
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
    if (!m_session) return;
    m_buffer.append(QString::fromUtf8(data));

    // Strip ANSI for detection purposes
    static QRegularExpression ansiRx(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    QString stripped = m_buffer;
    stripped.remove(ansiRx);

    // Step 1: Send /status when prompt appears
    if (!m_statusSent && (stripped.contains("Ready to code") || stripped.contains(QChar(0x276F)))) {
        m_session->write("/status\r");
        m_statusSent = true;
        return;
    }

    // Step 2: First right arrow when tab bar appears (Status → Config)
    if (m_statusSent && m_arrowsSent == 0 && stripped.contains("Config") && stripped.contains("Usage")) {
        m_session->write("\x1b[C");
        m_arrowsSent = 1;
        return;
    }

    // Step 3: Second right arrow when Config tab content appears (Config → Usage)
    if (m_arrowsSent == 1 && stripped.contains("Auto-compact")) {
        m_session->write("\x1b[C");
        m_arrowsSent = 2;
        return;
    }

    // Step 4: Parse usage data when available
    if (m_arrowsSent == 2) {
        parseOutput(m_buffer);
    }
}

void ClaudeProvider::onProcessExited(int exitCode) {
    Q_UNUSED(exitCode);
    cleanup();
    m_fetching = false;
}

void ClaudeProvider::cleanup() {
    if (m_session) {
        PtySession *s = m_session;
        m_session = nullptr; // Prevent re-entrant cleanup via processExited signal
        s->close();
        s->deleteLater();
    }
}

void ClaudeProvider::parseOutput(const QString &output) {
    // Remove ANSI escape codes
    static QRegularExpression ansiRegex(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    QString clean = output;
    clean.remove(ansiRegex);

    // Match usage entries: label followed by N% used
    static QRegularExpression usageRegex(
        R"((Current session|Current week \(all models\)|Current week \(Sonnet only\)|Extra usage)[^%]*?(\d{1,3})\s*%\s*used)",
        QRegularExpression::CaseInsensitiveOption
    );

    UsageSnapshot snap;
    snap.timestamp = QDateTime::currentDateTime();
    bool found = false;

    auto it = usageRegex.globalMatch(clean);
    while (it.hasNext()) {
        auto match = it.next();
        QString rawLabel = match.captured(1);
        double val = match.captured(2).toDouble();

        UsageLimit limit;
        if (rawLabel.startsWith("Current session", Qt::CaseInsensitive))
            limit.label = "Session";
        else if (rawLabel.contains("all models", Qt::CaseInsensitive))
            limit.label = "Weekly (all)";
        else if (rawLabel.contains("Sonnet only", Qt::CaseInsensitive))
            limit.label = "Weekly (Sonnet)";
        else if (rawLabel.startsWith("Extra", Qt::CaseInsensitive))
            limit.label = "Extra";

        limit.used = val;
        limit.total = 100.0;
        snap.limits.append(limit);
        found = true;
    }

    if (found) {
        setSnapshot(snap);
        setState(ProviderState::Active);
        emit dataChanged();

        // Defer cleanup to next event loop iteration — we're inside a PtySession signal
        QTimer::singleShot(0, this, [this]() {
            cleanup();
            m_fetching = false;
        });
    }
}
