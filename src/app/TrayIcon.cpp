#include "TrayIcon.h"
#include "IconRenderer.h"
#include <KLocalizedString>
#include <QAction>
#include <QWidgetAction>
#include <QIcon>
#include <QApplication>
#include <QCoreApplication>

TrayIcon::TrayIcon(ProviderRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_sni(new KStatusNotifierItem(this))
    , m_menu(new QMenu())
    , m_registry(registry)
    , m_timer(new QTimer(this))
{
    // Basic SNI setup
    m_sni->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_sni->setStatus(KStatusNotifierItem::Active);
    m_sni->setStandardActionsEnabled(false); // We provide our own Quit action
    
    // Use custom rendered placeholder icon initially
    m_sni->setIconByPixmap(IconRenderer::renderPlaceholder());
    
    m_sni->setToolTip(
        "codexbar",
        i18n("CodexBar"),
        i18n("Tracking AI Provider Usage")
    );

    setupMenu();
    m_sni->setContextMenu(m_menu);
    
    // Connect providers
    for (auto *provider : m_registry->providers()) {
        connect(provider, &Provider::dataChanged, this, &TrayIcon::updateIcon);
    }
    
    // Setup refresh timer (every 10 minutes, plus initial)
    // For specific providers like Codex, we might want faster polling or event driven?
    // macOS uses 5s or similar for active checks? 
    // Let's use 60s for now, and trigger immediate.
    connect(m_timer, &QTimer::timeout, this, [this](){
        for (auto *provider : m_registry->providers()) {
            provider->refresh();
        }
    });
    m_timer->start(60000); 
    
    // Initial refresh
    QTimer::singleShot(0, this, [this](){
        for (auto *provider : m_registry->providers()) {
            provider->refresh();
        }
    });
}

void TrayIcon::updateIcon() {
    // For v1, just use Codex provider
    auto *codex = m_registry->provider(ProviderID::Codex);
    if (codex) {
        auto snap = codex->snapshot();
        // Assume dark theme for now (TODO: detect)
        // Verify we have valid data (total > 0)
        if (snap.session.total > 0) {
            m_sni->setIconByPixmap(IconRenderer::renderIcon(snap, false));
            
            // Update widget (disabled for now as per requested menu layout)
            // m_menuWidget->updateData(codex->name(), snap);

            // Update tooltip
            QString title = QString("%1 Usage").arg(codex->name());
            QString tip = QString("Session: %1% | Weekly: %2%")
                .arg(snap.session.percent(), 0, 'f', 1)
                .arg(snap.weekly.percent(), 0, 'f', 1);
            m_sni->setToolTip("codexbar", title, tip);

            // Update menu actions
            // Use indentation as requested
            if (m_sessionAction) m_sessionAction->setText(QString("   Session: %1%").arg(snap.session.percent(), 0, 'f', 1));
            if (m_weeklyAction) m_weeklyAction->setText(QString("   Weekly: %1%").arg(snap.weekly.percent(), 0, 'f', 1));
        }
    }
}

void TrayIcon::setupMenu() {
    // 1. App Name
    auto *title = m_menu->addAction("CodexBar");
    title->setEnabled(false); // Header style
    
    m_menu->addSeparator();

    // 2. Codex Section
    auto *codexHeader = m_menu->addAction("Codex");
    codexHeader->setEnabled(false);

    m_sessionAction = m_menu->addAction("   Session: --%");
    m_sessionAction->setEnabled(false);
    
    m_weeklyAction = m_menu->addAction("   Weekly: --%");
    m_weeklyAction->setEnabled(false);
    
    m_menu->addSeparator();
    
    // 3. Settings & Refresh
    auto *settings = m_menu->addAction(i18n("Settings"));
    settings->setEnabled(false); // Does nothing for now
    
    m_menu->addAction(i18n("Refresh All"), this, [this](){
        for (auto *provider : m_registry->providers()) {
            provider->refresh();
        }
    });
    
    m_menu->addSeparator();

    // 4. Quit
    auto quitAction = m_menu->addAction(QIcon::fromTheme("application-exit"), i18n("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}
