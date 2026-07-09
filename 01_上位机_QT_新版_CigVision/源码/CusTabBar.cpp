#include "CusTabBar.h"
#include <QStylePainter>
#include <QStyleOptionTab>
#include <QPainter>
#include <QMouseEvent>

CusTabBar::CusTabBar(QWidget* parent) : QTabBar(parent)
{
    setDrawBase(false);
    setExpanding(false);
    setDocumentMode(true);
    setElideMode(Qt::ElideNone);
}

QSize CusTabBar::tabSizeHint(int index) const
{
    Q_UNUSED(index)
    return QSize(tabWidth, tabHeight);
}

void CusTabBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制整体背景
    painter.fillRect(rect(), normalBgColor);

    for (int i = 0; i < count(); i++)
    {
        QRect tabRect = this->tabRect(i);
        
        // 绘制标签背景
        if (i == currentIndex()) {
            painter.fillRect(tabRect, selectedBgColor);
        }

        // 绘制分隔线
        if (i < count() - 1) {
            painter.setPen(borderColor);
            painter.drawLine(tabRect.topRight(), tabRect.bottomRight());
        }

        // 绘制文字
        painter.setPen(textColor);
        painter.drawText(tabRect, Qt::AlignCenter, tabText(i));
    }

    // 绘制底部边框线
    painter.setPen(borderColor);
    painter.drawLine(rect().bottomLeft(), rect().bottomRight());
}

void CusTabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int tab = tabAt(event->pos());
        if (tab >= 0) {
            setCurrentIndex(tab);
        }
    }
    QTabBar::mousePressEvent(event);
}
