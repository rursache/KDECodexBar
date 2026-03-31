#include "ClaudeProvider.h"
#include <QDebug>
#include <QRegularExpression>
#include <QDir>
#include <QStandardPaths>

ClaudeProvider::ClaudeProvider(QObject *parent)
    : Provider(ProviderID::Claude, parent)
    , m_session(nullptr)
    , m_fetching(false)
    , m_statusSent(false)
    , m_arrowsSent(0)
{
    m_debounce.setSingleShot(true);
    connect(&m_debounce, &QTimer::timeout, this, &ClaudeProvider::sendStatus);
}

void ClaudeProvider::refresh() {
    if (m_fetching) return;

    // Find claude binary — desktop launches may have a limited PATH
    QString claudePath = QStandardPaths::findExecutable("claude");
    if (claudePath.isEmpty()) {
        const QStringList extraPaths = {
            QDir::homePath() + "/.local/bin",
            QDir::homePath() + "/.npm/bin",
            "/opt/claude-code/bin",
            "/usr/local/bin",
        };
        claudePath = QStandardPaths::findExecutable("claude", extraPaths);
    }
    if (claudePath.isEmpty()) {
        setState(ProviderState::Error);
        emit dataChanged();
        return;
    }

    m_fetching = true;
    m_statusSent = false;
    m_arrowsSent = 0;
    m_buffer.clear();
    m_debounce.stop();
    cleanup();

    m_session = new PtySession(this);
    connect(m_session, &PtySession::dataRead, this, &ClaudeProvider::onPtyData);
    connect(m_session, &PtySession::processExited, this, &ClaudeProvider::onProcessExited);

    if (!m_session->start(claudePath, {})) {
        setState(ProviderState::Error);
        cleanup();
        m_fetching = false;
        emit dataChanged();
    }
}

void ClaudeProvider::onPtyData(const QByteArray &data) {
    if (!m_session) return;
    m_buffer.append(QString::fromUtf8(data));

    // Before /status is sent, debounce — wait for output to settle
    // so we don't send commands into the welcome animation
    if (!m_statusSent) {
        m_debounce.start(1500);
        return;
    }

    // Strip ANSI for detection purposes
    static QRegularExpression ansiRx(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    QString stripped = m_buffer;
    stripped.remove(ansiRx);

    // First right arrow when tab bar appears (Status → Config)
    if (m_arrowsSent == 0 && stripped.contains("Config") && stripped.contains("Usage")) {
        m_session->write("\x1b[C");
        m_arrowsSent = 1;
        return;
    }

    // Second right arrow when Config tab content appears (Config → Usage)
    if (m_arrowsSent == 1 && stripped.contains("Auto-compact")) {
        m_session->write("\x1b[C");
        m_arrowsSent = 2;
        return;
    }

    // Parse usage data when available
    if (m_arrowsSent == 2) {
        parseOutput(m_buffer);
    }
}

void ClaudeProvider::sendStatus() {
    if (!m_session || m_statusSent) return;

    // Strip ANSI to check buffer content
    static QRegularExpression ansiRx(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    QString stripped = m_buffer;
    stripped.remove(ansiRx);

    // Dismiss interactive dialogs that block the prompt
    // Theme picker (first-run) or workspace trust dialog
    if (stripped.contains("Darkmode") && stripped.contains("Lightmode")) {
        m_buffer.clear();
        m_session->write("\r");
        m_debounce.start(3000);
        return;
    }
    if (stripped.contains("Itrustthisfolder") || stripped.contains("trustthisfolder")) {
        m_buffer.clear();
        m_session->write("\r");
        m_debounce.start(3000);
        return;
    }

    m_session->write("/status\r");
    m_statusSent = true;
}

void ClaudeProvider::onProcessExited(int exitCode) {
    Q_UNUSED(exitCode);
    m_debounce.stop();
    cleanup();
    m_fetching = false;
}

void ClaudeProvider::cleanup() {
    if (m_session) {
        PtySession *s = m_session;
        m_session = nullptr;
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
