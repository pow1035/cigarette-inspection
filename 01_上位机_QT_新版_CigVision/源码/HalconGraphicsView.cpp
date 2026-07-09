#include "HalconGraphicsView.h"
#include <QGraphicsPixmapItem>
#include <QScrollBar>
#include <QGraphicsSceneMouseEvent>
#include <QImageReader>

HalconGraphicsView::HalconGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
    , scene(new QGraphicsScene(this))
    , imageItem(nullptr)
    , currentZoom(1.0)
    , isPanning(false)
{
    setScene(scene);
    
    // 设置场景和视图的背景色为黑色
    scene->setBackgroundBrush(Qt::black);
    setBackgroundBrush(Qt::black);
    
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
}

HalconGraphicsView::~HalconGraphicsView()
{
    delete scene;
}

void HalconGraphicsView::displayImage(const QImage &image)
{
    // 先清除所有内容（包括图形项和图像）
    clearItems();  // 清除所有图形项
    clearImage();  // 清除图像
    
    if (!image.isNull()) {
        imageItem = scene->addPixmap(QPixmap::fromImage(image));
        scene->setSceneRect(image.rect());
        
        // 更新图像信息
        imageInfo.width = image.width();
        imageInfo.height = image.height();
        imageInfo.channels = image.depth() / 8;  // depth是位深度，除以8得到字节数（通道数）
        imageInfo.format = image.format() == QImage::Format_RGB888 ? "RGB" :
                          image.format() == QImage::Format_ARGB32 ? "ARGB" :
                          image.format() == QImage::Format_Grayscale8 ? "Grayscale" : "Unknown";
        imageInfo.isValid = true;
        // pixelSize需要在标定后设置
        
        fitInView();
    } else {
        imageInfo = ImageInfo(); // 重置图像信息
    }
}

void HalconGraphicsView::displayHImage(const void* hImage)
{
    // TODO: 实现Halcon图像的显示
    // 需要添加Halcon相关的转换代码
}

MovableRectItem* HalconGraphicsView::drawMovableRectangle(const QRectF &rect)
{
    if (scene) {
        MovableRectItem* rectItem = new MovableRectItem(rect);
        scene->addItem(rectItem);
        return rectItem;
    }
    return nullptr;
}

FixedRectItem* HalconGraphicsView::drawFixedRectangle(const QRectF &rect, const QPen &pen)
{
    if (scene) {
        FixedRectItem* rectItem = new FixedRectItem(rect, pen);
        scene->addItem(rectItem);
        return rectItem;
    }
    return nullptr;
}

QRectF HalconGraphicsView::getRectanglePosition(QGraphicsRectItem* rect) const
{
    if (rect) {
        return rect->rect().translated(rect->pos());
    }
    return QRectF();
}

void HalconGraphicsView::drawPoint(const QPointF &point, const QPen &pen)
{
    if (scene) {
        scene->addEllipse(point.x() - 2, point.y() - 2, 4, 4, pen, QBrush(pen.color()));
    }
}

void HalconGraphicsView::drawLine(const QLineF &line, const QPen &pen)
{
    if (scene) {
        scene->addLine(line, pen);
    }
}

void HalconGraphicsView::clearItems()
{
    if (scene) {
        QList<QGraphicsItem*> items = scene->items();
        for (QGraphicsItem* item : items) {
            // 如果不是图像项，就删除
            if (item != imageItem) {
                scene->removeItem(item);
                delete item;
            }
        }
    }
}

void HalconGraphicsView::clearImage()
{
    if (scene) {
        scene->clear();
        imageItem = nullptr;
    }
}

void HalconGraphicsView::zoomIn()
{
    scale(1.2, 1.2);
    currentZoom *= 1.2;
}

void HalconGraphicsView::zoomOut()
{
    scale(1.0/1.2, 1.0/1.2);
    currentZoom /= 1.2;
}

void HalconGraphicsView::fitInView(const QRectF &rect, Qt::AspectRatioMode aspectRatioMode)
{
    if (scene && imageItem) {
        if (rect.isEmpty()) {
            QGraphicsView::fitInView(imageItem, aspectRatioMode);
        } else {
            QGraphicsView::fitInView(rect, aspectRatioMode);
        }
        currentZoom = 1.0;
    }
}

void HalconGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else {
        zoomOut();
    }
}

void HalconGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        isPanning = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void HalconGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (isPanning) {
        QPointF delta = event->pos() - lastMousePos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastMousePos = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void HalconGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

bool HalconGraphicsView::loadJpgImage(const QString &filePath)
{
    QImageReader reader(filePath);
    if (!reader.canRead()) {
        return false;
    }

    QImage image = reader.read();
    if (image.isNull()) {
        return false;
    }

    displayImage(image);
    return true;
} 