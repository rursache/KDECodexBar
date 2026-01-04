#pragma once

#include <QObject>
#include <KStatusNotifierItem>
#include <QObject>
#include <QObject>
#include <QTimer>
#include "ProviderRegistry.h"
#include "MenuWidget.h"

class QMenu;
class QWidgetAction;

class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(ProviderRegistry *registry, QObject *parent = nullptr);

private slots:
    void updateIcon();

private:
    void setupMenu();

    KStatusNotifierItem *m_sni;
    QMenu *m_menu;
    MenuWidget *m_menuWidget;
    QWidgetAction *m_menuAction;
    QAction *m_codexSessionAction; 
    QAction *m_codexWeeklyAction;
    QAction *m_claudeSessionAction;
    QAction *m_claudeWeeklyAction;
    ProviderRegistry *m_registry;
    QTimer *m_timer;
};
