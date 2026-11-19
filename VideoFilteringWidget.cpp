/**
 * VideoFilteringWidget.cpp
 * 视频筛选悬浮球控制面板 - 实现文件
 */
 // AI辅助生成：DeepSeek, 2026-4-20
#include "VideoFilteringWidget.h"
#include "IApplicationController.h"
#include <QApplication>
#include <QScreen>
#include <QStyle>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QMetaObject>
#include <QCloseEvent>

 // ==================== 常量定义 ====================
constexpr int BALL_SIZE = 60;
constexpr int QUICK_BAR_HEIGHT = 40;
constexpr int QUICK_BAR_WIDTH = 180;
constexpr int FULL_PANEL_WIDTH = 360;
constexpr int FULL_PANEL_HEIGHT = 580;
constexpr int ANIMATION_DURATION = 250;
constexpr int HOTKEY_ID = 1001;

const QColor COLOR_IDLE = QColor(33, 150, 243);
const QColor COLOR_RUNNING = QColor(76, 175, 80);

// ==================== QuickActionButton 实现 ====================

/**
 * @brief 快速操作按钮构造函数
 * @param iconText 按钮显示的图标文字
 * @param tooltip 鼠标悬浮提示
 * @param parent 父控件
 */
QuickActionButton::QuickActionButton(const QString& iconText, const QString& tooltip,
    QWidget* parent)
    : QPushButton(parent)
    , m_iconText(iconText)
    , m_isHovered(false)
{
    setFixedSize(36, 36);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tooltip);
}

void QuickActionButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景
    QColor bgColor = m_isHovered ? QColor(255, 255, 255, 60) : QColor(255, 255, 255, 30);
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 8, 8);

    // 图标文字
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect(), Qt::AlignCenter, m_iconText);
}

void QuickActionButton::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
        m_isHovered = true;
    update();
}

void QuickActionButton::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
        m_isHovered = false;
    update();
}

// ==================== PanelButton 实现 ====================

/**
 * @brief 面板按钮构造函数
 * @param text 按钮文字
 * @param style 按钮样式（主色/危险/次要）
 * @param parent 父控件
 */
PanelButton::PanelButton(const QString& text, ButtonStyle style, QWidget* parent)
    : QPushButton(text, parent)
    , m_style(style)
    , m_isHovered(false)
{
    setFixedHeight(32);
    setCursor(Qt::PointingHandCursor);
    setFont(QFont("Microsoft YaHei UI", 9));
}

void PanelButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 根据样式选择颜色
    QColor baseColor, textColor;
    switch (m_style) {
    case Primary:
        baseColor = m_isHovered ? QColor("#1976D2") : QColor("#2196F3");
        textColor = Qt::white;
        break;
    case Danger:
        baseColor = m_isHovered ? QColor("#D32F2F") : QColor("#F44336");
        textColor = Qt::white;
        break;
    case Secondary:
    default:
        baseColor = m_isHovered ? QColor("#E0E0E0") : QColor("#F5F5F5");
        textColor = QColor("#333333");
        break;
    }

    // 绘制背景
    painter.setBrush(baseColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 6, 6);

    // 绘制文字
    painter.setPen(textColor);
    painter.drawText(rect(), Qt::AlignCenter, text());
}

void PanelButton::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
        m_isHovered = true;
    update();
}

void PanelButton::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
        m_isHovered = false;
    update();
}

// ==================== VideoFilteringWidget 实现 ====================

/**
 * @brief 悬浮球主窗口构造函数
 * @param controller 业务控制器
 * @param parent 父窗口
 */
VideoFilteringWidget::VideoFilteringWidget(std::shared_ptr<IApplicationController> controller,
    QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_controller(controller)
    , m_ballColor(COLOR_IDLE)
    , m_isQuickBarExpanded(false)
    , m_isFullPanelExpanded(false)
    , m_isDragging(false)
    , m_isRunning(false)
{
    // 设置窗口属性
    setAttribute(Qt::WA_TranslucentBackground);

    // 初始大小（仅悬浮球）
    setFixedSize(BALL_SIZE, BALL_SIZE);

    // 移动到屏幕右下角
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        move(screenGeometry.right() - BALL_SIZE - 30,
            screenGeometry.bottom() - BALL_SIZE - 100);
    }

    setupUI();
    setupAnimations();
    setupConnections();
    setupGlobalHotkey();

    // 初始化离开检测定时器
    m_leaveTimer = new QTimer(this);
    m_leaveTimer->setSingleShot(true);
    m_leaveTimer->setInterval(300);
    connect(m_leaveTimer, &QTimer::timeout, this, [this]() {
        if (!m_isFullPanelExpanded && !this->underMouse()) {
            collapseQuickBar();
        }
        });
}

/**
 * @brief 初始化所有UI控件：悬浮球、快捷栏、设置面板
 */
