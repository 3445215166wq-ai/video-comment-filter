/**
 * VideoFilteringWidget.h
 * 视频筛选悬浮球控制面板 - 头文件
 * 
 * 基于 IApplicationController 接口实现，功能包含：
 * - 悬浮球（可拖拽、颜色状态切换）
 * - 快速操作条（鼠标悬停展开，包含开始/状态/展开按钮）
 * - 完整控制面板（模式选择、阈值调节、目标窗口设置等）
 * - 全局热键 F8（停止筛选）
 * - 丝滑动画过渡
 */

#ifndef VIDEOFILTERINGWIDGET_H
#define VIDEOFILTERINGWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 前向声明
class IApplicationController;

// ==================== 悬浮球主控件 ====================
class VideoFilteringWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor ballColor READ ballColor WRITE setBallColor NOTIFY ballColorChanged)

public:
    explicit VideoFilteringWidget(std::shared_ptr<IApplicationController> controller, 
                                   QWidget *parent = nullptr);
    ~VideoFilteringWidget();

    // 颜色属性
    QColor ballColor() const { return m_ballColor; }
    void setBallColor(const QColor &color);

signals:
    void ballColorChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 快速操作条按钮
    void onStartClicked();
    void onStatusClicked();
    void onExpandClicked();
    void onHotkeyStop();
    
    // 完整面板按钮
    void onStopClicked();
    void onSetTargetWindowClicked();
    void onOpenPositiveTemplateClicked();
    void onOpenNegativeTemplateClicked();
    void onOpenCommentFolderClicked();
    void onResetDefaultClicked();
    void onClosePanelClicked();
    
    // 模式和阈值变化
    void onFilterModeChanged(int mode);
    void onPositiveThresholdChanged(double value);
    void onNegativeThresholdChanged(double value);
    void onRecordDataChanged(bool checked);
    
    // 状态更新（从后台线程调度到主线程）
    void handleStateUpdate(int mode, float posThreshold, float negThreshold, 
                           bool isRunning, bool recordData);
    void handleLogMessage(const QString &message);

private:
    void setupUI();
    void setupAnimations();
    void setupConnections();
    void setupGlobalHotkey();
    void removeGlobalHotkey();
    
    // 状态切换
    void updateBallColor(bool isRunning);
    void expandQuickBar();
    void collapseQuickBar();
    void expandFullPanel();
    void collapseFullPanel();
    void collapseAll();
    
    // 绘制
    void drawBall(QPainter &painter);
    void drawQuickBar(QPainter &painter);
    
    // 控制器
    std::shared_ptr<IApplicationController> m_controller;
    
    // UI 组件 - 快速操作条
    QWidget *m_quickBar;
    QPushButton *m_startBtn;
    QPushButton *m_statusBtn;
    QPushButton *m_expandBtn;
    
    // UI 组件 - 完整面板
    QWidget *m_fullPanel;
    QButtonGroup *m_modeGroup;
    QRadioButton *m_positiveRadio;
    QRadioButton *m_negativeRadio;
    QRadioButton *m_bothRadio;
    QSlider *m_posThresholdSlider;
    QSlider *m_negThresholdSlider;
    QDoubleSpinBox *m_posThresholdSpin;
    QDoubleSpinBox *m_negThresholdSpin;
    QLineEdit *m_targetWindowEdit;
    QPushButton *m_setTargetBtn;
    QCheckBox *m_recordDataCheck;
    QPushButton *m_openPosTemplateBtn;
    QPushButton *m_openNegTemplateBtn;
    QPushButton *m_openCommentBtn;
    QPushButton *m_resetBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_closePanelBtn;
    QTextEdit *m_logTextEdit;
    QLabel *m_statusLabel;
    QPushButton* m_openSimilarityBtn;
    
    // 动画
    QPropertyAnimation *m_quickBarAnim;
    QPropertyAnimation *m_fullPanelAnim;
    QPropertyAnimation *m_ballColorAnim;
    QGraphicsOpacityEffect *m_quickBarOpacity;
    QGraphicsOpacityEffect *m_fullPanelOpacity;
    QPropertyAnimation *m_quickBarFadeAnim;
    QPropertyAnimation *m_fullPanelFadeAnim;
    
    // 状态
    QColor m_ballColor;
    bool m_isQuickBarExpanded;
    bool m_isFullPanelExpanded;
    bool m_isDragging;
    bool m_isRunning;
    QPoint m_dragStartPos;
    QPoint m_windowStartPos;
    QTimer *m_leaveTimer;
    
    // 尺寸常量
    static constexpr int BALL_SIZE = 60;
    static constexpr int QUICK_BAR_WIDTH = 180;
    static constexpr int QUICK_BAR_HEIGHT = 44;
    static constexpr int FULL_PANEL_WIDTH = 380;
    static constexpr int FULL_PANEL_HEIGHT = 640;
    static constexpr int ANIMATION_DURATION = 250;
    static constexpr int HOTKEY_ID = 1001;
    
    // 颜色常量
    const QColor COLOR_IDLE = QColor("#2196F3");      // 蓝色 - 空闲
    const QColor COLOR_RUNNING = QColor("#4CAF50");   // 绿色 - 运行中
};

// ==================== 快速操作按钮 ====================
class QuickActionButton : public QPushButton
{
    Q_OBJECT

public:
    explicit QuickActionButton(const QString &iconText, const QString &tooltip, 
                                QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_iconText;
    bool m_isHovered;
};

// ==================== 面板按钮（统一风格） ====================
class PanelButton : public QPushButton
{
    Q_OBJECT

public:
    enum ButtonStyle {
        Primary,    // 主要操作（蓝色）
        Secondary,  // 次要操作（灰色）
        Danger      // 危险操作（红色）
    };
    
    explicit PanelButton(const QString &text, ButtonStyle style = Secondary, 
                          QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    ButtonStyle m_style;
    bool m_isHovered;
};

#endif // VIDEOFILTERINGWIDGET_H
