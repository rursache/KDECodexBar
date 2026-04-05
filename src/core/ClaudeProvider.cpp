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

    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this]() {
        qDebug() << "ClaudeProvider: Session timed out, forcing cleanup";
        cleanup();
        m_fetching = false;
    });
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
    m_timeout.stop();
    cleanup();
    m_timeout.start(30000);

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
    m_timeout.stop();
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

    // Match usage entries: label followed by N% used, optionally followed by reset info
    // After ANSI stripping, text may lose spaces, so "Resets" can appear as "Rese(t|s)s" etc.
    static QRegularExpression usageRegex(
        R"((Current\s*session|Current\s*week\s*\(all\s*models\)|Current\s*week\s*\(Sonnet\s*only\)|Extra\s*usage)[^%]*?(\d{1,3})\s*%\s*used\s*(Rese[^\(]*\([^\)]+\))?)",
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
        QString rawReset = match.captured(3).trimmed();

        UsageLimit limit;
        if (rawLabel.startsWith("Current session", Qt::CaseInsensitive) ||
            rawLabel.startsWith("Currentsession", Qt::CaseInsensitive))
            limit.label = "Session";
        else if (rawLabel.contains("all models", Qt::CaseInsensitive) ||
                 rawLabel.contains("allmodels", Qt::CaseInsensitive))
            limit.label = "Weekly (all)";
        else if (rawLabel.contains("Sonnet only", Qt::CaseInsensitive) ||
                 rawLabel.contains("Sonnetonly", Qt::CaseInsensitive))
            limit.label = "Weekly (Sonnet)";
        else if (rawLabel.startsWith("Extra", Qt::CaseInsensitive))
            limit.label = "Extra";

        limit.used = val;
        limit.total = 100.0;
        if (!rawReset.isEmpty()) {
            // Remove timezone like "(Europe/Bucharest)" and normalize prefix
            rawReset.replace(QRegularExpression(R"(\s*\([A-Za-z]+/[A-Za-z_]+\))"), "");
            rawReset.replace(QRegularExpression(R"(^Rese\w*s\s*)", QRegularExpression::CaseInsensitiveOption), "");
            QString timeStr = rawReset.simplified();

            // Parse absolute time from Claude CLI into relative "Resets in Xh Ym"
            // Formats: "5pm", "9:59am", "Apr 10, 9:59am", "Apr 10, 5pm"
            QDateTime resetDt;
            static QRegularExpression dateTimeRx(R"(^([A-Za-z]+)\s+(\d{1,2}),?\s+(.+)$)");
            auto dtMatch = dateTimeRx.match(timeStr);
            if (dtMatch.hasMatch()) {
                // Has date component: "Apr 10, 9:59am"
                QString monthDay = dtMatch.captured(1) + " " + dtMatch.captured(2);
                QString timePart = dtMatch.captured(3).trimmed();
                // Try with minutes then without
                resetDt = QDateTime::fromString(
                    QString("%1 %2 %3").arg(QDate::currentDate().year()).arg(monthDay).arg(timePart),
                    "yyyy MMM d h:mmap");
                if (!resetDt.isValid())
                    resetDt = QDateTime::fromString(
                        QString("%1 %2 %3").arg(QDate::currentDate().year()).arg(monthDay).arg(timePart),
                        "yyyy MMM d hap");
            } else {
                // Time only: "5pm", "9:59am" — assume today
                QTime t = QTime::fromString(timeStr, "h:mmap");
                if (!t.isValid()) t = QTime::fromString(timeStr, "hap");
                if (t.isValid()) resetDt = QDateTime(QDate::currentDate(), t);
            }

            if (resetDt.isValid()) {
                qint64 secsLeft = QDateTime::currentDateTime().secsTo(resetDt);
                if (secsLeft > 0) {
                    int days = secsLeft / 86400;
                    int hours = (secsLeft % 86400) / 3600;
                    int mins = (secsLeft % 3600) / 60;
                    if (days > 0)
                        limit.resetDescription = QString("Resets in %1d %2h").arg(days).arg(hours);
                    else if (hours > 0)
                        limit.resetDescription = QString("Resets in %1h %2m").arg(hours).arg(mins);
                    else
                        limit.resetDescription = QString("Resets in %1m").arg(mins);
                }
            }
        }
        snap.limits.append(limit);
        found = true;
    }

    if (found) {
        setSnapshot(snap);
        setState(ProviderState::Active);
        emit dataChanged();

        // Defer cleanup to next event loop iteration — we're inside a PtySession signal
        QTimer::singleShot(0, this, [this]() {
            m_timeout.stop();
            cleanup();
            m_fetching = false;
        });
    }
}