void VideoFilteringWidget::setupUI()
{
    // ========== 快速操作条 ==========
    m_quickBar = new QWidget(this);
    m_quickBar->setGeometry(BALL_SIZE - 10, (BALL_SIZE - QUICK_BAR_HEIGHT) / 2, 0, QUICK_BAR_HEIGHT);
    m_quickBar->setVisible(false);

    m_quickBarOpacity = new QGraphicsOpacityEffect(m_quickBar);
    m_quickBarOpacity->setOpacity(0);
    m_quickBar->setGraphicsEffect(m_quickBarOpacity);

    QHBoxLayout* quickLayout = new QHBoxLayout(m_quickBar);
    quickLayout->setContentsMargins(18, 0, 18, 0);
    quickLayout->setSpacing(16);

    m_startBtn = new QuickActionButton("▶", "开始筛选", m_quickBar);
    m_statusBtn = new QuickActionButton("i", "查看状态", m_quickBar);
    m_expandBtn = new QuickActionButton("⚙", "展开设置", m_quickBar);

    quickLayout->addWidget(m_startBtn);
    quickLayout->addWidget(m_statusBtn);
    quickLayout->addWidget(m_expandBtn);
    quickLayout->addStretch();

    // ========== 完整面板 ==========
    m_fullPanel = new QWidget(this);
    m_fullPanel->setGeometry(0, BALL_SIZE, FULL_PANEL_WIDTH, 0);
    m_fullPanel->setVisible(false);
    m_fullPanel->setStyleSheet(R"(
    /* === 全局文字颜色 === */
    * {
        color: #333333;
    }

    QWidget#fullPanel {
        background-color: white;
        border-radius: 12px;
    }

    QGroupBox {
        font-weight: bold;
        border: 1px solid #E0E0E0;
        border-radius: 8px;
        margin-top: 14px;
        padding-top: 12px;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        left: 12px;
        padding: 0 6px;
        color: #333333;
    }

    QRadioButton {
        spacing: 6px;
        color: #333333;
    }
    QSlider {
    min-height: 26px;
    }
    QSlider::groove:horizontal {
        height: 6px;
        background: #E0E0E0;
        border-radius: 3px;
    }
    QSlider::handle:horizontal {
        width: 16px;
        height: 16px;
        margin: -5px 0;
        background: #2196F3;
        border-radius: 8px;
    }
    QSlider::sub-page:horizontal {
        background: #2196F3;
        border-radius: 3px;
    }

    QLineEdit {
        border: 1px solid #E0E0E0;
        border-radius: 6px;
        padding: 6px 10px;
        color: #333333;
        background: white;
    }
    QLineEdit:focus {
        border-color: #2196F3;
    }

    QCheckBox {
        spacing: 8px;
        color: #333333;
    }

    QTextEdit {
        border: 1px solid #E0E0E0;
        border-radius: 6px;
        background: #FAFAFA;
        color: #333333;
        font-family: Consolas, monospace;
        font-size: 11px;
    }

    QLabel {
        color: #333333;
    }

    QDoubleSpinBox {
        color: #333333;
        background: white;
        border: 1px solid #E0E0E0;
        border-radius: 4px;
        padding: 2px 4px;
    }

    QComboBox {
        color: #333333;
        background: white;
        border: 1px solid #E0E0E0;
        border-radius: 4px;
        padding: 4px 8px;
    }
QDoubleSpinBox::up-button { width: 0; }
QDoubleSpinBox::down-button { width: 0; }
QDoubleSpinBox::up-arrow { image: none; }
QDoubleSpinBox::down-arrow { image: none; }
QDoubleSpinBox {
    border: 1px solid #E0E0E0;
    border-radius: 6px;
    padding: 4px 8px;
    background-color: #ffffff;
}
)");
    m_fullPanel->setObjectName("fullPanel");

    m_fullPanelOpacity = new QGraphicsOpacityEffect(m_fullPanel);
    m_fullPanelOpacity->setOpacity(0);
    m_fullPanel->setGraphicsEffect(m_fullPanelOpacity);

    QVBoxLayout* panelLayout = new QVBoxLayout(m_fullPanel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(12);

    // 标题栏
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel("视频筛选控制", m_fullPanel);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #333;");
    m_statusLabel = new QLabel("空闲", m_fullPanel);
    m_statusLabel->setStyleSheet("color: #666; font-size: 12px;");
    m_closePanelBtn = new QPushButton("×", m_fullPanel);
    m_closePanelBtn->setFixedSize(28, 28);
    m_closePanelBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            font-size: 20px;
            color: #999;
        }
        QPushButton:hover {
            color: #333;
            background: #F0F0F0;
            border-radius: 14px;
        }
    )");
    m_closePanelBtn->setCursor(Qt::PointingHandCursor);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_statusLabel);
    titleLayout->addWidget(m_closePanelBtn);
    panelLayout->addLayout(titleLayout);

    // 模式选择
    QGroupBox* modeGroup = new QGroupBox("筛选模式", m_fullPanel);
    QHBoxLayout* modeLayout = new QHBoxLayout(modeGroup);
    m_modeGroup = new QButtonGroup(this);
    m_positiveRadio = new QRadioButton("正向", modeGroup);
    m_negativeRadio = new QRadioButton("负向", modeGroup);
    m_bothRadio = new QRadioButton("双向", modeGroup);
    m_modeGroup->addButton(m_positiveRadio, 1);
    m_modeGroup->addButton(m_negativeRadio, 2);
    m_modeGroup->addButton(m_bothRadio, 3);
    modeLayout->addWidget(m_positiveRadio);
    modeLayout->addWidget(m_negativeRadio);
    modeLayout->addWidget(m_bothRadio);
    modeLayout->addStretch();
    panelLayout->addWidget(modeGroup);

    // 阈值调节
    QGroupBox* thresholdGroup = new QGroupBox("阈值设置", m_fullPanel);
    QGridLayout* thresholdLayout = new QGridLayout(thresholdGroup);
    thresholdLayout->setSpacing(8);

    // 正阈值
    QLabel* posLabel = new QLabel("正阈值:", thresholdGroup);
    m_posThresholdSlider = new QSlider(Qt::Horizontal, thresholdGroup);
    m_posThresholdSlider->setRange(0, 100);
    m_posThresholdSlider->setValue(70);
    m_posThresholdSpin = new QDoubleSpinBox(thresholdGroup);
    m_posThresholdSpin->setRange(0.0, 1.0);
    m_posThresholdSpin->setSingleStep(0.05);
    m_posThresholdSpin->setDecimals(2);
    m_posThresholdSpin->setValue(0.70);
    m_posThresholdSpin->setFixedWidth(80);

    QPushButton* btnUp = new QPushButton("▲", thresholdGroup);
    btnUp->setFixedSize(22, 18);
    btnUp->setStyleSheet(R"(
    QPushButton {
        border: none;
        background: #F0F0F0;
        font-size: 14px;
        color: #333;
        border-radius: 3px;
    }
    QPushButton:hover {
        background: #DCDCDC;
    }
)");

    QPushButton* btnDown = new QPushButton("▼", thresholdGroup);
    btnDown->setFixedSize(22, 18);
    btnDown->setStyleSheet(R"(
    QPushButton {
        border: none;
        background: #F0F0F0;
        font-size: 14px;
        color: #333;
        border-radius: 3px;
    }
    QPushButton:hover {
        background: #DCDCDC;
    }
)");

    connect(btnUp, &QPushButton::clicked, [this]() {
        m_posThresholdSpin->stepUp();
        });
    connect(btnDown, &QPushButton::clicked, [this]() {
        m_posThresholdSpin->stepDown();
        });

    QHBoxLayout* spinLayout = new QHBoxLayout();
    spinLayout->addWidget(m_posThresholdSpin);
    spinLayout->addWidget(btnUp);
    spinLayout->addWidget(btnDown);
    thresholdLayout->addWidget(posLabel, 0, 0);
    thresholdLayout->addWidget(m_posThresholdSlider, 0, 1);
    thresholdLayout->addLayout(spinLayout, 0, 2);

    // 负阈值
    QLabel* negLabel = new QLabel("负阈值:", thresholdGroup);
    m_negThresholdSlider = new QSlider(Qt::Horizontal, thresholdGroup);
    m_negThresholdSlider->setRange(0, 100);
    m_negThresholdSlider->setValue(80);
    m_negThresholdSpin = new QDoubleSpinBox(thresholdGroup);
    m_negThresholdSpin->setRange(0.0, 1.0);
    m_negThresholdSpin->setSingleStep(0.05);
    m_negThresholdSpin->setDecimals(2);
    m_negThresholdSpin->setValue(0.80);
    m_negThresholdSpin->setFixedWidth(80);

    QPushButton* btnUp2 = new QPushButton("▲", thresholdGroup);
    btnUp2->setFixedSize(22, 18);
    btnUp2->setStyleSheet(R"(
    QPushButton {
        border: none;
        background: #F0F0F0;
        font-size: 14px;
        color: #333;
        border-radius: 3px;
    }
    QPushButton:hover {
        background: #DCDCDC;
    }
)");

    QPushButton* btnDown2 = new QPushButton("▼", thresholdGroup);
    btnDown2->setFixedSize(22, 18);
    btnDown2->setStyleSheet(R"(
    QPushButton {
        border: none;
        background: #F0F0F0;
        font-size: 14px;
        color: #333;
        border-radius: 3px;
    }
    QPushButton:hover {
        background: #DCDCDC;
    }
)");

    connect(btnUp2, &QPushButton::clicked, [this]() {
        m_negThresholdSpin->stepUp();
        });
    connect(btnDown2, &QPushButton::clicked, [this]() {
        m_negThresholdSpin->stepDown();
        });

    QHBoxLayout* spinLayout2 = new QHBoxLayout();
    spinLayout2->addWidget(m_negThresholdSpin);
    spinLayout2->addWidget(btnUp2);
    spinLayout2->addWidget(btnDown2);
    thresholdLayout->addWidget(negLabel, 1, 0);
    thresholdLayout->addWidget(m_negThresholdSlider, 1, 1);
    thresholdLayout->addLayout(spinLayout2, 1, 2);

    panelLayout->addWidget(thresholdGroup);

    // 目标窗口设置
    QGroupBox* targetGroup = new QGroupBox("目标窗口", m_fullPanel);
    QHBoxLayout* targetLayout = new QHBoxLayout(targetGroup);
    m_targetWindowEdit = new QLineEdit(targetGroup);
    m_targetWindowEdit->setPlaceholderText("输入窗口标题关键字...");
    m_setTargetBtn = new PanelButton("设置", PanelButton::Primary, targetGroup);
    m_setTargetBtn->setFixedWidth(60);
    targetLayout->addWidget(m_targetWindowEdit);
    targetLayout->addWidget(m_setTargetBtn);
    panelLayout->addWidget(targetGroup);

    // 数据记录
    m_recordDataCheck = new QCheckBox("记录原始数据", m_fullPanel);
    panelLayout->addWidget(m_recordDataCheck);

    // 文件夹操作
    QGroupBox* folderGroup = new QGroupBox("文件夹管理", m_fullPanel);
    QGridLayout* folderLayout = new QGridLayout(folderGroup);
    folderLayout->setSpacing(8);

    m_openPosTemplateBtn = new PanelButton("打开正模板", PanelButton::Secondary, folderGroup);
    m_openNegTemplateBtn = new PanelButton("打开负模板", PanelButton::Secondary, folderGroup);
    m_openCommentBtn = new PanelButton("打开评论文件夹", PanelButton::Secondary, folderGroup);

    folderLayout->addWidget(m_openPosTemplateBtn, 0, 0);
    folderLayout->addWidget(m_openNegTemplateBtn, 0, 1);
    folderLayout->addWidget(m_openCommentBtn, 1, 0);

    m_openSimilarityBtn = new PanelButton("打开相似度报告", PanelButton::Secondary, folderGroup);
    m_openSimilarityBtn->setMinimumHeight(32);
    folderLayout->addWidget(m_openSimilarityBtn, 1, 1);

    panelLayout->addWidget(folderGroup);

    // 日志区域
    m_logTextEdit = new QTextEdit(m_fullPanel);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFixedHeight(80);
    m_logTextEdit->setPlaceholderText("日志输出...");
    panelLayout->addWidget(m_logTextEdit);

    // 底部按钮
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_resetBtn = new PanelButton("恢复默认", PanelButton::Secondary, m_fullPanel);
    m_stopBtn = new PanelButton("退出", PanelButton::Danger, m_fullPanel);
    bottomLayout->addWidget(m_resetBtn);
    bottomLayout->addStretch();
    bottomLayout->addWidget(m_stopBtn);
    panelLayout->addLayout(bottomLayout);

    panelLayout->addStretch();
}

