/*
* Copyright (C) 2020 ~ %YEAR% Uniontech Software Technology Co.,Ltd.
*
* Author:     shicetu <shicetu@uniontech.com>
*             hujianbo <hujianbo@uniontech.com>
* Maintainer: shicetu <shicetu@uniontech.com>
*             hujianbo <hujianbo@uniontech.com>
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

#include "mainwindow.h"

#include <QListWidgetItem>
#include <DLabel>
#include <QFileDialog>
#include <QStandardPaths>
#include <DSettingsDialog>
#include <DSettingsOption>
#include <DSettings>
#include <DLineEdit>
#include <DFileDialog>
#include <DDialog>
#include <QTextLayout>
#include <QStyleFactory>
#include <dsettingswidgetfactory.h>

DSettings *sDsetWgt;

CMainWindow::CMainWindow(DWidget *w): DMainWindow (w)
{
    m_devnumMonitor = new DevNumMonitor();
    m_devnumMonitor->start();
    QString m_strPath;
    m_strPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Pictures/摄像头";
    m_fileWatcher.addPath(m_strPath);
    m_strPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Videos/摄像头";
    m_fileWatcher.addPath(m_strPath);

    initUI();
    initTitleBar();
    initConnection();
}

DSettings *CMainWindow::getDsetMber()
{
    return pDSettings;
}

CMainWindow::~CMainWindow()
{
}

static QString lastOpenedPath()
{

    if (!sDsetWgt) {
        sDsetWgt = new DSettings;
    }
    QString lastPath = sDsetWgt->getOption("base.general.last_open_path").toString();
    QDir lastDir(lastPath);
    if (lastPath.isEmpty() || !lastDir.exists()) {
        lastPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        QDir newLastDir(lastPath);
        if (!newLastDir.exists()) {
            lastPath = QDir::currentPath();
        }
    }
    return lastPath;
}

/*
QTextOption::WrapMode:描述text以什么方式显示在文档中



*/


static QString ElideText(const QString &text, const QSize &size,
                         QTextOption::WrapMode wordWrap, const QFont &font,
                         Qt::TextElideMode mode, int lineHeight, int lastLineWidth)
{
    int height = 0;

    QTextLayout textLayout(text);
    QString str = nullptr;
    QFontMetrics fontMetrics(font);

    textLayout.setFont(font);
    const_cast<QTextOption *>(&textLayout.textOption())->setWrapMode(wordWrap);

    textLayout.beginLayout();

    QTextLine line = textLayout.createLine();

    while (line.isValid()) {
        height += lineHeight;

        if (height + lineHeight >= size.height()) {
            str += fontMetrics.elidedText(text.mid(line.textStart() + line.textLength() + 1),
                                          mode, lastLineWidth);

            break;
        }

        line.setLineWidth(size.width());

        const QString &tmp_str = text.mid(line.textStart(), line.textLength());

        if (tmp_str.indexOf('\n'))
            height += lineHeight;

        str += tmp_str;

        line = textLayout.createLine();

        if (line.isValid())
            str.append("\n");
    }

    textLayout.endLayout();

    if (textLayout.lineCount() == 1) {
        str = fontMetrics.elidedText(str, mode, lastLineWidth);
    }

    return str;
}

static void workaround_updateStyle(QWidget *parent, const QString &theme)
{
    parent->setStyle(QStyleFactory::create(theme));
    for (auto obj : parent->children()) {
        auto w = qobject_cast<QWidget *>(obj);
        if (w) {
            workaround_updateStyle(w, theme);
        }
    }
}

