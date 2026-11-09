#include "SectionSweepParameterPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace smrobot::workbench::freeform
{
    SectionSweepParameterPanel::SectionSweepParameterPanel(QWidget* parent):QWidget(parent)
    {
        setObjectName(QStringLiteral("sectionSweepParameters"));
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
        auto* group=new QGroupBox(QStringLiteral("等截面单侧扫描参数"),this);
        auto* form=new QFormLayout(group);
        const auto axes=[&](const QString& name,int initial) {
            auto* combo=new QComboBox(group);combo->setObjectName(name);
            combo->addItems({"+X","-X","+Y","-Y","+Z","-Z"});combo->setCurrentIndex(initial);
            connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){emit parametersEdited();});
            return combo;
        };
        m_scanAxis=axes("sectionScanAxis",4);m_sideAxis=axes("sectionSideAxis",2);
        form->addRow(QStringLiteral("扫描轴（管轴）"),m_scanAxis);
        form->addRow(QStringLiteral("喷涂侧（朝向喷枪）"),m_sideAxis);
        const auto spin=[&](const QString& name,const QString& label,double value,double minimum,
            const QString& suffix) {
            auto* box=new QDoubleSpinBox(group);box->setObjectName(name);box->setDecimals(3);
            box->setRange(minimum,100000.);box->setValue(value);box->setSuffix(suffix);
            form->addRow(label,box);
            connect(box,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double){emit parametersEdited();});
            return box;
        };
        m_passSpacing=spin("sectionPassSpacing",QStringLiteral("截面道间距"),5.,.1," mm");
        m_sampleSpacing=spin("sectionSampleSpacing",QStringLiteral("轴向采样间距"),1.,.1," mm");
        m_sprayDistance=spin("sectionSprayDistance",QStringLiteral("喷涂距离"),100.,.1," mm");
        m_spraySpeed=spin("sectionSpraySpeed",QStringLiteral("喷涂速度"),20.,.1," mm/s");
        m_transferSpeed=spin("sectionTransferSpeed",QStringLiteral("两端空移速度上限"),200.,.1," mm/s");
        m_extension=spin("sectionExtension",QStringLiteral("最小两端延伸"),10.,0.," mm");
        m_retract=spin("sectionRetract",QStringLiteral("端外附加安全余量"),2.,.1," mm");
        m_maximumTilt=spin("sectionMaximumTilt",QStringLiteral("相对喷涂侧最大倾角"),88.,0.," deg");
        m_maximumTilt->setMaximum(89.);
        m_alternate=new QCheckBox(QStringLiteral("相邻扫描道往返运行（姿态不翻转）"),group);
        m_alternate->setObjectName("sectionAlternate");m_alternate->setChecked(true);form->addRow(m_alternate);
        connect(m_alternate,&QCheckBox::toggled,this,[this](bool){emit parametersEdited();});
        auto* description=new QLabel(QStringLiteral(
            "适用于等截面板管件。保留原排道数量与顺序，调整管面倾角及瞄准点以覆盖交界处，不按膜厚补喷。往返时仅在上下端外关枪连接；延伸自动计入喷幅、加减速及阀延迟。覆盖按无遮挡且达到峰值 5% 的有限喷幅采样校核。"),group);
        description->setWordWrap(true);
        description->setMinimumHeight(description->fontMetrics().lineSpacing()*7);
        form->addRow(description);
        auto* button=new QPushButton(QStringLiteral("生成单侧扫描轨迹"),group);
        button->setObjectName("generateSectionSweep");form->addRow(button);
        connect(button,&QPushButton::clicked,this,&SectionSweepParameterPanel::generateRequested);
        layout->addWidget(group);layout->addStretch();
    }

    section_sweep::Parameters SectionSweepParameterPanel::parameters() const
    {
        section_sweep::Parameters p;
        p.scanAxis=m_scanAxis->currentIndex();p.sideAxis=m_sideAxis->currentIndex();
        p.passSpacing=m_passSpacing->value()*.001;p.sampleSpacing=m_sampleSpacing->value()*.001;
        p.sprayDistance=m_sprayDistance->value()*.001;p.spraySpeed=m_spraySpeed->value()*.001;
        p.transferSpeed=m_transferSpeed->value()*.001;p.endExtension=m_extension->value()*.001;
        p.retractClearance=m_retract->value()*.001;p.maximumTiltDegrees=m_maximumTilt->value();
        p.alternateDirection=m_alternate->isChecked();return p;
    }
}