/**
 * @brief F8热键触发：停止筛选
 */
void VideoFilteringWidget::onHotkeyStop()
{
    if (m_controller) {
        m_controller->StopFilteringLoop();
    }
}

/**
 * @brief 初始化所有动画：展开、收起、透明度、颜色
 */
void VideoFilteringWidget::setupAnimations()
{
    // 快速操作条展开动画
    m_quickBarAnim = new QPropertyAnimation(m_quickBar, "geometry", this);
    m_quickBarAnim->setDuration(ANIMATION_DURATION);
    m_quickBarAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 快速操作条透明度动画
    m_quickBarFadeAnim = new QPropertyAnimation(m_quickBarOpacity, "opacity", this);
    m_quickBarFadeAnim->setDuration(ANIMATION_DURATION);
    m_quickBarFadeAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 完整面板展开动画
    m_fullPanelAnim = new QPropertyAnimation(m_fullPanel, "geometry", this);
    m_fullPanelAnim->setDuration(ANIMATION_DURATION);
    m_fullPanelAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 完整面板透明度动画
    m_fullPanelFadeAnim = new QPropertyAnimation(m_fullPanelOpacity, "opacity", this);
    m_fullPanelFadeAnim->setDuration(ANIMATION_DURATION);
    m_fullPanelFadeAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 悬浮球颜色动画
    m_ballColorAnim = new QPropertyAnimation(this, "ballColor", this);
    m_ballColorAnim->setDuration(400);
    m_ballColorAnim->setEasingCurve(QEasingCurve::InOutQuad);
}