static QWidget *createSelectableLineEditOptionHandle(QObject *opt)
{
    auto option = qobject_cast<DTK_CORE_NAMESPACE::DSettingsOption *>(opt);

    auto le = new DLineEdit();
    auto main = new DWidget;
    auto layout = new QHBoxLayout;

    static QString nameLast = nullptr;

    main->setLayout(layout);
    DPushButton *icon = new DPushButton;
    icon->setAutoDefault(false);
    le->setFixedHeight(30);
    le->setObjectName("OptionSelectableLineEdit");
    //QString str = option->value().toString();
    le->setText(option->value().toString());
    auto fm = le->fontMetrics();
    auto pe = ElideText(le->text(), {285, fm.height()}, QTextOption::WrapAnywhere,
                        le->font(), Qt::ElideMiddle, fm.height(), 285);
    option->connect(le, &DLineEdit::focusChanged, [ = ](bool on) {
        if (on)
            le->setText(option->value().toString());
    });
    le->setText(pe);
    nameLast = pe;

    icon->setIcon(QIcon(":resources/icons/select-normal.svg"));
    icon->setFixedHeight(30);
    layout->addWidget(le);
    layout->addWidget(icon);

    auto optionWidget = DSettingsWidgetFactory::createTwoColumWidget(option, main);
    workaround_updateStyle(optionWidget, "light");

    DDialog *prompt = new DDialog(optionWidget);
    prompt->setIcon(QIcon(":/resources/icons/warning.svg"));
    prompt->setMessage(QObject::tr("You don't have permission to operate this folder"));
    prompt->setWindowFlags(prompt->windowFlags() | Qt::WindowStaysOnTopHint);
    prompt->addButton(QObject::tr("OK"), true, DDialog::ButtonRecommend);

    auto validate = [ = ](QString name, bool alert = true) -> bool {
        name = name.trimmed();
        if (name.isEmpty()) return false;

        if (name.size() && name[0] == '~')
        {
            name.replace(0, 1, QDir::homePath());
        }

        QFileInfo fi(name);
        QDir dir(name);
        if (fi.exists())
        {
            if (!fi.isDir()) {
                if (alert) le->showAlertMessage(QObject::tr("Invalid folder"));
                return false;
            }

            if (!fi.isReadable() || !fi.isWritable()) {
                return false;
            }
        } else
        {
            if (dir.cdUp()) {
                QFileInfo ch(dir.path());
                if (!ch.isReadable() || !ch.isWritable())
                    return false;
            }
        }

        return true;
    };
    QString temstr = lastOpenedPath();
    option->connect(icon, &DPushButton::clicked, [ = ]() {
        QString name = DFileDialog::getExistingDirectory(0, QObject::tr("Open folder"),
                                                         lastOpenedPath(),
                                                         DFileDialog::ShowDirsOnly | DFileDialog::DontResolveSymlinks);
        if (validate(name, false)) {
            option->setValue(name);
            nameLast = name;
        }
        QFileInfo fm(name);
        if ((!fm.isReadable() || !fm.isWritable()) && !name.isEmpty()) {
            prompt->show();
        }
    });



    option->connect(le, &DLineEdit::editingFinished, option, [ = ]() {

        QString name = le->text();
        QDir dir(name);

        auto pn = ElideText(name, {285, fm.height()}, QTextOption::WrapAnywhere,
                            le->font(), Qt::ElideMiddle, fm.height(), 285);
        auto nmls = ElideText(nameLast, {285, fm.height()}, QTextOption::WrapAnywhere,
                              le->font(), Qt::ElideMiddle, fm.height(), 285);

        if (!validate(le->text(), false)) {
            QFileInfo fn(dir.path());
            if ((!fn.isReadable() || !fn.isWritable()) && !name.isEmpty()) {
                prompt->show();
            }
        }
        if (!le->lineEdit()->hasFocus()) {
            if (validate(le->text(), false)) {
                option->setValue(le->text());
                le->setText(pn);
                nameLast = name;
            } else if (pn == pe) {
                le->setText(pe);
            } else {
                option->setValue(nameLast);
                le->setText(nmls);
            }
        }
    });

    option->connect(le, &DLineEdit::textEdited, option, [ = ](const QString & newStr) {
        validate(newStr);
    });

    option->connect(option, &DTK_CORE_NAMESPACE::DSettingsOption::valueChanged, le,
    [ = ](const QVariant & value) {
        auto pi = ElideText(value.toString(), {285, fm.height()}, QTextOption::WrapAnywhere,
                            le->font(), Qt::ElideMiddle, fm.height(), 285);
        le->setText(pi);
        le->update();
    });

    return  optionWidget;
}

void CMainWindow::slotPopupSettingsDialog()
{
    pDSettingDialog = new DSettingsDialog(this);
    pDSettings = DSettings::fromJsonFile(":/resource/settings.json");
    sDsetWgt = getDsetMber();

    pDSettingDialog->widgetFactory()->registerWidget("selectableEdit", createSelectableLineEditOptionHandle);
    //创建设置存储后端
    //QSettingBackend *pBackend = new QSettingBackend(m_srConfPath);

    //通过json文件创建DSettings对象
//    pDSettings = DSettings::fromJsonFile(":/resource/settings.json");
//    sDsetWgt = getDsetMber();
    //设置DSettings存储后端
    //pDSettings->setBackend(pBackend);

    pDSettingDialog->updateSettings(pDSettings);
    pDSettingDialog->exec();
}

