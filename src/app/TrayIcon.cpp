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
    QStringList tooltipLines;
    tooltipLines << "codexbar"; // Fallback title
    
    // 1. Codex
    auto *codex = m_registry->provider(ProviderID::Codex);
    if (codex) {
        auto snap = codex->snapshot();
        // Use Codex icon if active (TODO: merge or cycle icons?)
        // For now, Codex wins for the icon.
        if (snap.session.total > 0) {
             m_sni->setIconByPixmap(IconRenderer::renderIcon(snap, false));
        }

        if (m_codexSessionAction) m_codexSessionAction->setText(QString("   Session: %1%").arg(snap.session.percent(), 0, 'f', 1));
        if (m_codexWeeklyAction) m_codexWeeklyAction->setText(QString("   Weekly: %1%").arg(snap.weekly.percent(), 0, 'f', 1));
        
        tooltipLines.clear();
        tooltipLines << QString("<b>%1 Usage</b>").arg(codex->name());
        tooltipLines << QString("Session: %1%").arg(snap.session.percent(), 0, 'f', 1);
        tooltipLines << QString("Weekly: %1%").arg(snap.weekly.percent(), 0, 'f', 1);
    }

    // 2. Claude
    auto *claude = m_registry->provider(ProviderID::Claude);
    if (claude) {
        auto snap = claude->snapshot();
        
        if (m_claudeSessionAction) m_claudeSessionAction->setText(QString("   Session: %1%").arg(snap.session.percent(), 0, 'f', 1));
        if (m_claudeWeeklyAction) m_claudeWeeklyAction->setText(QString("   Weekly: %1%").arg(snap.weekly.percent(), 0, 'f', 1));

        if (!tooltipLines.isEmpty()) tooltipLines << ""; // spacer
        tooltipLines << QString("<b>%1 Usage</b>").arg(claude->name());
        tooltipLines << QString("Session: %1%").arg(snap.session.percent(), 0, 'f', 1);
        tooltipLines << QString("Weekly: %1%").arg(snap.weekly.percent(), 0, 'f', 1);
    }
    
    // Set Tooltip
    // KStatusNotifierItem setToolTip(iconName, title, subTitle)
    // We can put the whole multiline string in subTitle?
    // Title: CodexBar
    // Subtitle: The list.
    m_sni->setToolTip("codexbar", "CodexBar", tooltipLines.join("\n"));
}

void TrayIcon::setupMenu() {
    // 1. App Name
    auto *title = m_menu->addAction("CodexBar");
    title->setEnabled(false);
    
    m_menu->addSeparator();

    // 2. Codex Section
    auto *codexHeader = m_menu->addAction("Codex");
    codexHeader->setEnabled(false);

    m_codexSessionAction = m_menu->addAction("   Session: --%");
    m_codexSessionAction->setEnabled(false);
    
    m_codexWeeklyAction = m_menu->addAction("   Weekly: --%");
    m_codexWeeklyAction->setEnabled(false);
    
    // 3. Claude Section
    auto *claudeHeader = m_menu->addAction("Claude");
    claudeHeader->setEnabled(false);

    m_claudeSessionAction = m_menu->addAction("   Session: --%");
    m_claudeSessionAction->setEnabled(false);
    
    m_claudeWeeklyAction = m_menu->addAction("   Weekly: --%");
    m_claudeWeeklyAction->setEnabled(false);

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
