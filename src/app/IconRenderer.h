#pragma once

#include <QIcon>
#include <QColor>
#include <QSize>
#include "Provider.h" // For UsageSnapshot

class IconRenderer {
public:
    static QIcon renderIcon(const UsageSnapshot &snapshot, bool isDarkTheme = false);
    static QIcon renderPlaceholder();

private:
    static void drawBars(QPainter &p, const QRect &rect, const UsageSnapshot &snapshot, const QColor &fg);
};