/**
 * @brief 绑定所有信号槽：按钮、滑块、输入、回调
 */
void VideoFilteringWidget::setupConnections()
{
    // 快速操作条按钮
    connect(m_startBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onStartClicked);
    connect(m_statusBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onStatusClicked);
    connect(m_expandBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onExpandClicked);

    // 完整面板按钮
    connect(m_openSimilarityBtn, &QPushButton::clicked, this, [this]() {
        if (m_controller) m_controller->OpenSimilarityCheckFolder();
        });
    connect(m_closePanelBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onClosePanelClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onStopClicked);
    connect(m_setTargetBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onSetTargetWindowClicked);
    connect(m_openPosTemplateBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onOpenPositiveTemplateClicked);
    connect(m_openNegTemplateBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onOpenNegativeTemplateClicked);
    connect(m_openCommentBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onOpenCommentFolderClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &VideoFilteringWidget::onResetDefaultClicked);

    // 模式切换
    connect(m_modeGroup, &QButtonGroup::idClicked, this, &VideoFilteringWidget::onFilterModeChanged);

    // 阈值滑块和数值框同步
    connect(m_posThresholdSlider, &QSlider::valueChanged, this, [this](int value) {
        double dValue = value / 100.0;
        m_posThresholdSpin->blockSignals(true);
        m_posThresholdSpin->setValue(dValue);
        m_posThresholdSpin->blockSignals(false);
        onPositiveThresholdChanged(dValue);
        });
    connect(m_posThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_posThresholdSlider->blockSignals(true);
        m_posThresholdSlider->setValue(static_cast<int>(value * 100));
        m_posThresholdSlider->blockSignals(false);
        onPositiveThresholdChanged(value);
        });

    connect(m_negThresholdSlider, &QSlider::valueChanged, this, [this](int value) {
        double dValue = value / 100.0;
        m_negThresholdSpin->blockSignals(true);
        m_negThresholdSpin->setValue(dValue);
        m_negThresholdSpin->blockSignals(false);
        onNegativeThresholdChanged(dValue);
        });
    connect(m_negThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_negThresholdSlider->blockSignals(true);
        m_negThresholdSlider->setValue(static_cast<int>(value * 100));
        m_negThresholdSlider->blockSignals(false);
        onNegativeThresholdChanged(value);
        });

    // 数据记录
    connect(m_recordDataCheck, &QCheckBox::toggled, this, &VideoFilteringWidget::onRecordDataChanged);

    // 订阅控制器状态变化（后台线程 → 主线程调度）
    if (m_controller) {
        m_controller->SubscribeStateChange([this](const IApplicationController::FilterState& state) {
            QMetaObject::invokeMethod(this, "handleStateUpdate",
                Qt::QueuedConnection,
                Q_ARG(int, state.mode),
                Q_ARG(float, state.positive_threshold),
                Q_ARG(float, state.negative_threshold),
                Q_ARG(bool, state.is_running),
                Q_ARG(bool, state.record_data));
            });

        m_controller->SubscribeMessage([this](const std::string& msg) {
            QString qmsg = QString::fromStdString(msg);
            QMetaObject::invokeMethod(this, "handleLogMessage",
                Qt::QueuedConnection,
                Q_ARG(QString, qmsg));
            });
    }
}

