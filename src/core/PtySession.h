#pragma once

#include <string>
#include <vector>
#include <memory>
#include <QObject>
#include <QSocketNotifier>

class PtySession : public QObject {
    Q_OBJECT
public:
    explicit PtySession(QObject *parent = nullptr);
    ~PtySession();

    bool start(const QString &program, const QStringList &arguments);
    void write(const QByteArray &data);
    void close();

    bool isRunning() const;

signals:
    void dataRead(const QByteArray &data);
    void processExited(int exitCode);

private slots:
    void onReadActivated(int socket);

private:
    int m_masterFd;
    int m_pid;
    QSocketNotifier *m_notifier;
};