void CMainWindow::initUI()
{
    this->setWindowFlag(Qt::FramelessWindowHint);
    DWidget *wget = new DWidget;
    qDebug() << "initUI";
    QVBoxLayout *hboxlayout = new QVBoxLayout;

//    QPalette *pal = new QPalette(m_videoPre.palette());
//    pal->setColor(QPalette::Background, Qt::black); //设置黑色边框
//    m_videoPre.setAutoFillBackground(true);
//    m_videoPre.setPalette(*pal);
//    _animationlable = new AnimationLabel;
//    _animationlable->setAttribute(Qt::WA_TranslucentBackground);
//    _animationlable->setWindowFlags(Qt::FramelessWindowHint);
//    _animationlable->setParent(this);
//    _animationlable->setGeometry(width() / 2 - 100, height() / 2 - 100, 200, 200);

    //m_preWgt = new PreviewWidget(centralWidget());
    //hboxlayout->addWidget(&m_preWgt);
    hboxlayout->addWidget(&m_videoPre);
    hboxlayout->setContentsMargins(0, 0, 0, 0);



//    hboxlayout->addWidget(&m_toolBar);
    //hboxlayout->addWidget(&m_thumbnail,Qt::AlignBottom);

//    hboxlayout->addWidget(&m_thumbnail, Qt::AlignBottom);
//    m_thumbnail.setFixedHeight(100);

//    hboxlayout->setStretch(0, 16);
//    hboxlayout->setStretch(1, 1);
//    hboxlayout->setStretch(2, 3);

    wget->setLayout(hboxlayout);
    setCentralWidget(wget);

    setupTitlebar();
    m_thumbnail = new ThumbnailsBar(this);
    m_thumbnail->move(0, height() - 10);
    m_thumbnail->setFixedHeight(100);

    m_thumbnail->setVisible(true);
    m_thumbnail->setMaximumWidth(1200);
    this->resize(850, 600);
    //m_thumbnail->setFixedWidth(this->width());
}

void CMainWindow::initTitleBar()
{
    pDButtonBox = new DButtonBox();
    pDButtonBox->setFixedWidth(200);
    QList<DButtonBoxButton *> listButtonBox;
    pTakPictureBtn = new DButtonBoxButton(QStringLiteral("拍照"));
    pTakVideoBtn = new DButtonBoxButton(QStringLiteral("视频"));
    listButtonBox.append(pTakPictureBtn);
    listButtonBox.append(pTakVideoBtn);
    pDButtonBox->setButtonList(listButtonBox, false);
    titlebar()->addWidget(pDButtonBox);

    pSelectBtn = new DIconButton(nullptr/*DStyle::SP_IndicatorSearch*/);

    titlebar()->setIcon(QIcon::fromTheme("preferences-system"));// /usr/share/icons/bloom/apps/96
    titlebar()->addWidget(pSelectBtn, Qt::AlignLeft);
}