/**
 * @brief 注册Windows全局热键F8
 */
void VideoFilteringWidget::setupGlobalHotkey() {
#ifdef Q_OS_WIN
    if (m_controller) {
        bool ok = m_controller->RegisterGlobalHotKey((HWND)this->winId());
        handleLogMessage(ok ? "[INFO] 全局热键 F8 已注册" : "[WARN] 全局热键 F8 注册失败（可能被占用）");
    }
#endif
}

/**
 * @brief 卸载全局热键
 */
void VideoFilteringWidget::removeGlobalHotkey()
{
#ifdef Q_OS_WIN
    if (m_controller) {
        m_controller->UnregisterGlobalHotKey();
    }
#endif
}

/**
 * @brief 设置悬浮球颜色并刷新
 * @param color 目标颜色
 */
void VideoFilteringWidget::setBallColor(const QColor& color)
{
    if (m_ballColor != color) {
        m_ballColor = color;
        update();
        emit ballColorChanged();
    }
}

/**
 * @brief 更新悬浮球状态颜色（运行/空闲）
 * @param isRunning 是否正在运行
 */
void VideoFilteringWidget::updateBallColor(bool isRunning)
{
    m_ballColorAnim->stop();
    m_ballColorAnim->setStartValue(m_ballColor);
    m_ballColorAnim->setEndValue(isRunning ? COLOR_RUNNING : COLOR_IDLE);
    m_ballColorAnim->start();
}

/**
 * @brief 展开快捷操作栏
 */
