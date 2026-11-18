#include "RegionalParameterPanel.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace smrobot::workbench::freeform {
RegionalParameterPanel::RegionalParameterPanel(QWidget* parent):QWidget(parent){
    setObjectName("regionalParameterPanel");auto* layout=new QVBoxLayout(this);
    auto* geometry=new QGroupBox(QStringLiteral("分区与参考线"),this);auto* form=new QFormLayout(geometry);
    m_axis=new QComboBox(geometry);m_axis->setObjectName("regionalSpanAxis");
    m_axis->addItems({"X","Y","Z",QStringLiteral("自动选择 X / Y / Z")});m_axis->setCurrentIndex(2);
    m_axis->setToolTip(QStringLiteral("选择模型坐标中的展向。自动模式在 X/Y/Z 中选择；适用于具有上下平台的叶片。"));
    form->addRow(QStringLiteral("叶片展向"),m_axis);
    const QStringList names={QStringLiteral("端部搜索范围"),QStringLiteral("叶身侧法向阈值"),QStringLiteral("平台侧法向阈值"),
        QStringLiteral("切片间距"),QStringLiteral("喷道间距 / 偏移步长"),QStringLiteral("沿道采样间距"),
        QStringLiteral("起始沿面偏移"),QStringLiteral("终止沿面偏移"),QStringLiteral("喷距"),QStringLiteral("喷涂速度"),QStringLiteral("转移速度"),QStringLiteral("平滑连接长度")};
    const double defaults[]={22,.22,.78,8,8,2,-12,12,80,22,120,12};
    for(int i=0;i<12;++i){auto* box=new QDoubleSpinBox(this);m_values[i]=box;box->setObjectName(QString("regionalValue%1").arg(i));box->setDecimals(3);
        box->setRange(i==6||i==7?-10000.:.001,10000.);if(i==0)box->setRange(4,44);if(i==1||i==2)box->setRange(.02,.98);
        box->setValue(defaults[i]);box->setSuffix(i==0?" %":(i==1||i==2?QString():i==9||i==10?" mm/s":" mm"));
        connect(box,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](){emit parametersEdited();});
        if(i<3)form->addRow(names[i],box);}
    layout->addWidget(geometry);
    auto* partition=new QPushButton(QStringLiteral("1 自动识别叶片分区"),this);partition->setObjectName("regionalIdentify");m_actions[0]=partition;layout->addWidget(partition);
    m_colors=new QPushButton(QStringLiteral("显示分区颜色"),this);m_colors->setObjectName("regionalColors");m_colors->setCheckable(true);m_colors->setChecked(true);layout->addWidget(m_colors);
    connect(m_colors,&QPushButton::toggled,this,[this](){emit colorsChanged();});
    m_pathView=new QComboBox(this);m_pathView->setObjectName("regionalPathView");
    m_pathView->addItems({QStringLiteral("轨迹显示：工件表面目标线"),QStringLiteral("轨迹显示：喷枪 TCP（含喷距）")});layout->addWidget(m_pathView);
    connect(m_pathView,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](){emit colorsChanged();});
    auto* viewHint=new QLabel(QStringLiteral("表面目标线用于检查覆盖；TCP 为距表面一个喷距的枪位。黄色关枪转移线仅在 TCP 视图显示。"),this);viewHint->setWordWrap(true);layout->addWidget(viewHint);

    auto* legend=new QLabel(QStringLiteral("蓝：叶身；橙：下过渡；紫：上过渡\n绿：下内侧；青：上内侧；灰：禁喷；红：待审查"),this);legend->setWordWrap(true);layout->addWidget(legend);
    auto* regionGroup=new QGroupBox(QStringLiteral("逐区规划方法"),this);auto* rf=new QFormLayout(regionGroup);
    m_region=new QComboBox(this);m_region->setObjectName("regionalSelectedRegion");m_region->addItems({QStringLiteral("叶身"),QStringLiteral("下过渡区"),QStringLiteral("上过渡区"),QStringLiteral("下平台内侧"),QStringLiteral("上平台内侧")});
    m_method=new QComboBox(this);m_method->setObjectName("regionalMethod");m_method->addItems({QStringLiteral("切片法"),QStringLiteral("过渡参考线偏移法"),QStringLiteral("不生成")});
    rf->addRow(QStringLiteral("目标区域"),m_region);rf->addRow(QStringLiteral("该区方法"),m_method);
    connect(m_region,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int i){QSignalBlocker block(m_method);m_method->setCurrentIndex(int(m_methods[i]));});
    connect(m_method,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int i){m_methods[m_region->currentIndex()]=static_cast<blade_region::Method>(i);emit parametersEdited();});
    layout->addWidget(regionGroup);
    auto* process=new QGroupBox(QStringLiteral("切片 / 偏移 / 连接参数"),this);auto* pf=new QFormLayout(process);
    for(int i=3;i<12;++i)pf->addRow(names[i],m_values[i]);layout->addWidget(process);
    auto* hint=new QLabel(QStringLiteral("偏移为沿面有符号距离：负值朝叶身，正值朝平台。先拟合两侧边界及面中点参考线，再生成过渡道；范围包含 0 时保留参考线喷道。"),this);hint->setWordWrap(true);layout->addWidget(hint);
    const QStringList actions={QString(),QStringLiteral("2 识别两侧边界并拟合参考线"),QStringLiteral("3 按选区方法生成轨迹"),QStringLiteral("4 平滑连接全部区域轨迹"),QStringLiteral("取消计算"),QStringLiteral("生成全部启用区域轨迹")};
    for(int i=1;i<6;++i){auto* b=new QPushButton(actions[i],this);m_actions[i]=b;b->setObjectName(QString("regionalAction%1").arg(i));layout->addWidget(b);}
    for(int i=0;i<6;++i)connect(m_actions[i],&QPushButton::clicked,this,[this,i](){emit actionRequested(i);});
    connect(m_axis,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](){emit parametersEdited();});
    m_summary=new QLabel(this);m_summary->setObjectName("regionalSummary");m_summary->setWordWrap(true);layout->addWidget(m_summary);layout->addStretch();
    setState(false,false,false,false,false);
}
blade_region::Parameters RegionalParameterPanel::parameters()const {
    blade_region::Parameters p;p.spanAxis=m_axis->currentIndex();p.endFraction=m_values[0]->value()/100.;p.bodyCosine=m_values[1]->value();p.platformCosine=m_values[2]->value();
    p.sliceSpacing=m_values[3]->value()*.001;p.passSpacing=m_values[4]->value()*.001;p.sampleSpacing=m_values[5]->value()*.001;
    p.firstOffset=m_values[6]->value()*.001;p.lastOffset=m_values[7]->value()*.001;p.sprayDistance=m_values[8]->value()*.001;
    p.spraySpeed=m_values[9]->value()*.001;p.transferSpeed=m_values[10]->value()*.001;p.blendLength=m_values[11]->value()*.001;p.methods=m_methods;return p;
}
blade_region::Region RegionalParameterPanel::selectedRegion()const{return static_cast<blade_region::Region>(m_region->currentIndex());}
bool RegionalParameterPanel::tcpPreview()const{return m_pathView->currentIndex()==1;}
bool RegionalParameterPanel::colorsEnabled()const{return m_colors->isChecked();}
void RegionalParameterPanel::setSummary(const QString& s){m_summary->setText(s);}
void RegionalParameterPanel::setState(bool identified,bool references,bool paths,bool connected,bool busy){
    Q_UNUSED(references);Q_UNUSED(connected);
    m_pathView->setEnabled(!busy);
    for(auto* b:m_values)b->setEnabled(!busy);m_axis->setEnabled(!busy);m_region->setEnabled(!busy);m_method->setEnabled(!busy);m_colors->setEnabled(!busy&&identified);
    m_actions[0]->setEnabled(!busy);m_actions[1]->setEnabled(!busy&&identified);m_actions[2]->setEnabled(!busy&&identified);m_actions[3]->setEnabled(!busy&&paths);m_actions[4]->setEnabled(busy);m_actions[5]->setEnabled(!busy&&identified);
}
}
