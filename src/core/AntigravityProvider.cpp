#include "AntigravityProvider.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QStandardPaths>

static const QString kProcessName = "language_server_linux_x64"; // From user's ps aux
static const QString kUserStatusPath = "/exa.language_server_pb.LanguageServerService/GetUserStatus";
static const QString kCommandModelPath = "/exa.language_server_pb.LanguageServerService/GetCommandModelConfigs";
static const QString kUnleashPath = "/exa.language_server_pb.LanguageServerService/GetUnleashData";

AntigravityProvider::AntigravityProvider(QObject *parent)
    : Provider(ProviderID::Antigravity, parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_isFetching(false)
{
}

AntigravityProvider::~AntigravityProvider() = default;

void AntigravityProvider::refresh() {
    if (m_isFetching) return;
    m_isFetching = true;
    detectProcess();
}

void AntigravityProvider::detectProcess() {
    QProcess *process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            setState(ProviderState::Error);
            m_isFetching = false;
            return;
        }

        QString output = QString::fromUtf8(process->readAllStandardOutput());
        QStringList lines = output.split('\n');
        
        ProcessInfo foundInfo = {0, 0, "", ""};
        bool found = false;

        for (const QString &line : lines) {
            // Check for correct process
            // line format: "PID COMMAND_ARGS..." due to ps -ax -o pid=,command=
            // But we can just fuzzy match for safety
            if (!line.contains(kProcessName)) continue;
            
            // Should contain --app_data_dir and antigravity (common check)
            if (!line.contains("--app_data_dir") || !line.contains("antigravity")) continue;

            foundInfo = parseProcessLine(line);
            if (!foundInfo.csrfToken.isEmpty()) {
                found = true;
                break;
            }
        }

        if (found) {
            findPorts(foundInfo);
        } else {
            qDebug() << "AntigravityProvider: Process not found";
            setState(ProviderState::Error); 
            m_isFetching = false;
        }
    });

    process->start("ps", {"-ax", "-o", "pid=,command="});
}

AntigravityProvider::ProcessInfo AntigravityProvider::parseProcessLine(const QString &line) {
    ProcessInfo info = {0, 0, "", ""};
    
    // Extract PID
    QString trimmed = line.trimmed();
    int spaceIdx = trimmed.indexOf(' ');
    if (spaceIdx == -1) return info;
    
    info.pid = trimmed.left(spaceIdx).toInt();
    info.commandLine = trimmed.mid(spaceIdx + 1);

    // Extract CSRF Token
    // --csrf_token <token>
    static QRegularExpression csrfRegex(R"(--csrf_token[=\s]+([^\s]+))");
    auto match = csrfRegex.match(info.commandLine);
    if (match.hasMatch()) {
        info.csrfToken = match.captured(1);
    }

    // Extract Extension Port
    static QRegularExpression portRegex(R"(--extension_server_port[=\s]+(\d+))");
    auto portMatch = portRegex.match(info.commandLine);
    if (portMatch.hasMatch()) {
        info.extensionPort = portMatch.captured(1).toInt();
    }

    return info;
}

void AntigravityProvider::findPorts(const ProcessInfo &info) {
    // Determine lsof binary
    QString lsofMap = QStandardPaths::findExecutable("lsof");
    if (lsofMap.isEmpty()) {
        // Fallback or error?
        // On many minimal linuxes lsof implies root or isn't there.
        // We could try parsing /proc/net/tcp but mapping inodes to PIDs is hard without root.
        // For now, assume user usually has lsof or we can try just extensionPort if available?
        // The mac implementation falls back to extensionPort for HTTP.
        // Let's force try extensionPort if lsof misses.
        if (info.extensionPort > 0) {
           QList<int> ports; 
           ports << info.extensionPort;
           probePorts(ports, info);
           return;
        }
        
        qDebug() << "AntigravityProvider: lsof not found and no extension port known";
        setState(ProviderState::Error);
        m_isFetching = false;
        return;
    }

    QProcess *lsof = new QProcess(this);
    connect(lsof, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, lsof, info](int exitCode, QProcess::ExitStatus status) {
        lsof->deleteLater();
        // lsof returns 1 if no files found, which is possible.
        
        QString output = QString::fromUtf8(lsof->readAllStandardOutput());
        QList<int> ports = parseLsofOutput(output);
        
        if (ports.isEmpty()) {
             // Fallback to extraction port if we have one
             if (info.extensionPort > 0) {
                 ports << info.extensionPort;
             } else {
                 setState(ProviderState::Error);
                 m_isFetching = false;
                 return;
             }
        }
        
        probePorts(ports, info);
    });

    // lsof -nP -iTCP -sTCP:LISTEN -a -p <pid>
    lsof->start(lsofMap, {"-nP", "-iTCP", "-sTCP:LISTEN", "-a", "-p", QString::number(info.pid)});
}

