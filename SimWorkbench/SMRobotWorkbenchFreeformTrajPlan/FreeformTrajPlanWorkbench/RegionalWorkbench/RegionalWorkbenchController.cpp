#include "SlicingWorkbench/FreeformTrajPlanWorkbenchController.h"
#include "RegionalParameterPanel.h"
#include "SectionSweepWorkbench/SectionSweepAdapters.h"
#include <QMetaObject>
#include <QStringList>

namespace smrobot::workbench::freeform {
FreeformTrajPlanWorkbenchController::~FreeformTrajPlanWorkbenchController(){
    m_regionalCancel=true;if(m_regionalThread.joinable())m_regionalThread.join();
}
void FreeformTrajPlanWorkbenchController::updateRegionalPanel(){
    auto* panel=m_parameterPanel.regionalPanel();if(!panel)return;
    if(m_regionalBusy){panel->setState(false,false,false,false,true);return;}
    panel->setState(m_regionalSession.identified(),m_regionalSession.referencesReady(),
        !m_regionalSession.paths().empty(),m_regionalSession.trajectory().success,m_regionalBusy);
    if(m_regionalBusy)return;
    const QStringList names={QStringLiteral("叶身"),QStringLiteral("下过渡"),QStringLiteral("上过渡"),QStringLiteral("下内侧"),QStringLiteral("上内侧"),QStringLiteral("禁喷"),QStringLiteral("待审查")};
    QString summary;
    if(m_regionalSession.identified())for(int i=0;i<7;++i)summary+=QStringLiteral("%1：%2 mm²\n").arg(names[i]).arg(m_regionalSession.regionAreas()[i]*1e6,0,'f',1);
    if(m_regionalSession.identified())summary+=QStringLiteral("参考带 %1；分区道 %2；连接位姿 %3。红色待审查面不参与喷涂。")
        .arg(m_regionalSession.bands().size()).arg(m_regionalSession.paths().size()).arg(m_regionalSession.trajectory().points.size());
    panel->setSummary(summary);
}
void FreeformTrajPlanWorkbenchController::regionalParametersEdited(){
    if(m_regionalBusy)return;
    try{m_regionalSession.setParameters(m_parameterPanel.regionalPanel()->parameters());m_regionalParametersValid=true;
        if(m_regionalPlanning){refreshRegionalOverlay();publishSharedData();showResult(true,QStringLiteral("参数已更新，相关轨迹已失效；分区参数改变后须重新识别。"));}}
    catch(const std::exception& e){m_regionalParametersValid=false;m_regionalSession.clearPaths();if(m_regionalPlanning){publishSharedData();refreshRegionalOverlay();showResult(false,QString::fromUtf8(e.what()));}}
    updateRegionalPanel();
}
void FreeformTrajPlanWorkbenchController::startRegionalJob(std::function<blade_region::Report()> job){
    if(m_regionalBusy)return;if(m_regionalThread.joinable())m_regionalThread.join();
    m_regionalSession.clearConnection();publishSharedData();m_regionalCancel=false;m_regionalBusy=true;
    updateRegionalPanel();showResult(true,QStringLiteral("分区规划计算中，可取消；请等待结果后操作模型或导出。"));
    const auto epoch=++m_regionalEpoch;
    m_regionalThread=std::thread([this,epoch,job=std::move(job)](){
        blade_region::Report report;
        try{report=job();}catch(const std::exception& e){report={false,e.what()};}
        QMetaObject::invokeMethod(this,[this,report,epoch](){if(epoch!=m_regionalEpoch)return;m_regionalBusy=false;
            refreshRegionalOverlay();publishSharedData();updateRegionalPanel();
            showResult(report.success,QString::fromStdString(report.message));},Qt::QueuedConnection);
    });
}
void FreeformTrajPlanWorkbenchController::regionalAction(int action){
    if(action==4){m_regionalCancel=true;return;}
    if(m_regionalBusy||!m_regionalPlanning)return;
    if(!m_regionalParametersValid){showResult(false,QStringLiteral("请先修正分区参数。"));return;}
    if(action==0){
        if(m_generalData.modelDataManager_.mainModelData_.m_indices.empty()){showResult(false,QStringLiteral("请先导入叶片模型。"));return;}
        auto mesh=std::make_shared<std::vector<section_sweep::Triangle>>(sectionMeshInMeters(m_generalData.modelDataManager_.mainModelData_,m_algorithmToSceneScale));
        const auto p=m_parameterPanel.regionalPanel()->parameters();
        startRegionalJob([this,mesh,p](){return m_regionalSession.identify(*mesh,p,&m_regionalCancel);});
    }else if(action==1){startRegionalJob([this](){return m_regionalSession.fitReferences(&m_regionalCancel);});
    }else if(action==2){const auto region=m_parameterPanel.regionalPanel()->selectedRegion();
        startRegionalJob([this,region](){return m_regionalSession.generate(region,&m_regionalCancel);});
    }else if(action==3){startRegionalJob([this](){return m_regionalSession.connect(&m_regionalCancel);});
    }else if(action==5){startRegionalJob([this](){
        for(int i=0;i<5;++i)if(m_regionalSession.parameters().methods[i]!=blade_region::Method::Disabled){auto report=m_regionalSession.generate(static_cast<blade_region::Region>(i),&m_regionalCancel);if(!report.success)return report;}
        return blade_region::Report{true,"All enabled regions generated; inspect individual paths then use smooth connection."};});}
}
}
