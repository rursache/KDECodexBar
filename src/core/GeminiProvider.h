#pragma once

#include "Provider.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QDateTime>
#include <QJsonObject>

class GeminiProvider : public Provider {
    Q_OBJECT
public:
    explicit GeminiProvider(QObject *parent = nullptr);
    ~GeminiProvider() override;

    void refresh() override;

private slots:
    void onQuotaReply(QNetworkReply *reply);
    void onTokenRefreshReply(QNetworkReply *reply);

private:
    struct OAuthCredentials {
        QString accessToken;
        QString refreshToken;
        qint64 expiryDateMs = 0; // Epoch ms
    };

    void fetchQuota();
    bool loadCredentials();
    void refreshAccessToken();
    void saveCredentials(const QJsonObject &json);

    QNetworkAccessManager *m_nam;
    OAuthCredentials m_creds;
    bool m_isRefreshing = false;
};
