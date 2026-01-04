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
#include <QSettings>

TrayIcon::TrayIcon(ProviderRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_sni(new KStatusNotifierItem(this))
    , m_menu(new QMenu())
    , m_registry(registry)
    , m_timer(new QTimer(this))
    , m_selectedProviderID(ProviderID::Codex)
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
    
    // Setup refresh timer (every 60s)
    connect(m_timer, &QTimer::timeout, this, [this](){
        for (auto *provider : m_registry->providers()) {
            provider->refresh();
        }
    });
    m_timer->start(60000); 
    
    // Initial refresh
    QTimer::singleShot(0, this, [this](){
        // Create settings dialog to load initial settings (or just use QSettings directly here)
        // Since we want to respect the saved interval, we should read it properly.
        QSettings s("KDECodexBar", "KDECodexBar");
        int interval = s.value("refresh_interval", 60000).toInt();
        if (interval > 0) m_timer->start(interval);
        else m_timer->stop();

        for (auto *provider : m_registry->providers()) {
            provider->refresh();
        }
    });
}

void TrayIcon::updateIcon() {
    // 1. Icon Update
    // Only render icon for the selected provider
    auto *provider = m_registry->provider(m_selectedProviderID);
    bool iconUpdated = false;
    
    if (provider && provider->state() == ProviderState::Active) {
        auto snap = provider->snapshot();
        if (!snap.limits.isEmpty()) {
             m_sni->setIconByPixmap(IconRenderer::renderIcon(snap, false));
             iconUpdated = true;
        }
    }
    
    if (!iconUpdated) {
        // Fallback or keep previous? simpler to just placeholder if empty/inactive
        // m_sni->setIconByPixmap(IconRenderer::renderPlaceholder());
    }

    // 2. Tooltip Update
    // Only show selected provider in tooltip
    QString tooltip;
    
    if (provider && provider->state() == ProviderState::Active) {
         UsageSnapshot snap = provider->snapshot();
         if (!snap.limits.isEmpty()) {
            tooltip += QString("<b>%1</b>").arg(provider->name());
            for (const auto &limit : snap.limits) {
                  tooltip += QString("<br>&nbsp;&nbsp;%1: %2%")
                    .arg(limit.label)
                    .arg(limit.percent(), 0, 'f', 1);
            }
         }
    }

    // Use a descriptive title instead of App Name to ensure visibility and avoid duplication
    // User requested "CodexBar" as title -> "KDECodexBar"
    m_sni->setToolTip(QIcon(), "KDECodexBar", tooltip);
    
    // Refresh the menu actions (stats might have changed)
    setupMenu();
}

void TrayIcon::setupMenu() {
    m_menu->clear();

    // App Name
    auto *title = m_menu->addAction("KDECodexBar");
    title->setEnabled(false);
    m_menu->addSeparator();

    for (const auto &provider : m_registry->providers()) {
        // Section Header (Selectable)
        bool isSelected = (provider->id() == m_selectedProviderID);
        // Use text-based indicator to control spacing
        QString label = isSelected ? QStringLiteral("✓ %1").arg(provider->name()) 
                                   : QStringLiteral("   %1").arg(provider->name());
        
        QAction *header = m_menu->addAction(label);
        
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);
        
        // Handle selection
        connect(header, &QAction::triggered, this, [this, provider](){
            m_selectedProviderID = provider->id();
            updateIcon(); // Will redraw icon/tooltip and rebuild menu (updating checks)
        });

        UsageSnapshot snap = provider->snapshot();
        // Dynamic stats
        if (snap.limits.isEmpty()) {
             QAction *act = m_menu->addAction("     No usage data");
             act->setEnabled(false);
        } else {
             for (const auto &limit : snap.limits) {
                 QString text = QString("     %1: %2%").arg(limit.label).arg(limit.percent(), 0, 'f', 1);
                 
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
    connect(settings, &QAction::triggered, this, [this](){
        if (!m_settingsDialog) {
            m_settingsDialog = new SettingsDialog(); // Create lazily or pass parent
            // Since TrayIcon is a QObject not QWidget, careful with parent logic for dialogs.
            // But we can just show it.
            connect(m_settingsDialog, &SettingsDialog::settingsChanged, this, &TrayIcon::applySettings);
        }
        m_settingsDialog->show();
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
    });
    
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

void TrayIcon::applySettings() {
    if (!m_settingsDialog) return;
    
    int interval = m_settingsDialog->refreshInterval();
    if (interval > 0) {
        m_timer->start(interval);
    } else {
        m_timer->stop();
    }
}