void VideoFilteringWidget::expandQuickBar()
{
    if (m_isQuickBarExpanded || m_isFullPanelExpanded) return;

    m_isQuickBarExpanded = true;
    m_quickBar->setVisible(true);

    setFixedSize(BALL_SIZE + QUICK_BAR_WIDTH, BALL_SIZE);

    m_quickBarAnim->stop();
    m_quickBarAnim->setStartValue(QRect(BALL_SIZE, (BALL_SIZE - QUICK_BAR_HEIGHT) / 2, 0, QUICK_BAR_HEIGHT));
    m_quickBarAnim->setEndValue(QRect(BALL_SIZE, (BALL_SIZE - QUICK_BAR_HEIGHT) / 2, QUICK_BAR_WIDTH, QUICK_BAR_HEIGHT));
    m_quickBarAnim->start();

    m_quickBarFadeAnim->stop();
    m_quickBarFadeAnim->setStartValue(0.0);
    m_quickBarFadeAnim->setEndValue(1.0);
    m_quickBarFadeAnim->start();
}

/**
 * @brief 收起快捷操作栏
 */
void VideoFilteringWidget::collapseQuickBar()
{
    if (!m_isQuickBarExpanded) return;

    m_isQuickBarExpanded = false;

    m_quickBarAnim->stop();
    m_quickBarAnim->setStartValue(m_quickBar->geometry());
    m_quickBarAnim->setEndValue(QRect(BALL_SIZE, (BALL_SIZE - QUICK_BAR_HEIGHT) / 2, 0, QUICK_BAR_HEIGHT));
    m_quickBarAnim->start();

    m_quickBarFadeAnim->stop();
    m_quickBarFadeAnim->setStartValue(1.0);
    m_quickBarFadeAnim->setEndValue(0.0);
    m_quickBarFadeAnim->start();

    connect(m_quickBarAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_isQuickBarExpanded && !m_isFullPanelExpanded) {
            m_quickBar->setVisible(false);
            setFixedSize(BALL_SIZE, BALL_SIZE);
        }
        }, Qt::SingleShotConnection);
}

/**
 * @brief 展开完整设置面板
 */
void VideoFilteringWidget::expandFullPanel()
{
    if (m_isFullPanelExpanded) return;

    if (m_isQuickBarExpanded) {
        m_isQuickBarExpanded = false;
        m_quickBar->setVisible(false);
    }

    m_isFullPanelExpanded = true;
    m_fullPanel->setVisible(true);

    setFixedSize(FULL_PANEL_WIDTH, BALL_SIZE + FULL_PANEL_HEIGHT);

    m_fullPanelAnim->stop();
    m_fullPanelAnim->setStartValue(QRect(0, BALL_SIZE, FULL_PANEL_WIDTH, 0));
    m_fullPanelAnim->setEndValue(QRect(0, BALL_SIZE, FULL_PANEL_WIDTH, FULL_PANEL_HEIGHT));
    m_fullPanelAnim->start();

    m_fullPanelFadeAnim->stop();
    m_fullPanelFadeAnim->setStartValue(0.0);
    m_fullPanelFadeAnim->setEndValue(1.0);
    m_fullPanelFadeAnim->start();
}

/**
 * @brief 收起完整设置面板
 */
void VideoFilteringWidget::collapseFullPanel()
{
    if (!m_isFullPanelExpanded) return;

    m_isFullPanelExpanded = false;

    m_fullPanelAnim->stop();
    m_fullPanelAnim->setStartValue(m_fullPanel->geometry());
    m_fullPanelAnim->setEndValue(QRect(0, BALL_SIZE, FULL_PANEL_WIDTH, 0));
    m_fullPanelAnim->start();

    m_fullPanelFadeAnim->stop();
    m_fullPanelFadeAnim->setStartValue(1.0);
    m_fullPanelFadeAnim->setEndValue(0.0);
    m_fullPanelFadeAnim->start();

    connect(m_fullPanelAnim, &QPropertyAnimation::finished, this, [this]() {
        if (!m_isFullPanelExpanded) {
            m_fullPanel->setVisible(false);
            setFixedSize(BALL_SIZE, BALL_SIZE);
        }
        }, Qt::SingleShotConnection);
}

/**
 * @brief 收起所有展开的面板
 */
void VideoFilteringWidget::collapseAll()
{
    if (m_isFullPanelExpanded) {
        collapseFullPanel();
    }
    if (m_isQuickBarExpanded) {
        collapseQuickBar();
    }
}

// ==================== 事件处理 ====================

void VideoFilteringWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawBall(painter);

    if (m_isQuickBarExpanded) {
        drawQuickBar(painter);
    }

    if (m_isFullPanelExpanded) {
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        QRect panelRect(0, BALL_SIZE, FULL_PANEL_WIDTH, height() - BALL_SIZE);
        painter.drawRoundedRect(panelRect, 12, 12);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(0, 0, 0, 20), 1));
        painter.drawRoundedRect(panelRect.adjusted(0, 0, 0, 0), 12, 12);
    }
}

/**
 * @brief 绘制悬浮球主体（阴影、渐变、图标）
 */
