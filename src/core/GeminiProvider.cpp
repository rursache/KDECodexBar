#include "GeminiProvider.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

// Extracted from Gemini CLI bundle
static const QString kClientId = "681255809395-oo8ft2oprdrnp9e3aqf6av3hmdib135j.apps.googleusercontent.com";
static const QString kClientSecret = "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";
static const QString kQuotaEndpoint = "https://cloudcode-pa.googleapis.com/v1internal:retrieveUserQuota";
static const QString kTokenEndpoint = "https://oauth2.googleapis.com/token";

GeminiProvider::GeminiProvider(QObject *parent)
    : Provider(ProviderID::Gemini, parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

GeminiProvider::~GeminiProvider() = default;

void GeminiProvider::refresh() {
    if (!loadCredentials()) {
        setState(ProviderState::Error);
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Refresh if expired or expiring soon (within 5 minutes)
    if (m_creds.expiryDateMs > 0 && now > (m_creds.expiryDateMs - 300000)) {
        refreshAccessToken();
    } else {
        fetchQuota();
    }
}

bool GeminiProvider::loadCredentials() {
    QString home = QDir::homePath();
    QFile file(home + "/.gemini/oauth_creds.json");
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "GeminiProvider: Could not open credentials file.";
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    m_creds.accessToken = root.value("access_token").toString();
    m_creds.refreshToken = root.value("refresh_token").toString();
    m_creds.expiryDateMs = static_cast<qint64>(root.value("expiry_date").toDouble());

    return !m_creds.accessToken.isEmpty();
}

void GeminiProvider::saveCredentials(const QJsonObject &json) {
    QString home = QDir::homePath();
    QFile file(home + "/.gemini/oauth_creds.json");
    if (!file.open(QIODevice::ReadOnly)) {
        return; // Handle error?
    }
    
    // Read existing to preserve other fields like id_token if needed
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();
    
    // Update fields
    if (json.contains("access_token")) root["access_token"] = json["access_token"];
    if (json.contains("expires_in")) {
        double expiresIn = json["expires_in"].toDouble();
        root["expiry_date"] = static_cast<double>(QDateTime::currentMSecsSinceEpoch() + (expiresIn * 1000));
        m_creds.expiryDateMs = static_cast<qint64>(root["expiry_date"].toDouble());
    }
    
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
    }
    
    // Update local cache
    if (json.contains("access_token")) m_creds.accessToken = json["access_token"].toString();
}

void GeminiProvider::refreshAccessToken() {
    if (m_isRefreshing) return;
    m_isRefreshing = true;
    
    QUrl url(kTokenEndpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    QByteArray body;
    body.append("client_id=" + kClientId.toUtf8());
    body.append("&client_secret=" + kClientSecret.toUtf8());
    body.append("&refresh_token=" + m_creds.refreshToken.toUtf8());
    body.append("&grant_type=refresh_token");
    
    QNetworkReply *reply = m_nam->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        onTokenRefreshReply(reply);
    });
}

void GeminiProvider::onTokenRefreshReply(QNetworkReply *reply) {
    m_isRefreshing = false;
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "GeminiProvider: Token refresh failed:" << reply->errorString();
        setState(ProviderState::Error);
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    saveCredentials(doc.object());
    
    // Retry fetch
    fetchQuota();
}

void GeminiProvider::fetchQuota() {
    QUrl url(kQuotaEndpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_creds.accessToken).toUtf8());
    
    // Empty JSON body usually works, but we could add project ID if we wanted to be strict
    QByteArray body = "{}";
    
    QNetworkReply *reply = m_nam->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply](){ onQuotaReply(reply); });
}

void GeminiProvider::onQuotaReply(QNetworkReply *reply) {
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "GeminiProvider: Quota fetch failed:" << reply->errorString();
        // If 401, maybe force refresh next time?
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
             m_creds.expiryDateMs = 0; // Force refresh
        }
        setState(ProviderState::Error);
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();
    QJsonArray buckets = root.value("buckets").toArray();
    
    UsageSnapshot snap;
    snap.limits.clear();
    
    // Map interesting models
    // We want to group by model, keeping the lowest fraction (highest usage)
    // Structure: buckets: [{ modelId: "...", remainingFraction: 0.x, ... }]
    
    struct LimitInfo {
        double usedPct;
        QString resetTime;
    };
    QMap<QString, LimitInfo> modelUsage;
    
    for (const QJsonValue &v : buckets) {
        QJsonObject b = v.toObject();
        QString modelId = b.value("modelId").toString();
        if (modelId.isEmpty() || !b.contains("remainingFraction")) continue;
        
        double fraction = b.value("remainingFraction").toDouble();
        double used = (1.0 - fraction) * 100.0;
        
        // Keep the WORST usage per model (highest used %)
        if (!modelUsage.contains(modelId) || used > modelUsage[modelId].usedPct) {
            modelUsage[modelId] = { used, b.value("resetTime").toString() };
        }
    }
    
    // Filter for targets
    QStringList targets = {"gemini-2.5-flash", "gemini-2.5-pro"};
    
    for (const QString &target : targets) {
        if (modelUsage.contains(target)) {
            UsageLimit limit;
            // Label: "Flash", "Pro" etc
            if (target.contains("flash")) limit.label = "Flash";
            else if (target.contains("pro")) limit.label = "Pro";
            else limit.label = target;
            
            limit.used = modelUsage[target].usedPct;
            limit.total = 100.0;
            
            // Simple reset desc parsing if needed, but for now empty is fine
            limit.resetDescription = ""; 
            
            snap.limits.append(limit);
        }
    }
    
    setSnapshot(snap);
    setState(ProviderState::Active);
}
