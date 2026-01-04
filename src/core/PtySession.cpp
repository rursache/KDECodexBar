#include "PtySession.h"
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <csignal>
#include <vector>
#include <QDebug>

PtySession::PtySession(QObject *parent)
    : QObject(parent)
    , m_masterFd(-1)
    , m_pid(-1)
    , m_notifier(nullptr)
{
}

PtySession::~PtySession() {
    close();
}

bool PtySession::start(const QString &program, const QStringList &arguments) {
    if (isRunning()) return false;

    int masterFd, slaveFd;
    char slaveName[1024];

    // Create pseudo-terminal
    if (openpty(&masterFd, &slaveFd, slaveName, nullptr, nullptr) == -1) {
        qCritical() << "openpty failed";
        return false;
    }

    m_pid = fork();
    if (m_pid == -1) {
        qCritical() << "fork failed";
        ::close(masterFd);
        ::close(slaveFd);
        return false;
    }

    if (m_pid == 0) {
        // Child process
        ::close(masterFd);

        // Setup session
        setsid();
        if (ioctl(slaveFd, TIOCSCTTY, nullptr) == -1) {
            // non-fatal
        }

        // Duplicate standard file descriptors
        dup2(slaveFd, STDIN_FILENO);
        dup2(slaveFd, STDOUT_FILENO);
        dup2(slaveFd, STDERR_FILENO);
        if (slaveFd > STDERR_FILENO) ::close(slaveFd);

        // Prepare args
        std::vector<char*> args;
        QByteArray programBytes = program.toLocal8Bit();
        args.push_back(programBytes.data());

        // Hold data alive
        std::vector<QByteArray> argStorage;
        for (const auto &arg : arguments) {
            argStorage.push_back(arg.toLocal8Bit());
            args.push_back(argStorage.back().data());
        }
        args.push_back(nullptr);

        // Execute
        execvp(args[0], args.data());
        
        // If execvp returns, it failed
        perror("execvp failed");
        _exit(1);
    }

    // Parent process
    ::close(slaveFd);
    m_masterFd = masterFd;

    // Set non-blocking
    int flags = fcntl(m_masterFd, F_GETFL, 0);
    fcntl(m_masterFd, F_SETFL, flags | O_NONBLOCK);

    // Setup notifier
    m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &PtySession::onReadActivated);

    qDebug() << "PtySession started PID:" << m_pid;
    return true;
}

void PtySession::write(const QByteArray &data) {
    if (m_masterFd != -1) {
        ::write(m_masterFd, data.constData(), data.size());
    }
}

void PtySession::close() {
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }

    if (m_masterFd != -1) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }

    if (m_pid != -1) {
        kill(m_pid, SIGTERM);
        waitpid(m_pid, nullptr, 0); // Reaps zombie
        m_pid = -1;
        emit processExited(0);
    }
}

bool PtySession::isRunning() const {
    return m_pid != -1;
}

void PtySession::onReadActivated(int socket) {
    if (socket != m_masterFd) return;

    char buffer[4096];
    ssize_t bytesRead = ::read(m_masterFd, buffer, sizeof(buffer));

    if (bytesRead > 0) {
        emit dataRead(QByteArray(buffer, bytesRead));
    } else if (bytesRead < 0 && errno != EAGAIN) {
        // Error or EOF
        close();
    } else if (bytesRead == 0) {
        // EOF
        close();
    }
}
