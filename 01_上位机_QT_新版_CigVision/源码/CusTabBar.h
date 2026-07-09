#pragma once
#include <qtabbar.h>
class CusTabBar : public QTabBar
{
    Q_OBJECT
public:
    CusTabBar(QWidget* parent = nullptr);
    QSize tabSizeHint(int index) const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QColor normalBgColor = QColor(240, 240, 240);      // 正常背景色
    QColor selectedBgColor = QColor(200, 200, 200);    // 选中背景色
    QColor borderColor = QColor(180, 180, 180);        // 边框颜色
    QColor textColor = QColor(60, 60, 60);             // 文字颜色
    int tabHeight = 30;                                // 标签高度
    int tabWidth = 100;                                // 标签宽度
};