void VideoFilteringWidget::drawBall(QPainter& painter)
{
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(4, 4, BALL_SIZE - 4, BALL_SIZE - 4);

    QRadialGradient gradient(BALL_SIZE / 2, BALL_SIZE / 2 - 5, BALL_SIZE / 2);
    gradient.setColorAt(0, m_ballColor.lighter(120));
    gradient.setColorAt(1, m_ballColor);
    painter.setBrush(gradient);
    painter.drawEllipse(2, 2, BALL_SIZE - 4, BALL_SIZE - 4);

    painter.setBrush(QColor(255, 255, 255, 80));
    painter.drawEllipse(12, 8, 20, 12);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);

    if (m_isRunning) {
        painter.drawRect(BALL_SIZE / 2 - 8, BALL_SIZE / 2 - 8, 16, 16);
    }
    else {
        QPainterPath triangle;
        triangle.moveTo(BALL_SIZE / 2 - 6, BALL_SIZE / 2 - 10);
        triangle.lineTo(BALL_SIZE / 2 + 10, BALL_SIZE / 2);
        triangle.lineTo(BALL_SIZE / 2 - 6, BALL_SIZE / 2 + 10);
        triangle.closeSubpath();
        painter.drawPath(triangle);
    }
}

/**
 * @brief 绘制快捷栏背景渐变
 */
void VideoFilteringWidget::drawQuickBar(QPainter& painter)
{
    QRect barRect(BALL_SIZE - 0, (BALL_SIZE - QUICK_BAR_HEIGHT) / 2,
        m_quickBar->width(), QUICK_BAR_HEIGHT);

    QLinearGradient gradient(barRect.left(), 0, barRect.right(), 0);
    gradient.setColorAt(0, m_ballColor);
    gradient.setColorAt(1, m_ballColor.darker(110));

    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);

    QPainterPath path;
    path.addRoundedRect(barRect, 22, 22);
    painter.drawPath(path);
}

void VideoFilteringWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        QRect ballRect(0, 0, BALL_SIZE, BALL_SIZE);
        if (ballRect.contains(event->pos())) {
            m_isDragging = true;
            m_dragStartPos = event->globalPosition().toPoint();
            m_windowStartPos = pos();

            if (m_isRunning && !m_isFullPanelExpanded) {
                onHotkeyStop();
                m_isDragging = false;
            }
        }
    }
    QWidget::mousePressEvent(event);
}

void VideoFilteringWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging) {
        QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
        move(m_windowStartPos + delta);
    }
    QWidget::mouseMoveEvent(event);
}

void VideoFilteringWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void VideoFilteringWidget::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event)
        m_leaveTimer->stop();

    if (!m_isRunning && !m_isFullPanelExpanded) {
        expandQuickBar();
    }
}

void VideoFilteringWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)

        if (!m_isFullPanelExpanded) {
            m_leaveTimer->start();
        }
}

/**
 * @brief 原生Windows事件处理：捕获热键消息
 */
bool VideoFilteringWidget::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == HOTKEY_ID) {
            if (m_isRunning) {
                onHotkeyStop();
            }
            *result = 0;
            return true;
        }
    }
#endif
    return QWidget::nativeEvent(eventType, message, result);
}

VideoFilteringWidget::~VideoFilteringWidget()
{
    removeGlobalHotkey();
    if (m_controller) {
        m_controller->StopFilteringLoop();
    }
    m_controller.reset();
}

void VideoFilteringWidget::closeEvent(QCloseEvent* event)
{
    removeGlobalHotkey();
    if (m_controller) {
        m_controller->StopFilteringLoop();
    }
    QWidget::closeEvent(event);
}

// ==================== 槽函数实现 ====================

/**
 * @brief 开始筛选按钮点击
 */
void VideoFilteringWidget::onStartClicked()
{
    if (!m_controller) return;

    auto state = m_controller->GetCurrentState();
    if (state.mode == 0) {
        QMessageBox::warning(this, "提示", "请先选择筛选模式");
        return;
    }

    m_controller->StartFilteringLoop();
    collapseAll();
}

/**
 * @brief 状态按钮点击：弹窗显示当前配置
 */
void VideoFilteringWidget::onStatusClicked()
{
    if (!m_controller) return;

    auto state = m_controller->GetCurrentState();

    QString modeStr;
    switch (state.mode) {
    case 1: modeStr = "正向"; break;
    case 2: modeStr = "负向"; break;
    case 3: modeStr = "双向"; break;
    default: modeStr = "未设置"; break;
    }

    QString info = QString(
        "当前状态\n\n"
        "筛选模式: %1\n"
        "正阈值: %2\n"
        "负阈值: %3\n"
        "运行状态: %4\n"
        "记录数据: %5"
    ).arg(modeStr)
        .arg(state.positive_threshold, 0, 'f', 2)
        .arg(state.negative_threshold, 0, 'f', 2)
        .arg(state.is_running ? "运行中" : "已停止")
        .arg(state.record_data ? "是" : "否");

    QMessageBox::information(this, "状态信息", info);
}

