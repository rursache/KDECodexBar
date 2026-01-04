#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_settings("CodexBar", "CodexBar")
{
    setWindowTitle(tr("CodexBar Settings"));
    setFixedSize(300, 200);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Refresh Interval
    QLabel *intervalLabel = new QLabel(tr("Refresh Interval:"), this);
    layout->addWidget(intervalLabel);

    m_intervalCombo = new QComboBox(this);
    m_intervalCombo->addItem(tr("Manual"), -1);
    m_intervalCombo->addItem(tr("1 Minute"), 60000);
    m_intervalCombo->addItem(tr("3 Minutes"), 180000);
    m_intervalCombo->addItem(tr("5 Minutes"), 300000);
    m_intervalCombo->addItem(tr("15 Minutes"), 900000);
    layout->addWidget(m_intervalCombo);

    layout->addSpacing(10);

    // Autostart
    m_autostartCheck = new QCheckBox(tr("Run at Startup"), this);
    layout->addWidget(m_autostartCheck);

    layout->addStretch();

    // Buttons
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttonBox);

    loadSettings();
}

int SettingsDialog::refreshInterval() const {
    return m_intervalCombo->currentData().toInt();
}

bool SettingsDialog::isAutostartEnabled() const {
    return m_autostartCheck->isChecked();
}

void SettingsDialog::loadSettings() {
    int interval = m_settings.value("refresh_interval", 60000).toInt(); // Default 1 min
    int index = m_intervalCombo->findData(interval);
    if (index != -1) {
        m_intervalCombo->setCurrentIndex(index);
    } else {
        m_intervalCombo->setCurrentIndex(1); // Default to 1 min if unknown
    }

    bool autostart = m_settings.value("autostart", false).toBool();
    m_autostartCheck->setChecked(autostart);
}

void SettingsDialog::saveSettings() {
    m_settings.setValue("refresh_interval", refreshInterval());
    m_settings.setValue("autostart", isAutostartEnabled());
    
    updateAutostart(isAutostartEnabled());
    
    emit settingsChanged();
    accept();
}

void SettingsDialog::updateAutostart(bool enable) {
    // Linux XDG Autostart
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString autostartDir = configDir + "/autostart";
    QDir dir(autostartDir);
    if (!dir.exists()) dir.mkpath(".");

    QString desktopFile = autostartDir + "/codexbar.desktop";

    if (enable) {
        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=CodexBar\n";
            out << "Exec=" << QCoreApplication::applicationFilePath() << "\n";
            out << "Icon=codexbar\n"; // Assume icon exists or let it fallback
            out << "Comment=AI Usage Tracker\n";
            out << "X-GNOME-Autostart-enabled=true\n";
            out << "Terminal=false\n";
            file.close();
        }
    } else {
        QFile::remove(desktopFile);
    }
}