void CMainWindow::initConnection()
{
    //系统文件夹变化信号
    connect(&m_fileWatcher, SIGNAL(directoryChanged(const QString &)), m_thumbnail, SLOT(onFileChanged(const QString &)));
    //系统文件变化信号
    connect(&m_fileWatcher, SIGNAL(fileChanged(const QString &)), m_thumbnail, SLOT(onFileChanged(const QString &)));
//    actionToken     m_actToken;

//    avCodec         m_avCodec;
//    CameraDetect    m_camDetect;
//    effectproxy     m_effProxy;
//    ToolBar         m_toolBar;
    //拍照按钮信号
    connect(&m_toolBar, SIGNAL(sltPhoto()), &m_videoPre, SLOT(onBtnPhoto()));

    //三连拍按钮信号
    connect(&m_toolBar, SIGNAL(sltThreeShot()), &m_videoPre, SLOT(onBtnThreeShots()));

    //录像按钮信号
    connect(&m_toolBar, SIGNAL(sltVideo()), &m_videoPre, SLOT(onBtnVideo()));
    //特效按钮信号
    connect(&m_toolBar, SIGNAL(sltEffect()), &m_videoPre, SLOT(onBtnEffect()));


    //拍照信号
    connect(&m_toolBar, SIGNAL(takepic()), &m_actToken, SLOT(onTakePic()));

    //三连拍信号
    connect(&m_toolBar, SIGNAL(threeShots()), &m_actToken, SLOT(onThreeShots()));
    //录像信号
    connect(&m_toolBar, SIGNAL(takeVideo()), &m_actToken, SLOT(onTakeVideo()));
    //三连拍取消信号
    connect(&m_toolBar, SIGNAL(cancelThreeShots()), &m_actToken, SLOT(onCancelThreeShots()));
    //录像结束信号
    connect(&m_toolBar, SIGNAL(takeVideoOver()), &m_actToken, SLOT(onTakeVideoOver()));
    //拍照信号--显示倒计时
    connect(&m_toolBar, SIGNAL(takepic()), &m_videoPre, SLOT(onShowCountdown()));
    //三连拍信号--显示倒计时
    connect(&m_toolBar, SIGNAL(threeShots()), &m_videoPre, SLOT(onShowThreeCountdown()));
    //录像信号--显示计时
    connect(&m_toolBar, SIGNAL(takeVideo()), &m_videoPre, SLOT(onTShowTime()));
    //三连拍取消信号
    //connect(&m_toolBar, SIGNAL(cancelThreeShots()), &m_videoPre, SLOT(onCancelThreeShots()));
    //录像结束信号
    //connect(&m_toolBar, SIGNAL(takeVideoOver()), &m_videoPre, SLOT(onTakeVideoOver()));
    //选择特效信号
    connect(&m_toolBar, SIGNAL(chooseEffect()), &m_videoPre, SLOT(onChooseEffect()));
    //特效选择左边按钮
    connect(&m_toolBar, SIGNAL(moreEffectLeft()), &m_videoPre, SLOT(onMoreEffectLeft()));
    //特效选择右边按钮
    connect(&m_toolBar, SIGNAL(moreEffectRight()), &m_videoPre, SLOT(onMoreEffectRight()));
    //找不到设备信号
    connect(&m_videoPre, SIGNAL(disableButtons()), &m_toolBar, SLOT(set_btn_state_no_dev()));
    //正常按钮时能信号
    connect(&m_videoPre, SIGNAL(ableButtons()), &m_toolBar, SLOT(set_btn_state_wth_dev()));
    //结束占用按钮状态改变
    connect(&m_videoPre, SIGNAL(finishTakedCamera()), &m_toolBar, SLOT(onFinishTakedCamera()));
    //结束特效选择信号
    connect(&m_videoPre, SIGNAL(finishEffectChoose()), &m_toolBar, SLOT(onFinishEffectChoose()));
    //设备切换信号
    connect(pSelectBtn, SIGNAL(clicked()), &m_videoPre, SLOT(changeDev()));
    //单设备信号
    connect(m_devnumMonitor, SIGNAL(seltBtnStateEnable()), this, SLOT(setSelBtnShow()));
    //多设备信号
    connect(m_devnumMonitor, SIGNAL(seltBtnStateDisable()), this, SLOT(setSelBtnHide()));

}
void CMainWindow::setSelBtnHide()
{
    pSelectBtn->hide();
}

void CMainWindow::setSelBtnShow()
{
    pSelectBtn->show();
}

void CMainWindow::setupTitlebar()
{
    DMenu *menu = new DMenu();
    QAction *settingAction(new QAction(tr("Settings"), this));
    menu->addAction(settingAction);
    titlebar()->setMenu(menu);
    connect(settingAction, &QAction::triggered, this, &CMainWindow::settingsTriggered);


    m_setwidget = new Set_Widget(centralWidget());
    m_setwidget->setBackgroundRole(QPalette::Background);
    m_setwidget->setAutoFillBackground(true);
    QPalette *plette = new QPalette();

    plette->setBrush(QPalette::Background, QBrush(QColor(64, 64, 64, 180), Qt::SolidPattern));
    plette->setBrush(QPalette::WindowText, QBrush(QColor(255, 255, 255, 255), Qt::SolidPattern));
    m_setwidget->setPalette(*plette);

    connect(settingAction, &QAction::triggered, this, &CMainWindow::slotPopupSettingsDialog);

    //m_setwidget->update();
    //m_setwidget->setGeometry(0, 15 + m_setwidget->height(), this->width(), this->height() - m_setwidget->height());
}

void CMainWindow::resizeEvent(QResizeEvent *event)
{
    //Q_UNUSED(event);
    if (QEvent::Resize == event->type()) {
        int width = this->width();
        int height = this->height();
        //qDebug() << width << " " << height;
        m_videoPre.resize(width, height);
        if (m_thumbnail) {
            m_thumbnail->resize(/*qMin(width,TOOLBAR_MINIMUN_WIDTH)*/width, 100);
            m_thumbnail->move((this->width() - m_thumbnail->width()) / 2,
                              this->height() - m_thumbnail->height() - 5);
        }
    }

}

void CMainWindow::menuItemInvoked(QAction *action)
{

}

void CMainWindow::settingsTriggered(bool bTrue)
{
    m_setwidget->show();
}

void CMainWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_Escape) {
        m_setwidget->hide();
    }
    QWidget::keyReleaseEvent(ev);
}




