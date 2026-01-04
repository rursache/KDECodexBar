#include "IconRenderer.h"
#include <QPainter>
#include <QPixmap>
#include <QRect>

// Plasma tray icons are typically 22x22
static const QSize kIconSize(22, 22);

static void g_drawBar(QPainter &p, const QRect &rect, double percent, const QColor &fg) {
    // Track
    p.setPen(Qt::NoPen);
    QColor trackColor = fg;
    trackColor.setAlphaF(0.3);
    p.setBrush(trackColor);
    
    // Pill shape: radius = height / 2.0
    qreal radius = rect.height() / 2.0;
    p.drawRoundedRect(rect, radius, radius);

    // Fill
    if (percent > 0) {
        if (percent > 100) percent = 100;
        QRectF fill = rect; // Use QRectF for precision if needed, but QRect is fine for display
        fill.setWidth(rect.width() * (percent / 100.0));
        p.setBrush(fg);
        p.drawRoundedRect(fill, radius, radius);
    }
}

QIcon IconRenderer::renderIcon(const UsageSnapshot &snapshot, bool isDarkTheme) {
    QPixmap pixmap(kIconSize);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    // TODO: Detect system theme color properly. For now assume white (dark theme/panel) or black (light theme).
    // In Plasma, KIconLoader usually handles this, or utilize standard palette.
    QColor fg = isDarkTheme ? Qt::white : Qt::black; 
    // Fallback: use a neutral color or hardcoded white if we assume dark panel.
    // Plasma panels are often dark. Let's default to white for visibility on dark panels for v0.
    fg = Qt::white; 

    // Draw a simple box structure to look like the empty bars
    QRect rect = QRect(QPoint(0, 0), kIconSize);
    
    // Vertical layout calculation
    // Total height = Top(5) + Gap(2) + Bottom(3) = 10px
    // Center it in 22px height -> (22 - 10) / 2 = 6px top margin
    int topY = 6;
    int leftX = 2; // Wider bars (margin 2)
    int rightX = 2;
    int width = rect.width() - leftX - rightX;

    // Top Bar: Session Usage (Thicker)
    QRect topBar(leftX, topY, width, 5);
    
    // Bottom Bar: Weekly Usage (Thinner)
    QRect bottomBar(leftX, topY + 5 + 2, width, 3);

    // Reuse common drawing logic by creating a dummy snapshot for placeholder?
    // Or just draw manually here to ensure placeholder look is distinct if needed.
    // For now, drawing manually as per previous code structure.
    
    g_drawBar(p, topBar, snapshot.session.percent(), fg);
    g_drawBar(p, bottomBar, snapshot.weekly.percent(), fg);

    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

QIcon IconRenderer::renderPlaceholder() {
    // This method is no longer used/needed to be public if renderIcon handles 0 usage? 
    // But TrayIcon calls it. We should make it call renderIcon with empty snapshot.
    // Or just duplicate the logic briefly for now to keep it compiling.
    // Ideally we'd remove renderPlaceholder and just use renderIcon(UsageSnapshot()).
    // Let's implement it using g_drawBar for consistency.
    
    QPixmap pixmap(kIconSize);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    
    QColor fg = Qt::white; 

    QRect rect = QRect(QPoint(0, 0), kIconSize);
    int topY = 6;
    int leftX = 2;
    int rightX = 2;
    int width = rect.width() - leftX - rightX;

    QRect topBar(leftX, topY, width, 5);
    QRect bottomBar(leftX, topY + 5 + 2, width, 3);
    
    g_drawBar(p, topBar, 0.0, fg);
    g_drawBar(p, bottomBar, 0.0, fg);
    
    QIcon icon;
    icon.addPixmap(pixmap);
    return icon;
}

void IconRenderer::drawBars(QPainter &p, const QRect &rect, const UsageSnapshot &snapshot, const QColor &fg) {
    // Vertical layout calculation
    int topY = 6;
    int leftX = 2;
    int rightX = 2;
    int width = rect.width() - leftX - rightX;

    // Top Bar
    QRect topBar(leftX, topY, width, 5);
    g_drawBar(p, topBar, snapshot.session.percent(), fg);

    // Bottom Bar
    QRect bottomBar(leftX, topY + 5 + 2, width, 3);
    g_drawBar(p, bottomBar, snapshot.weekly.percent(), fg);
}
