#include "CodexProvider.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>

CodexProvider::CodexProvider(QObject *parent)
    : Provider(ProviderID::Codex, parent)
{
}

CodexProvider::~CodexProvider()
{
    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }
}

void CodexProvider::refresh()
{
    if (m_internalState != State::Idle && m_internalState != State::Finished) {
        // Already running
        return;
    }

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, &CodexProvider::onProcessStarted);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &CodexProvider::onReadyReadStandardOutput);
    connect(m_process, &QProcess::finished, this, &CodexProvider::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &CodexProvider::onProcessError);

    m_internalState = State::Starting;
    setState(ProviderState::Active); 
    // We could set a Loading state if we had one, but Active is fine.
    
    // Command from macOS: codex -s read-only -a untrusted app-server
    QString program = "codex";
    QStringList arguments;
    arguments << "-s" << "read-only" << "-a" << "untrusted" << "app-server";
    
    // TODO: Ideally resolve 'codex' path similar to macOS BinaryLocator
    // For now rely on PATH.
    
    m_process->start(program, arguments);
}

void CodexProvider::onProcessStarted()
{
    m_internalState = State::Initializing;
    
    // Send initialize
    // params: ["clientInfo": ["name": clientName, "version": clientVersion]]    qDebug() << "CodexProvider: Process started, sending initialize";
    
    QJsonObject clientInfo;
    clientInfo["name"] = "codexbar-linux";
    clientInfo["version"] = "2.0.0";
    
    QJsonObject params;
    params["clientInfo"] = clientInfo;
    
    m_initializeId = m_nextId++;
    QJsonObject request;
    request["id"] = m_initializeId;
    request["method"] = "initialize";
    request["params"] = params;
    
    sendPayload(request);
}

void CodexProvider::onReadyReadStandardOutput()
{
    if (!m_process) return;
    m_buffer.append(m_process->readAllStandardOutput());
    
    while (true) {
        int newlineIndex = m_buffer.indexOf('\n');
        if (newlineIndex == -1) break;
        
        QByteArray line = m_buffer.left(newlineIndex);
        m_buffer.remove(0, newlineIndex + 1);
        
        if (line.trimmed().isEmpty()) continue;
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            handleMessage(doc.object());
        } else {
            qWarning() << "CodexProvider: Failed to parse JSON:" << line;
        }
    }
}

void CodexProvider::handleMessage(const QJsonObject &message)
{
    if (message.contains("id")) {
        int id = message["id"].toInt();
        if (message.contains("result")) {
            handleRpcResult(id, message["result"]);
        } else if (message.contains("error")) {
            QJsonObject err = message["error"].toObject();
            qWarning() << "CodexProvider: RPC Error:" << err["message"].toString();
            // TODO: Handle error state
        }
    } else {
        // Notification or other message
        // macOS ignores notifications usually: if message["id"] == nil ...
    }
}

void CodexProvider::handleRpcResult(int id, const QJsonValue &result)
{
    if (id == m_initializeId) {
        qDebug() << "CodexProvider: Initialized, fetching limits";
        // Initialized. Now send "initialized" notification and then fetch limits.
        sendNotification("initialized");
        
        m_fetchLimitsId = m_nextId++;
        QJsonObject request;
        request["id"] = m_fetchLimitsId;
        request["method"] = "account/rateLimits/read";
        
        sendPayload(request);
        m_internalState = State::FetchingLimits;
        
    } else if (id == m_fetchLimitsId) {
        // Parse rate limits
        // Response structure: { rateLimits: { primary: {...}, secondary: {...}, credits: {...} } }
        UsageSnapshot snapshot;
        snapshot.timestamp = QDateTime::currentDateTime();
        
        QJsonObject root = result.toObject(); // "result" value passed in
        QJsonObject rateLimits = root["rateLimits"].toObject();
        
        // Helper to parse window
        auto parseWindow = [](const QString &label, const QJsonObject &win) -> UsageLimit {
            UsageLimit limit;
            limit.label = label;
            if (win.isEmpty()) return limit;
            
            double usedPercent = win["usedPercent"].toDouble();
            limit.used = usedPercent;
            limit.total = 100.0; 
            limit.unit = "%";
            limit.resetDescription = win["resetDescription"].toString();
            return limit;
        };
        
        snapshot.limits.append(parseWindow("Session", rateLimits["primary"].toObject()));
        snapshot.limits.append(parseWindow("Weekly", rateLimits["secondary"].toObject()));
        
        qDebug() << "CodexProvider: Fetched limits. Count:" << snapshot.limits.size();

        setSnapshot(snapshot);
        setState(ProviderState::Active);
        
        m_internalState = State::Finished;
        
        // Done for this refresh cycle
        m_process->terminate();
    }
}

void CodexProvider::sendPayload(const QJsonObject &payload)
{
    if (!m_process) return;
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    m_process->write(data);
    m_process->write("\n");
}

void CodexProvider::sendNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject p;
    p["method"] = method;
    if (!params.isEmpty()) {
        p["params"] = params;
    }
    sendPayload(p);
}

void CodexProvider::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    // If we already finished logically (e.g. successful fetch), ignore the exit status of the killed process
    if (m_internalState == State::Finished) return;

    m_internalState = State::Finished;
    if (exitStatus == QProcess::CrashExit) {
         qWarning() << "CodexProvider: Process crashed";
         setState(ProviderState::Error);
    }
    // else normal exit
}

void CodexProvider::onProcessError(QProcess::ProcessError error)
{
    // If we're done, ignore errors (like Crashed due to terminate())
    if (m_internalState == State::Finished) return;

    qWarning() << "CodexProvider: Process error" << error;
    setState(ProviderState::Error);
    m_internalState = State::Idle;
}
