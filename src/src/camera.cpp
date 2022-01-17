/*
* Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     hujianbo <hujianbo@uniontech.com>
*             shicetu <shicetu@uniontech.com>
* Maintainer: hujianbo <hujianbo@uniontech.com>
*             shicetu <shicetu@uniontech.com>
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define TMPPATH "/tmp/tmp.jpg"

#include "camera.h"
#include "videosurface.h"
#include "datamanager.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "core_io.h"
#include "v4l2_devices.h"
#ifdef __cplusplus
}
#endif

#include <sys/stat.h>
#include <unistd.h>

#include <QUrl>
#include <QtMultimediaWidgets/QCameraViewfinder>
#include <QDir>

#define MAXLINE 100

Camera *Camera::m_instance = nullptr;

static _cam_config_t camConfig = {1920, 1080, NULL, NULL, NULL, NULL, NULL, NULL, 1, 30};
Camera *Camera::instance()
{
    if (m_instance == nullptr) {
        m_instance = new Camera;
    }
    return m_instance;
}

Camera::Camera()
    : m_camera(nullptr)
    , m_mediaRecoder(nullptr)
    , m_imageCapture(nullptr)
    , m_videoSurface(nullptr)
    , m_curDevName("")
    , m_bReadyRecord(false)
{
    parseConfig();
    initMember();
}

void Camera::initMember()
{
    // 连接相机surface，能够发送每帧QImage数据到外部
    m_videoSurface = new VideoSurface(this);
    connect(m_videoSurface, &VideoSurface::presentImage, this, &Camera::presentImage);

    // 初始设备名为空，默认启动config中的摄像头设备，若config设备名为空，则启动defaultCamera
    switchCamera();
}

void Camera::switchCamera()
{
    refreshCamDevList();

    int nCurIndex = m_cameraDevList.indexOf(m_curDevName);

    QString nextDevName;
    if (nCurIndex + 1 < m_cameraDevList.size() && nCurIndex != -1)
        nextDevName = m_cameraDevList[nCurIndex + 1];
    else if (nCurIndex + 1 == m_cameraDevList.size() && nCurIndex != -1)
        nextDevName = m_cameraDevList[0];
    else {
        nextDevName = camConfig.device_location;
        if (nextDevName.isEmpty())
            nextDevName = QCameraInfo::defaultCamera().deviceName();
    }

    startCamera(nextDevName);
}

void Camera::refreshCamera()
{
    refreshCamDevList();

    // 设备链表找不到当前设备，发送设备断开信号
    if (!m_curDevName.isEmpty() && m_cameraDevList.indexOf(m_curDevName) == -1) {
        m_curDevName = "";
        emit currentCameraDisConnected();
    }

    // 若没有摄像头工作，则重启摄像头
    restartCamera();
}

// 重启摄像头
void Camera::restartCamera()
{
    QList<QCameraInfo> availableCams = QCameraInfo::availableCameras();
    if (availableCams.isEmpty() && !m_cameraDevList.isEmpty()){
        if (!m_camera || m_camera->status() == QCamera::UnloadedStatus) {
            emit cameraCannotUsed();
            return;
        }
    }

    if (DataManager::instance()->getdevStatus() == CAM_CANUSE)
        return;

    startCamera(QCameraInfo::defaultCamera().deviceName());

    DataManager::instance()->setdevStatus(CAM_CANUSE);

    emit cameraDevRestarted();
}

void Camera::refreshCamDevList()
{
    m_cameraDevList.clear();

    v4l2_device_list_t *devlist = get_device_list();
    for (int i = 0; i < devlist->num_devices; i++)
        m_cameraDevList.push_back(devlist->list_devices[i].device);
}

void Camera::onCameraStatusChanged(QCamera::Status status)
{
    if (m_mediaRecoder) {
        if (m_bReadyRecord && status == QCamera::ActiveStatus) {
            m_mediaRecoder->record();
            m_bReadyRecord = false;
        }
    }
}

QStringList Camera::getSupportResolutions()
{
    if (!m_imageCapture)
        return  QStringList();

    QStringList resolutionsList;

    QList<QSize> supportResolutionList = m_imageCapture->supportedResolutions();
    if (supportResolutionList.isEmpty())
        qInfo() << "Support Resolution is Empty!";

    int size = supportResolutionList.size();
    resolutionsList.clear();
    for (int i = 0; i < size; i++) {
        QString resol = QString("%1x%2").arg(supportResolutionList[size - 1 -i].width()).arg(supportResolutionList[size - 1 -i].height());
        resolutionsList.append(resol);
    }

    return resolutionsList;
}

// 设置相机分辨率
void Camera::setCameraResolution(QSize size)
{
    Q_ASSERT(m_camera);
    Q_ASSERT(m_imageCapture);

    // 设置图片捕获器分辨率到相机
    QImageEncoderSettings imageSettings = m_imageCapture->encodingSettings();
    imageSettings.setResolution(size);
    m_imageCapture->setEncodingSettings(imageSettings);

    // 设置视频分辨率到相机
    QAudioEncoderSettings audioSettings = m_mediaRecoder->audioSettings();
    QVideoEncoderSettings videoSettings = m_mediaRecoder->videoSettings();
    videoSettings.setCodec("video/x-vp8");
    videoSettings.setResolution(size);
    m_mediaRecoder->setEncodingSettings(audioSettings, videoSettings, "video/webm");

    // 设置分辨率到相机
    QCameraViewfinderSettings viewfinderSettings = m_camera->viewfinderSettings();
    viewfinderSettings.setResolution(size);
    m_camera->setViewfinderSettings(viewfinderSettings);

    // 同步有变更的分辨率到config
    camConfig.width = size.rwidth();
    camConfig.height = size.rheight();
    saveConfig();
}

QSize Camera::getCameraResolution()
{
    return QSize(camConfig.width,camConfig.height);
}

void Camera::startCamera(const QString &devName)
{
    // 存在上一相机，先停止
    if (m_camera)
        stopCamera();

    m_camera = new QCamera(devName.toStdString().c_str());
    QCameraInfo cameraInfo(*m_camera);
    m_curDevName = devName;

    connect(m_camera, SIGNAL(statusChanged(QCamera::Status)), this, SLOT(onCameraStatusChanged(QCamera::Status)));

    m_imageCapture = new QCameraImageCapture(m_camera);
    m_mediaRecoder = new QMediaRecorder(m_camera);

    QVideoEncoderSettings videoSettings = m_mediaRecoder->videoSettings();
    videoSettings.setCodec("video/x-vp8");
    QAudioEncoderSettings audioSettings = m_mediaRecoder->audioSettings();
    m_mediaRecoder->setEncodingSettings(audioSettings, videoSettings, "video/webm");

    m_camera->setCaptureMode(QCamera::CaptureStillImage);
    m_camera->setViewfinder(m_videoSurface);

    m_camera->start();

    // 同步设备名称到config
    camConfig.device_name = strdup(cameraInfo.description().toStdString().c_str());
    camConfig.device_location = strdup(cameraInfo.deviceName().toStdString().c_str());

    QList<QSize> supportList = m_imageCapture->supportedResolutions();

    // 当前设备与config设备名不同，重置分辨率到最大值，并同步到config文件
    QString configDevName = QString(camConfig.device_name);
    if (m_curDevName != configDevName
            || (m_curDevName.isEmpty() && m_curDevName == configDevName)) {
        if (!supportList.isEmpty()) {
            camConfig.width = supportList.last().rwidth();
            camConfig.height = supportList.last().rheight();
        }
    }

    // 若当前摄像头不支持该分辨率，重置为当前摄像头最大分辨率
    if (!supportList.isEmpty()) {
        bool bResetResolution = true;
        for (int i = 0; i < supportList.size(); i++) {
            if (supportList[i].width() == camConfig.width &&
                    supportList[i].height() == camConfig.height) {
                bResetResolution = false;
                break;
            }
        }
        if (bResetResolution) {
            camConfig.width = supportList.last().width();
            camConfig.height = supportList.last().height();
        }
    }

    // 同步config分辨率到相机，并保存到文件
    setCameraResolution(QSize(camConfig.width, camConfig.height));

    // 发送相机名称变更信号
    emit cameraSwitched(cameraInfo.description());
}

void Camera::stopCamera()
{
    if (m_camera) {
        connect(m_camera, SIGNAL(statusChanged(QCamera::Status)), this, SLOT(onCameraStatusChanged(QCamera::Status)));
        m_camera->stop();
        m_camera->deleteLater();
        m_camera = nullptr;
    }

    if (m_imageCapture) {
        m_imageCapture->deleteLater();
        m_imageCapture = nullptr;
    }

    if (m_mediaRecoder) {
        m_mediaRecoder->deleteLater();
        m_mediaRecoder = nullptr;
    }
}

void Camera::setCaptureImage()
{
    m_camera->setCaptureMode(QCamera::CaptureStillImage);
}

void Camera::setCaptureVideo()
{
    m_camera->setCaptureMode(QCamera::CaptureVideo);
}

void Camera::setVideoOutPutPath(QString &path)
{
    m_mediaRecoder->setOutputLocation(QUrl::fromLocalFile(path));
}

void Camera::startRecoder()
{
    m_bReadyRecord = true;
    if (m_camera->captureMode() != QCamera::CaptureVideo) {
        m_camera->setCaptureMode(QCamera::CaptureVideo);
    }
}

void Camera::stopRecoder()
{
    m_mediaRecoder->stop();
    if (m_camera->captureMode() != QCamera::CaptureStillImage)
        m_camera->setCaptureMode(QCamera::CaptureStillImage);
}

qint64 Camera::getRecoderTime()
{
    return m_mediaRecoder->duration();
}

QMediaRecorder::State Camera::getRecoderState()
{
    return  m_mediaRecoder->state();
}

bool Camera::isReadyRecord()
{
    return m_bReadyRecord;
}

int Camera::parseConfig()
{
    char *config_path = smart_cat(getenv("HOME"), '/', ".config/deepin/deepin-camera");
    mkdir(config_path, 0777);
    char *config_file = smart_cat(config_path, '/', "deepin-camera");
    /*释放字符串内存*/
    free(config_path);

    FILE *fp;
    char bufr[MAXLINE];
    int line = 0;

    /*open file for read*/
    if((fp = fopen(config_file,"r")) == NULL)
    {
        fprintf(stderr, "deepin-camera: couldn't open %s for read: %s\n", config_path, strerror(errno));
        return -1;
    }
    while(fgets(bufr, MAXLINE, fp) != NULL)
    {
        line++;
        char *bufp = bufr;

        /*trim leading and trailing spaces and newline*/
        trim_leading_wspaces(bufp);
        trim_trailing_wspaces(bufp);

        /*skip empty or commented lines */
        int size = strlen(bufp);
        if(size < 1 || *bufp == '#')
        {
            continue;
        }

        char *token = NULL;
        char *value = NULL;

        char *sp = strrchr(bufp, '=');

        if(sp)
        {
            long size = sp - bufp;
            token = strndup(bufp, (ulong)size);
            trim_leading_wspaces(token);
            trim_trailing_wspaces(token);

            value = strdup(sp + 1);
            trim_leading_wspaces(value);
            trim_trailing_wspaces(value);
        }

        /*skip invalid lines */
        if(!token || !value || strlen(token) < 1 || strlen(value) < 1)
        {
            fprintf(stderr, "deepin-camera: (config) skiping invalid config entry at line %i\n", line);
            if(token)
                free(token);
            if(value)
                free(value);
            continue;
        }

        /*check tokens*/
        if(strcmp(token, "width") == 0) {
            camConfig.width = (int) strtoul(value, NULL, 10);
        } else if(strcmp(token, "height") == 0) {
            camConfig.height = (int) strtoul(value, NULL, 10);
        } else if(strcmp(token, "device_name") == 0 && strlen(value) > 0) {
            if(camConfig.device_name)
                free(camConfig.device_name);
            camConfig.device_name = strdup(value);
        } else if(strcmp(token, "device_location") == 0 && strlen(value) > 0) {
            if(camConfig.device_location)
                free(camConfig.device_location);
            camConfig.device_location = strdup(value);
        }
    }
    fclose(fp);
    return 0;
}

