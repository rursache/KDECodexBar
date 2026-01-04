#pragma once

#include <QDialog>
#include <QSettings>

class QComboBox;
class QCheckBox;
class QDialogButtonBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Getters for current settings
    int refreshInterval() const; // in ms, -1 for manual
    bool isAutostartEnabled() const;

signals:
    void settingsChanged();

private slots:
    void saveSettings();
    void loadSettings();
    void updateAutostart(bool enable);

private:
    QComboBox *m_intervalCombo;
    QCheckBox *m_autostartCheck;
    QDialogButtonBox *m_buttonBox;
    QSettings m_settings;
};
