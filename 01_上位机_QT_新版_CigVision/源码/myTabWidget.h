#pragma once
#include <QTabWidget>
#include "CusTabBar.h"

class myTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit myTabWidget(QWidget* parent = nullptr);
    virtual ~myTabWidget();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    CusTabBar* customTabBar;
};

