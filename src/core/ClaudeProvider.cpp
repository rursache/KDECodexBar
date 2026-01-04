#include "ClaudeProvider.h"
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>

ClaudeProvider::ClaudeProvider(QObject *parent)
    : Provider(ProviderID::Claude, parent)
    , m_session(nullptr)
    , m_fetching(false)
    , m_confirmationSent(false)
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
    m_confirmationSent = false;
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
    // Use \r because interactive CLI might expect CR
    if (!m_buffer.contains("/usage") && (m_buffer.contains("Ready to code") || m_buffer.contains(">"))) {
         m_session->write("/usage\r");
    }

    // If we see the autocomplete menu but no result yet, press Enter again
    if (!m_confirmationSent && m_buffer.contains("Show plan usage limits") && !m_buffer.contains("Current session")) {
        m_session->write("\r");
        m_confirmationSent = true;
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

// Helper to extract percent from a specific block
double extractPercent(const QStringList &lines, const QString &label) {
    // Find label
    int startIdx = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].contains(label, Qt::CaseInsensitive)) {
            startIdx = i;
            break;
        }
    }
    if (startIdx == -1) return -1.0;

    // Scan next 15 lines
    static QRegularExpression pctRegex(R"((\d{1,3})\s*%\s*(used|left))", QRegularExpression::CaseInsensitiveOption);
    
    for (int i = startIdx; i < qMin(startIdx + 15, lines.size()); ++i) {
        auto match = pctRegex.match(lines[i]);
        if (match.hasMatch()) {
            double val = match.captured(1).toDouble();
            QString type = match.captured(2).toLower();
            if (type == "used") {
                return val;
            } else {
                return 100.0 - val;
            }
        }
    }
    return -1.0;
}

void ClaudeProvider::parseOutput(const QString &output) {
    UsageSnapshot snap = snapshot(); // Get current snapshot

    // Remove ANSI codes
    static QRegularExpression ansiRegex(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    QString clean = output;
    clean.remove(ansiRegex);
    
    // qDebug().noquote() << clean; // Uncomment for debugging
    
    QStringList lines = clean.split('\n');

    bool found = false;

    // Extract Session
    double sessionVal = extractPercent(lines, "Current session");
    if (sessionVal >= 0) {
        snap.session.used = sessionVal;
        snap.session.total = 100.0;
        found = true;
    }

    // Extract Weekly
    double weeklyVal = extractPercent(lines, "Current week");
    if (weeklyVal >= 0) {
        snap.weekly.used = weeklyVal;
        snap.weekly.total = 100.0;
        found = true;
    }

    if (found) {
        setSnapshot(snap);
        setState(ProviderState::Active);
        emit dataChanged();
    }
}
