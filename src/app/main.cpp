#include <KAboutData>
#include <KLocalizedString>
#include <QApplication>

#include "ProviderRegistry.h"
#include "TrayIcon.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  KLocalizedString::setApplicationDomain("kdecodexbar");

  KAboutData aboutData("kdecodexbar", i18n("KDECodexBar"), APP_VERSION,
                       i18n("CodexBar for Plasma"), KAboutLicense::MIT,
                       i18n("(c) 2025 Radu Ursache"), QString(),
                       "https://github.com/rursache/KDECodexBar");
  KAboutData::setApplicationData(aboutData);

  // Needed for proper icon association and portal registration
  app.setDesktopFileName("kdecodexbar");
  app.setQuitOnLastWindowClosed(false);

  auto *registry = new ProviderRegistry(&app);
  auto *trayIcon = new TrayIcon(registry, &app);
  // TODO: Connect registry to trayIcon

  return app.exec();
}