QList<int> AntigravityProvider::parseLsofOutput(const QString &output) {
    QList<int> ports;
    QStringList lines = output.split('\n');
    // Example: process 123 user ... TCP 127.0.0.1:36559 (LISTEN)
    // Regex looking for :(\d+)
    static QRegularExpression portRegex(R"(:(\d+)\s+\(LISTEN\))");
    
    for (const QString &line : lines) {
        auto match = portRegex.match(line);
        if (match.hasMatch()) {
            bool ok;
            int p = match.captured(1).toInt(&ok);
            if (ok && !ports.contains(p)) {
                ports.append(p);
            }
        }
    }
    std::sort(ports.begin(), ports.end());
    return ports;
}

// Simple probing: Try the first port. In a real scenario, we might iterate.
// For expediency, if we have multiple, we'll try the first one that replies to Unleash, but
// simplistic async logic is: Try one, if fail try next.
// Here we'll just try to fetch UserStatus from the first one that looks like an API port.
// Or just try specific ones.
// The macOS probe tries them all for "GetUnleashData".
void AntigravityProvider::probePorts(const QList<int> &ports, const ProcessInfo &info) {
    // For MVP, if we have a port from lsof that matches extensionPort, prefer that?
    // Actually the logic implies finding the *HTTPS* port which is random.
    // extensionPort is HTTP.
    
    // We initiate a fetch on the first candidate. If it fails, we could chain.
    // Let's implement a simple recursion or just try the first non-extension port?
    
    int targetPort = 0;
    if (!ports.isEmpty()) targetPort = ports.first();
    
    // If we have an extension port, and we didn't find others, use it (HTTP).
    // Ideally we iterate. Let's pick the last one (often newest?) or just the first.
    
    qDebug() << "AntigravityProvider: Fetching status from port" << targetPort;
    fetchUserStatus(targetPort, info.csrfToken);
}

void AntigravityProvider::fetchUserStatus(int port, const QString &token) {
    QUrl url;
    url.setScheme("https"); // Try HTTPS first (local server self-signed)
    url.setHost("127.0.0.1");
    url.setPort(port);
    url.setPath(kUserStatusPath);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("X-Codeium-Csrf-Token", token.toUtf8());
    request.setRawHeader("Connect-Protocol-Version", "1");
    
    // Allow self-signed
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
    
    // Body from docs
    QJsonObject meta;
    meta["ideName"] = "antigravity";
    meta["extensionName"] = "antigravity";
    meta["ideVersion"] = "unknown";
    meta["locale"] = "en";
    
    QJsonObject body;
    body["metadata"] = meta;
    
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onUserStatusReply(reply);
    });
}

void AntigravityProvider::onUserStatusReply(QNetworkReply *reply) {
    m_isFetching = false; 
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "AntigravityProvider: API Error" << reply->errorString();
        // Here we *could* retry with HTTP or another port
        setState(ProviderState::Error);
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QJsonObject userStatus = root.value("userStatus").toObject();
    
    // Parse mapping
    QJsonArray clientConfigs = userStatus.value("cascadeModelConfigData").toObject()
                                         .value("clientModelConfigs").toArray();
                                         
    UsageSnapshot snap;
    snap.limits.clear();
    
    // Simple mapping strategy from docs
    // 1. Claude
    // 2. Gemini Pro Low
    // 3. Gemini Flash
    
    struct RawQuota {
        QString label;
        double remaining;
        QString resetTime;
    };
    QList<RawQuota> found;
    
    for (const auto &v : clientConfigs) {
        QJsonObject c = v.toObject();
        QString label = c.value("label").toString();
        QJsonObject q = c.value("quotaInfo").toObject();
        if (q.isEmpty() || !q.contains("remainingFraction")) continue;
        
        found.append({
            label,
            q.value("remainingFraction").toDouble(),
            q.value("resetTime").toString()
        });
    }
    
    // Select specific models
    auto findModel = [&](const QString &pattern, const QString &exclude = "") -> int {
        for (int i=0; i<found.size(); ++i) {
            QString l = found[i].label.toLower();
            if (l.contains(pattern)) {
                if (!exclude.isEmpty() && l.contains(exclude)) continue;
                return i;
            }
        }
        return -1;
    };
    
    auto addLimit = [&](int idx, const QString &displayName) {
        if (idx >= 0) {
            UsageLimit limit;
            limit.label = displayName;
            limit.total = 100.0;
            limit.used = (1.0 - found[idx].remaining) * 100.0;
            limit.resetDescription = ""; // Need ISO parser if we want detailed "3h" text
            snap.limits.append(limit);
        }
    };
    
    addLimit(findModel("claude", "thinking"), "Claude");
    addLimit(findModel("pro", "low"), "Pro"); // "Gemini Pro Low" -> "Pro"
    addLimit(findModel("flash"), "Flash");
    
    setSnapshot(snap);
    setState(ProviderState::Active);
}

void AntigravityProvider::onCommandModelConfigReply(QNetworkReply *reply) {
    // Fallback path if needed
    reply->deleteLater();
}
