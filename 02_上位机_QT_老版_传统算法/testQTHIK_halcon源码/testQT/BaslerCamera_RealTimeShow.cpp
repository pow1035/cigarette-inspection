#include "BaslerCamera_RealTimeShow.h"
#include <QDebug>
#include <QPainter>
#include <QRect>

BaslerCamera_RealTimeShow::BaslerCamera_RealTimeShow(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    m_camera.RegisterImageEventHandler(this, Pylon::RegistrationMode_ReplaceAll, Pylon::Ownership_ExternalOwnership);
    // Register this object as a configuration event handler, so we will be notified of camera state changes.
    // See Pylon::CConfigurationEventHandler for details
    m_camera.RegisterConfiguration(this, Pylon::RegistrationMode_ReplaceAll, Pylon::Ownership_ExternalOwnership);

    // Add the AutoPacketSizeConfiguration and let pylon delete it when not needed anymore.
    m_camera.RegisterConfiguration(new CAutoPacketSizeConfiguration(), Pylon::RegistrationMode_Append, Pylon::Cleanup_Delete);

    m_camera.Attach(Pylon::CTlFactory::GetInstance().CreateFirstDevice(), Pylon::Cleanup_Delete);

    m_camera.Open();

    // Camera may have been disconnected.
    if (!m_camera.IsOpen() || m_camera.IsGrabbing())
    {
        return;
    }

    // Since we may switch between single and continuous shot, we must configure the camera accordingly.
    // The predefined configurations are only executed once when the camera is opened.
    // To be able to use them in our use case, we just call them explicitly to apply the configuration.
    m_continousConfiguration.OnOpened(m_camera);

    // Start grabbing until StopGrabbing() is called.
    m_camera.StartGrabbing(Pylon::GrabStrategy_OneByOne, Pylon::GrabLoop_ProvidedByInstantCamera);

    ui.centralWidget->installEventFilter(this);//安装Qt的事件过滤器

    connect(this, SIGNAL(OneImageFinishSignal()), this, SLOT(OneImageFinishSlot()));

}

void BaslerCamera_RealTimeShow::OnImagesSkipped(Pylon::CInstantCamera& camera, size_t countOfSkippedImages)
{

}
void BaslerCamera_RealTimeShow::OnImageGrabbed(Pylon::CInstantCamera& camera, const Pylon::CGrabResultPtr& grabResult)
{

    m_mutexLock.lock();

    m_ptrGrabResult = grabResult;//将捕获到的图像传递出去

    //qDebug() << __FUNCTION__;

    emit OneImageFinishSignal();

    m_mutexLock.unlock();

}

void BaslerCamera_RealTimeShow::OneImageFinishSlot()
{
    //qDebug() << __FUNCTION__;
    ui.centralWidget->update();
}
bool BaslerCamera_RealTimeShow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui.centralWidget && event->type() == QEvent::Paint)
    {
        showImage();
    }
    return false;
}
void BaslerCamera_RealTimeShow::showImage()
{

    m_mutexLock.lock();

    //qDebug() << "123" << endl;

      // 新建pylon ImageFormatConverter对象.
    CImageFormatConverter formatConverter;

    Mat openCvImage;
    QPainter painter(ui.centralWidget);

    //确定输出像素格式
    formatConverter.OutputPixelFormat = PixelType_BGR8packed;
    //将抓取的缓冲数据转化成pylon image.
    formatConverter.Convert(m_bitmapImage, m_ptrGrabResult);

    // 将 pylon image转成OpenCV image.
    openCvImage = cv::Mat(m_ptrGrabResult->GetHeight(), m_ptrGrabResult->GetWidth(), CV_8UC3, (uint8_t*)m_bitmapImage.GetBuffer());

    QImage img((const unsigned char*)(openCvImage.data), openCvImage.cols, openCvImage.rows, openCvImage.cols * 3, QImage::Format_RGB888);

    QRectF target;
    target.setLeft(0);
    target.setTop(0);
    target.setSize(this->size());

    QRectF source;
    source.setLeft(0);
    source.setTop(0);
    source.setSize(img.size());

    painter.drawImage(target, img, source);

    m_mutexLock.unlock();


}

// Pylon::CConfigurationEventHandler functions
void BaslerCamera_RealTimeShow::OnAttach(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnAttached(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnDetach(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnDetached(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnDestroy(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnDestroyed(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnOpen(Pylon::CInstantCamera& camera)
{
    Pylon::String_t strFriendlyName = camera.GetDeviceInfo().GetFriendlyName();
    qDebug() << __FUNCTION__ << " - " << strFriendlyName.c_str();
}


void BaslerCamera_RealTimeShow::OnOpened(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnClose(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnClosed(Pylon::CInstantCamera& camera)
{
    Pylon::String_t strFriendlyName = camera.GetDeviceInfo().GetFriendlyName();
    qDebug() << __FUNCTION__ << " - " << strFriendlyName.c_str();
}


void BaslerCamera_RealTimeShow::OnGrabStart(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnGrabStarted(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnGrabStop(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnGrabStopped(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
    m_camera.DeregisterConfiguration(&m_continousConfiguration);
}


void BaslerCamera_RealTimeShow::OnGrabError(Pylon::CInstantCamera& camera, const char* errorMessage)
{
    qDebug() << __FUNCTION__;
}


void BaslerCamera_RealTimeShow::OnCameraDeviceRemoved(Pylon::CInstantCamera& camera)
{
    qDebug() << __FUNCTION__;
}

BaslerCamera_RealTimeShow::~BaslerCamera_RealTimeShow()
{
    Perform cleanup.
        if (m_camera.IsPylonDeviceAttached())
        {
            try
            {
                // Close camera.
                // This will also stop the grab.
                m_camera.Close();

                // Free the camera.
                // This will also stop the grab and close the camera.
                m_camera.DestroyDevice();
            }
            catch (const Pylon::GenericException & e)
            {
                qDebug() << e.what();
            }
        }
}
