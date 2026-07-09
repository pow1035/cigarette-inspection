#ifndef HALCONGRAPHICSVIEW_H
#define HALCONGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <windows.h>
#include <HalconCpp.h>
using namespace HalconCpp;


    

// 可移动的矩形项类（蓝色）
class MovableRectItem : public QGraphicsRectItem
{
public:
    explicit MovableRectItem(const QRectF &rect, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(rect, parent) {
        setFlag(QGraphicsItem::ItemIsMovable);
        setFlag(QGraphicsItem::ItemIsSelectable);
        setFlag(QGraphicsItem::ItemSendsGeometryChanges);
        // 设置默认蓝色
        setPen(QPen(Qt::blue, 2));
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        if (change == ItemPositionChange && scene()) {
            QPointF newPos = value.toPointF();
            QRectF rect = scene()->sceneRect();
            newPos.setX(qMin(rect.right(), qMax(newPos.x(), rect.left())));
            newPos.setY(qMin(rect.bottom(), qMax(newPos.y(), rect.top())));
            return newPos;
        }
        return QGraphicsRectItem::itemChange(change, value);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            QGraphicsRectItem::mousePressEvent(event);
        }
    }
};

// 固定的矩形项类
class FixedRectItem : public QGraphicsRectItem
{
public:
    explicit FixedRectItem(const QRectF &rect, const QPen &pen, QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(rect, parent) {
        setPen(pen);
    }
};

class HalconGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit HalconGraphicsView(QWidget *parent = nullptr);
    ~HalconGraphicsView();

    // 图像基本信息
    struct ImageInfo {
        int width = 0;          // 图像宽度
        int height = 0;         // 图像高度
        int channels = 0;       // 图像通道数
        QString format;         // 图像格式
        double pixelSize = 0.0; // 像素尺寸（用于标定）
        bool isValid = false;   // 图像是否有效
    } imageInfo;

    // 获取图像信息
    const ImageInfo& getImageInfo() const { return imageInfo; }
    
    // 显示QImage
    void displayImage(const QImage &image);
    // 显示Halcon图像（后续需要添加Halcon相关头文件）
    void displayHImage(const void* hImage);
    
    // 绘制功能
    MovableRectItem* drawMovableRectangle(const QRectF &rect);  // 可移动的蓝色矩形
    FixedRectItem* drawFixedRectangle(const QRectF &rect, const QPen &pen);  // 固定的自定义颜色矩形
    void drawPoint(const QPointF &point, const QPen &pen = QPen(Qt::red));
    void drawLine(const QLineF &line, const QPen &pen = QPen(Qt::red));
    
    // 获取矩形位置
    QRectF getRectanglePosition(QGraphicsRectItem* rect) const;
    
    // 清除所有图形项
    void clearItems();
    // 清除图像
    void clearImage();
    // 缩放功能
    void zoomIn();
    void zoomOut();
    void fitInView(const QRectF &rect = QRectF(), Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio);

    // 添加新方法：加载JPG图片
    bool loadJpgImage(const QString &filePath);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QGraphicsScene *scene;
    QGraphicsPixmapItem *imageItem;
    qreal currentZoom;
    QPointF lastMousePos;
    bool isPanning;
};

#endif // HALCONGRAPHICSVIEW_H 