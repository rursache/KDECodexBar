#include "MenuWidget.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include <QDateTime>

MenuWidget::MenuWidget(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 12);
    mainLayout->setSpacing(12);

    // Header: Provider Name + Updated Time/Status
    auto *headerLayout = new QHBoxLayout();
    m_providerLabel = new QLabel("Provider", this);
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(11); // Slightly larger
    m_providerLabel->setFont(titleFont);

    m_statusLabel = new QLabel("Updated just now", this);
    m_statusLabel->setStyleSheet("color: #888888; font-size: 11px;");

    headerLayout->addWidget(m_providerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(headerLayout);

    // Separator
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("color: #333333;"); // Subtle divider
    mainLayout->addWidget(line);

    // Sections
    mainLayout->addWidget(createSection("Session", m_sessionUsed, m_sessionReset, m_sessionBar));
    mainLayout->addWidget(createSection("Weekly", m_weeklyUsed, m_weeklyReset, m_weeklyBar));
    
    // Ensure visibility
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

QSize MenuWidget::sizeHint() const {
    return QSize(280, 200); // Reasonable default
}

QSize MenuWidget::minimumSizeHint() const {
    return QSize(260, 150);
}

QWidget* MenuWidget::createSection(const QString &title, QLabel* &usedLabel, QLabel* &resetLabel, QProgressBar* &bar) {
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *titleLabel = new QLabel(title, this);
    QFont f = font();
    f.setBold(true);
    titleLabel->setFont(f);
    layout->addWidget(titleLabel);

    bar = new QProgressBar(this);
    bar->setTextVisible(false);
    bar->setFixedHeight(6);
    // Mimic the macos look (rounded, beige/orange color)
    bar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            background-color: #3a3a3a; /* Dark track */
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background-color: #cc9977; /* Beige/Orange like screenshot */
            border-radius: 3px;
        }
    )");
    layout->addWidget(bar);

    auto *infoLayout = new QHBoxLayout();
    usedLabel = new QLabel("0% used", this);
    usedLabel->setStyleSheet("color: #cccccc;");
    
    resetLabel = new QLabel("", this);
    resetLabel->setStyleSheet("color: #888888;");

    infoLayout->addWidget(usedLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(resetLabel);
    layout->addLayout(infoLayout);

    return container;
}

void MenuWidget::updateData(const QString &providerName, const UsageSnapshot &snapshot) {
    m_providerLabel->setText(providerName);
    
    // Update timestamps if needed, or just "Updated just now" if we assume immediate push
    // For now hardcode or format timestamp
    // m_statusLabel->setText(...) 

    // Session
    m_sessionBar->setValue(static_cast<int>(snapshot.session.percent()));
    m_sessionUsed->setText(QString("%1% used").arg(snapshot.session.percent(), 0, 'f', 0));
    m_sessionReset->setText(snapshot.session.resetDescription);

    // Weekly
    m_weeklyBar->setValue(static_cast<int>(snapshot.weekly.percent()));
    m_weeklyUsed->setText(QString("%1% used").arg(snapshot.weekly.percent(), 0, 'f', 0));
    m_weeklyReset->setText(snapshot.weekly.resetDescription);
}

void MenuWidget::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
