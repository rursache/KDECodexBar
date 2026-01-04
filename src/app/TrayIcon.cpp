#include "TrayIcon.h"
#include "IconRenderer.h"
#include <KLocalizedString>
#include <QAction>
#include <QWidgetAction>
#include <QIcon>
#include <QApplication>
#include <QCoreApplication>
#include "ProviderRegistry.h"
#include "Provider.h"

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
        if (!snap.limits.isEmpty()) {
             m_sni->setIconByPixmap(IconRenderer::renderIcon(snap, false));
        }

        // Action text updates handled by menu setup mostly, but if we have persistent actions:
        // Note: In new dynamic menu, we rebuild menu on showing or we should update actions dynamically.
        // For now, let's assumes updateIcon just updates the SNI icon and tooltip.
        // Actions in the menu are static pointers m_codexSessionAction which we should probably remove or update differently.
        // Since we switched to loop-based setupMenu, we don't have m_codexSessionAction handy unless we map them.
        // Let's rely on setupMenu being called or refreshing the menu logic? 
        // Actually setupMenu clears the menu. So actions are recreated.
        // But KStatusNotifierItem menu might need explicit update if it's open.
        // For now, just fix the compile error by removing old specific action updates if they are not reliable.
    }
    // Tooltip
    QString tooltip = "<b>CodexBar</b>";
    
    for (const auto &provider : m_registry->providers()) {
        if (provider->state() == ProviderState::Active) {
            UsageSnapshot snap = provider->snapshot();
            tooltip += QString("%1:\n").arg(provider->name());
            
            for (const auto &limit : snap.limits) {
                  tooltip += QString("  %1: %2%\n")
                    .arg(limit.label)
                    .arg(limit.percent(), 0, 'f', 1);
            }
        }
    }
    
    if (tooltip.isEmpty()) {
        tooltip = "No usage data";
    } else {
        tooltip.chop(1);
    }
    
    m_sni->setToolTip(QIcon(), "CodexBar", tooltip);
    
    // Refresh the menu actions to reflect new data
    setupMenu();
}

void TrayIcon::setupMenu() {
    m_menu->clear();

    // App Name
    auto *title = m_menu->addAction("CodexBar");
    title->setEnabled(false);
    m_menu->addSeparator();

    for (const auto &provider : m_registry->providers()) {
        // Section Header
        QAction *header = m_menu->addAction(provider->name());
        header->setEnabled(false);
        
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);

        UsageSnapshot snap = provider->snapshot();
        // Dynamic stats
        if (snap.limits.isEmpty()) {
             QAction *act = m_menu->addAction("  No usage data");
             act->setEnabled(false);
        } else {
             for (const auto &limit : snap.limits) {
                 QString text = QString("  %1: %2%").arg(limit.label).arg(limit.percent(), 0, 'f', 1);
                 
                 // Append reset info if available
                 if (!limit.resetDescription.isEmpty()) {
                     text += QString(" (%1)").arg(limit.resetDescription);
                 }
                 
                 QAction *act = m_menu->addAction(text);
                 act->setEnabled(false);
             }
        }

        m_menu->addSeparator();
    }
    
    // Settings & Refresh
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