int Camera::saveConfig()
{
    QString config_file = QString(getenv("HOME")) + QDir::separator() + QString(".config") + QDir::separator() + QString("deepin") +
                          QDir::separator() + QString("deepin-camera") + QDir::separator() + QString("deepin-camera");

    const char *filename = config_file.toLatin1().data();
    FILE *fp;

    /*open file for write*/
    if((fp = fopen(filename,"w")) == NULL)
    {
        fprintf(stderr, "deepin-camera: couldn't open %s for write: %s\n", filename, strerror(errno));
        return -1;
    }

    /* use c locale - make sure floats are writen with a "." and not a "," */
    setlocale(LC_NUMERIC, "C");

    /*write config data*/
    fprintf(fp, "\n");
    fprintf(fp, "#video input width\n");
    fprintf(fp, "width=%i\n", camConfig.width);
    fprintf(fp, "#video input height\n");
    fprintf(fp, "height=%i\n", camConfig.height);
    fprintf(fp, "#device name\n");
    fprintf(fp, "device_name=%s\n",camConfig.device_name);
    fprintf(fp, "#device location\n");
    fprintf(fp, "device_location=%s\n",camConfig.device_location);
    fprintf(fp, "#video name\n");
    fprintf(fp, "video_name=%s\n", camConfig.video_name);
    fprintf(fp, "#video path\n");
    fprintf(fp, "video_path=%s\n", camConfig.video_path);
    fprintf(fp, "#photo name\n");
    fprintf(fp, "photo_name=%s\n", camConfig.photo_name);
    fprintf(fp, "#photo path\n");
    fprintf(fp, "photo_path=%s\n", camConfig.photo_path);
    fprintf(fp, "#fps numerator (def. 1)\n");
    fprintf(fp, "fps_num=%i\n", camConfig.fps_num);
    fprintf(fp, "#fps denominator (def. 25)\n");
    fprintf(fp, "fps_denom=%i\n", camConfig.fps_denom);
    fprintf(fp, "#audio device index (-1 - api default)\n");

    /* return to system locale */
    setlocale(LC_NUMERIC, "");

    /* flush stream buffers to filesystem */
    fflush(fp);

    /* close file after fsync (sync file data to disk) */
    if (fsync(fileno(fp)) || fclose(fp))
    {
        fprintf(stderr, "deeepin_camera: error writing configuration data to file: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}
