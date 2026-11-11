#include "TrajectoryOptimizationWorkbench.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace robot_qt_viewer
{
    TrajectoryOptimizationPanel::TrajectoryOptimizationPanel(QWidget* parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("trajectoryOptimizationPanel"));
        inputs = new QWidget(parent);
        inputs->setObjectName(QStringLiteral("trajectoryOptimizationInputs"));
        auto* left = new QVBoxLayout(inputs);
        auto* files = new QGroupBox(QStringLiteral("优化输入"), inputs);
        auto* form = new QFormLayout(files);
        auto addFile = [this, form, files](const QString& title, QLineEdit*& edit,
                       const QString& name, const QString& filter, bool directory) {
            auto* row = new QWidget(files);
            auto* layout = new QHBoxLayout(row); layout->setContentsMargins(0, 0, 0, 0);
            edit = new QLineEdit(row); edit->setObjectName(name);
            auto* button = new QPushButton(QStringLiteral("选择…"), row);
            layout->addWidget(edit, 1); layout->addWidget(button); form->addRow(title, row);
            connect(edit, &QLineEdit::textChanged, this, &TrajectoryOptimizationPanel::inputsChanged);
            connect(button, &QPushButton::clicked, this, [this, edit, title, filter, directory]() {
                const QString path = directory ? QFileDialog::getExistingDirectory(this, title, edit->text())
                    : QFileDialog::getOpenFileName(this, title, edit->text(), filter);
                if(!path.isEmpty()) edit->setText(path);
            });
        };
        addFile(QStringLiteral("STL工件"), modelPath, QStringLiteral("optimizationModelPath"), QStringLiteral("STL (*.stl *.STL)"), false);
        addFile(QStringLiteral("待优化轨迹"), trajectoryPath, QStringLiteral("optimizationTrajectoryPath"), QStringLiteral("轨迹 (*.txt *.csv *.json)"), false);
        addFile(QStringLiteral("结果父目录"), outputPath, QStringLiteral("optimizationOutputPath"), {}, true);
        outputPath->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+QStringLiteral("/RS2026/TrajectoryOptimization"));
        auto* note = new QLabel(QStringLiteral("支持TCP矩阵TXT（含时间）、工艺CSV和单轨迹JSON。TXT不含喷阀标志时，仅将长轴向扫描识别为喷涂；优先导入工艺CSV。\n本对象使用网页示例坐标：铜片正面Y=140 mm，高度Z=375～575 mm，管轴沿Z。"), files);
        note->setWordWrap(true); form->addRow(note); left->addWidget(files);

        auto* prediction = new QGroupBox(QStringLiteral("涂层分析参数与模型"), inputs);
        auto* pf = new QFormLayout(prediction);
        predictionModel = new QComboBox(prediction);
        predictionModel->addItem(QStringLiteral("Paper Gaussian + 热历史/遮挡"), QStringLiteral("paper_gaussian"));
        predictionModel->setObjectName(QStringLiteral("optimizationPredictionModel"));
        pf->addRow(QStringLiteral("预测模型"), predictionModel);
        shareCoating = new QCheckBox(QStringLiteral("共享涂层分析设置"), prediction); shareCoating->setChecked(true);
        shareCoating->setToolTip(QStringLiteral("每次点击优化时复制参数快照；不会随其他工作台后续调整而改变本次计算。"));
        useHistory = new QCheckBox(QStringLiteral("使用热历史修正"), prediction); useHistory->setChecked(true);
        useBvh = new QCheckBox(QStringLiteral("使用STL遮挡"), prediction); useBvh->setChecked(true);
        autoFlip = new QCheckBox(QStringLiteral("自动翻转背向法向"), prediction); autoFlip->setChecked(true);
        for(auto* check : {shareCoating, useHistory, useBvh, autoFlip}) {
            pf->addRow(check); connect(check, &QCheckBox::toggled, this, &TrajectoryOptimizationPanel::inputsChanged);
        }
        timeStep = new QDoubleSpinBox(prediction); timeStep->setDecimals(4); timeStep->setRange(.001, .05); timeStep->setValue(.02); timeStep->setSuffix(QStringLiteral(" s"));
        pf->addRow(QStringLiteral("独立计算积分步长"), timeStep);
        auto* params = new QLabel(QStringLiteral("高斯喷幅和热历史常数与涂层分析共用C++参数定义；参考喷距120 mm，计算使用轨迹实际喷距。专用轨迹框架：喷射+Z，主轴+Y。"), prediction);
        params->setWordWrap(true); pf->addRow(params);
        connect(shareCoating, &QCheckBox::toggled, this, [this](bool shared) {
            predictionModel->setEnabled(!shared); timeStep->setEnabled(!shared);
        });
        predictionModel->setEnabled(false); timeStep->setEnabled(false);
        connect(predictionModel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrajectoryOptimizationPanel::inputsChanged);
        connect(timeStep, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TrajectoryOptimizationPanel::inputsChanged);
        left->addWidget(prediction);

        auto* nativeNote = new QLabel(QStringLiteral("原生 C++ 计算：轨迹优化、厚度预测和绘图均在程序内部运行。"), inputs);
        nativeNote->setWordWrap(true); left->addWidget(nativeNote); left->addStretch();

        auto* right = new QVBoxLayout(this);
        auto* optimization = new QGroupBox(QStringLiteral("轨迹均匀性优化"), this);
        auto* of = new QFormLayout(optimization);
        object = new QComboBox(optimization); object->setObjectName(QStringLiteral("optimizationObject"));
        object->addItem(QStringLiteral("超迈管面焊接件（平面优化）"), QStringLiteral("chaomai.pipe_plate.plane"));
        object->addItem(QStringLiteral("超迈管面焊接件（全优化）"), QStringLiteral("chaomai.pipe_plate.full"));
        of->addRow(QStringLiteral("优化模型对象"), object);
        tolerance = new QDoubleSpinBox(optimization); tolerance->setObjectName(QStringLiteral("optimizationTolerance"));
        tolerance->setDecimals(2); tolerance->setRange(.1, 100); tolerance->setValue(10); tolerance->setSuffix(QStringLiteral(" %"));
        target = new QDoubleSpinBox(optimization); target->setObjectName(QStringLiteral("optimizationTargetThickness"));
        target->setDecimals(2); target->setRange(.01, 10000); target->setValue(165); target->setSuffix(QStringLiteral(" μm"));
        of->addRow(QStringLiteral("允许厚度偏差 ±"), tolerance); of->addRow(QStringLiteral("目标铜片厚度"), target);
        scope = new QLabel(optimization); scope->setWordWrap(true); of->addRow(scope);
        optimize = new QPushButton(QStringLiteral("优化轨迹"), optimization); optimize->setObjectName(QStringLiteral("optimizeTrajectoryButton"));
        compare = new QPushButton(QStringLiteral("计算优化前后厚度与均匀性并绘图"), optimization); compare->setObjectName(QStringLiteral("compareOptimizationButton"));
        cancel = new QPushButton(QStringLiteral("取消计算"), optimization);
        openOutput = new QPushButton(QStringLiteral("打开本次结果目录"), optimization);
        of->addRow(optimize); of->addRow(compare); of->addRow(cancel); of->addRow(openOutput); right->addWidget(optimization);
        progress = new QProgressBar(this); progress->setRange(0, 1000); right->addWidget(progress);
        status = new QLabel(this); status->setWordWrap(true); status->setObjectName(QStringLiteral("optimizationStatus")); right->addWidget(status);
        statistics = new QLabel(this); statistics->setWordWrap(true); statistics->setTextInteractionFlags(Qt::TextSelectableByMouse); right->addWidget(statistics);
        log = new QPlainTextEdit(this); log->setReadOnly(true); log->setMaximumBlockCount(1000); right->addWidget(log, 1);
        connect(optimize, &QPushButton::clicked, this, &TrajectoryOptimizationPanel::optimizeRequested);
        connect(compare, &QPushButton::clicked, this, &TrajectoryOptimizationPanel::compareRequested);
        connect(cancel, &QPushButton::clicked, this, &TrajectoryOptimizationPanel::cancelRequested);
        connect(openOutput, &QPushButton::clicked, this, &TrajectoryOptimizationPanel::openOutputRequested);
        connect(object, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrajectoryOptimizationPanel::inputsChanged);
        for(auto* spin : {target, tolerance}) connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TrajectoryOptimizationPanel::inputsChanged);
    }
}
