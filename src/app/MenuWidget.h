#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include "Provider.h"

class MenuWidget : public QWidget {
    Q_OBJECT
public:
    explicit MenuWidget(QWidget *parent = nullptr);
    void updateData(const QString &providerName, const UsageSnapshot &snapshot);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QWidget* createSection(const QString &title, QLabel* &usedLabel, QLabel* &resetLabel, QProgressBar* &bar);

    QLabel *m_providerLabel;
    QLabel *m_statusLabel;
    
    QLabel *m_sessionUsed;
    QLabel *m_sessionReset;
    QProgressBar *m_sessionBar;

    QLabel *m_weeklyUsed;
    QLabel *m_weeklyReset;
    QProgressBar *m_weeklyBar;

    // TODO: Add Model section if needed for phase 4
};