/**
 * @brief 展开设置面板
 */
void VideoFilteringWidget::onExpandClicked()
{
    expandFullPanel();
}

/**
 * @brief 完全退出程序
 */
void VideoFilteringWidget::onStopClicked()
{
    if (m_controller) {
        m_controller->StopFilteringLoop();
    }
    qApp->quit();
}

/**
 * @brief 设置目标窗口
 */
void VideoFilteringWidget::onSetTargetWindowClicked()
{
    if (!m_controller) return;

    QString keyword = m_targetWindowEdit->text().trimmed();
    if (keyword.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入窗口标题关键字");
        return;
    }

    m_controller->SetTargetWindow(keyword.toStdWString());
    m_logTextEdit->append(QString("已设置目标窗口: %1").arg(keyword));
}

void VideoFilteringWidget::onOpenPositiveTemplateClicked()
{
    if (m_controller) {
        m_controller->OpenTemplateFolder(true);
    }
}

void VideoFilteringWidget::onOpenNegativeTemplateClicked()
{
    if (m_controller) {
        m_controller->OpenTemplateFolder(false);
    }
}

void VideoFilteringWidget::onOpenCommentFolderClicked()
{
    if (m_controller) {
        m_controller->OpenCommentDataFolder();
    }
}

/**
 * @brief 重置为默认配置
 */
void VideoFilteringWidget::onResetDefaultClicked()
{
    if (m_controller) {
        m_controller->ResetToDefaultSettings();
        m_logTextEdit->append("已恢复默认设置");
    }
}

/**
 * @brief 关闭设置面板
 */
void VideoFilteringWidget::onClosePanelClicked()
{
    collapseFullPanel();
}

/**
 * @brief 筛选模式切换
 */
void VideoFilteringWidget::onFilterModeChanged(int mode)
{
    if (m_controller) {
        m_controller->SetFilterMode(mode);
    }
}

/**
 * @brief 正向阈值变化
 */
void VideoFilteringWidget::onPositiveThresholdChanged(double value)
{
    if (m_controller) {
        m_controller->SetPositiveThreshold(static_cast<float>(value));
    }
}

/**
 * @brief 负向阈值变化
 */
void VideoFilteringWidget::onNegativeThresholdChanged(double value)
{
    if (m_controller) {
        m_controller->SetNegativeThreshold(static_cast<float>(value));
    }
}

/**
 * @brief 数据记录开关变化
 */
void VideoFilteringWidget::onRecordDataChanged(bool checked)
{
    if (m_controller) {
        m_controller->SetRecordData(checked);
    }
}

/**
 * @brief 接收业务层状态更新，同步到UI
 */
void VideoFilteringWidget::handleStateUpdate(int mode, float posThreshold, float negThreshold,
    bool isRunning, bool recordData)
{
    // 更新模式
    switch (mode) {
    case 1: m_positiveRadio->setChecked(true); break;
    case 2: m_negativeRadio->setChecked(true); break;
    case 3: m_bothRadio->setChecked(true); break;
    default:
        m_modeGroup->setExclusive(false);
        m_positiveRadio->setChecked(false);
        m_negativeRadio->setChecked(false);
        m_bothRadio->setChecked(false);
        m_modeGroup->setExclusive(true);
        break;
    }

    // 更新阈值
    m_posThresholdSlider->blockSignals(true);
    m_posThresholdSpin->blockSignals(true);
    m_posThresholdSlider->setValue(static_cast<int>(posThreshold * 100));
    m_posThresholdSpin->setValue(posThreshold);
    m_posThresholdSlider->blockSignals(false);
    m_posThresholdSpin->blockSignals(false);

    m_negThresholdSlider->blockSignals(true);
    m_negThresholdSpin->blockSignals(true);
    m_negThresholdSlider->setValue(static_cast<int>(negThreshold * 100));
    m_negThresholdSpin->setValue(negThreshold);
    m_negThresholdSlider->blockSignals(false);
    m_negThresholdSpin->blockSignals(false);

    // 更新记录开关
    m_recordDataCheck->blockSignals(true);
    m_recordDataCheck->setChecked(recordData);
    m_recordDataCheck->blockSignals(false);

    // 更新运行状态
    if (m_isRunning != isRunning) {
        m_isRunning = isRunning;
        updateBallColor(isRunning);
        m_statusLabel->setText(isRunning ? "运行中" : "空闲");
        m_statusLabel->setStyleSheet(isRunning ?
            "color: #4CAF50; font-size: 12px; font-weight: bold;" :
            "color: #666; font-size: 12px;");

        if (isRunning) {
            collapseAll();
        }
    }
}

/**
 * @brief 输出日志到UI
 */
void VideoFilteringWidget::handleLogMessage(const QString& message)
{
    m_logTextEdit->append(message);
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}