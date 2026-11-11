#include "TrajectoryOptimizationService.h"
#include "ChaomaiPlaneOptimizer.h"
#include <SprayThicknessPrediction/ThicknessPrediction.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QPageSize>
#include <QLocale>
#include <QPainterPath>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace robot_qt_viewer {
namespace {
using namespace chaomai_plane;
constexpr double pi=3.14159265358979323846;
const char* planeId="chaomai.pipe_plate.plane";
const char* modelHash="2e2a3a496f9eeb4b9918bf795a3c759b386aaa80a9396e99e0c166d738f5cc01";
QByteArray readBytes(const QString& path) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)) throw std::runtime_error(("Cannot read: "+path).toStdString());return f.readAll();
}
QByteArray sha(const QByteArray& bytes) {return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();}
QString digest(const QString& path) {return QString::fromLatin1(sha(readBytes(path)));}
void saveBytes(const QString& path,const QByteArray& bytes) {
    QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()) throw std::runtime_error(("Cannot save: "+path).toStdString());
}
Json jsonFromBytes(QByteArray bytes) {
    if(bytes.startsWith("\xEF\xBB\xBF")) bytes.remove(0,3);
    return Json::parse(bytes.constData(),bytes.constData()+bytes.size());
}
Json readJson(const QString& path) {return jsonFromBytes(readBytes(path));}
void saveJson(const QString& path,const Json& j) {saveBytes(path,QByteArray::fromStdString(j.dump(2)));}
QString text(const Json& j) {return QString::fromStdString(j.get<std::string>());}
Json fromQt(const QJsonObject& j) {return jsonFromBytes(QJsonDocument(j).toJson(QJsonDocument::Compact));}
QJsonObject toQt(const Json& j) {return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).object();}
std::vector<double> parseNumbers(QString source) {
    source.remove(QChar(0xfeff));source.replace(',', ' ');
    const auto words=source.split(QRegularExpression(QStringLiteral("\\s+")),Qt::SkipEmptyParts);
    std::vector<double> numbers;numbers.reserve(words.size());
    for(const auto& w:words) {bool ok=false;double x=w.toDouble(&ok);if(!ok||!std::isfinite(x)) throw std::runtime_error("Invalid numeric trajectory value.");numbers.push_back(x);}
    return numbers;
}
Plan loadPlan(const QString& path,bool sourceSlots=true) {
    const auto data=readBytes(path);const auto ext=QFileInfo(path).suffix().toLower();
    if(ext==QStringLiteral("json")) return planFromJson(jsonFromBytes(data),sourceSlots);
    Plan plan;
    auto append=[&](Vec p,double yaw,double t,int on) {plan.p.push_back(p);plan.q.push_back(inverseKinematics(p,yaw));plan.time.push_back(t);plan.on.push_back(on);};
    if(ext==QStringLiteral("csv")) {
        const auto lines=QString::fromUtf8(data).remove(QChar(0xfeff)).split(QRegularExpression(QStringLiteral("[\\r\\n]+")),Qt::SkipEmptyParts);
        if(lines.size()<4) throw std::runtime_error("Process CSV has too few rows.");
        const auto header=lines[0].split(',');std::array<int,6> ids{};
        const QStringList required{QStringLiteral("time_s"),QStringLiteral("tcp_x_mm"),QStringLiteral("tcp_y_mm"),QStringLiteral("tcp_z_mm"),QStringLiteral("R_deg"),QStringLiteral("spray_effective_arrival")};
        for(int k=0;k<6;++k) {ids[k]=header.indexOf(required[k]);if(ids[k]<0) throw std::runtime_error("Process CSV is missing required columns.");}
        for(int row=1;row<lines.size();++row) {
            const auto values=lines[row].split(','); std::array<double,6> v{};
            for(int k=0;k<6;++k) {bool ok=false;if(ids[k]>=values.size()) throw std::runtime_error("Incomplete process CSV row.");v[k]=values[ids[k]].toDouble(&ok);if(!ok||!std::isfinite(v[k])) throw std::runtime_error("Invalid process CSV value.");}
            if(v[5]!=0&&v[5]!=1) throw std::runtime_error("Spray flags must be 0 or 1.");
            append({v[1]/1000,v[2]/1000,v[3]/1000},v[4]*pi/180,v[0],int(v[5]));
        }
    } else {
        QStringList clean;for(const auto& line:QString::fromUtf8(data).split('\n')) clean.push_back(line.section('#',0,0));
        const auto numbers=parseNumbers(clean.join('\n'));
        if(numbers.empty()||numbers.size()%17) throw std::runtime_error("TCP TXT requires row-major 4x4 matrix plus time (17 values per point).");
        double previousYaw=0;
        for(size_t i=0;i<numbers.size();i+=17) {
            const double* a=numbers.data()+i;
            if(std::abs(a[12])>1e-6||std::abs(a[13])>1e-6||std::abs(a[14])>1e-6||std::abs(a[15]-1)>1e-6) throw std::runtime_error("Invalid homogeneous TCP matrix.");
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) {
                double dot=0;for(int k=0;k<3;++k) dot+=a[k*4+r]*a[k*4+c];
                if(std::abs(dot-(r==c?1.:0.))>2e-5) throw std::runtime_error("TCP rotation is not orthonormal.");
            }
            double yaw=std::atan2(a[5],a[1]);
            if(i) {while(yaw-previousYaw>pi) yaw-=2*pi;while(yaw-previousYaw< -pi) yaw+=2*pi;}
            previousYaw=yaw;
            const double c=std::cos(yaw),s=std::sin(yaw),expected[9]{0,c,-s,0,s,c,1,0,0};
            for(int r=0;r<3;++r) for(int k=0;k<3;++k) if(std::abs(a[r*4+k]-expected[3*r+k])>2e-5) throw std::runtime_error("This XYZR method requires the demonstrated Rz(yaw) TCP frame.");
            append({a[3]/1000,a[7]/1000,a[11]/1000},yaw,a[16],0);
        }
        for(size_t i=1;i<plan.p.size();++i) {
            auto axial=[&](size_t j) {return std::hypot(plan.p[j][0]-plan.p[j-1][0],plan.p[j][1]-plan.p[j-1][1])<1e-7 && std::abs(plan.p[j][2]-plan.p[j-1][2])>1e-10 && std::abs(plan.q[j][3]-plan.q[j-1][3])<1e-6;};
            if(!axial(i)) continue;
            const size_t a=i-1;while(i+1<plan.p.size()&&axial(i+1)) ++i;
            if(std::abs(plan.p[i][2]-plan.p[a][2])>.15) for(size_t k=a+1;k<=i;++k) plan.on[k]=1;
        }
        plan.inferredSprayFlags=true;
    }
    validatePlan(plan,sourceSlots);return plan;
}
Surface loadSurface(const QString& path) {
    const auto bytes=readBytes(path);Surface mesh;
    auto append=[&](Triangle tri) {
        const Vec a{tri[1][0]-tri[0][0],tri[1][1]-tri[0][1],tri[1][2]-tri[0][2]},b{tri[2][0]-tri[0][0],tri[2][1]-tri[0][1],tri[2][2]-tri[0][2]};
        Vec n{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
        const double norm=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(!(norm>1e-20)) throw std::runtime_error("STL contains a degenerate triangle.");
        for(auto& x:n) x/=norm;
        for(auto& p:tri) {p[0]=-p[0];p[1]=.14-p[1];p[2]+=.375;}
        n[0]=-n[0];n[1]=-n[1];mesh.triangles.push_back(tri);mesh.normals.push_back(n);
    };
    const auto count=bytes.size()>=84?qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()+80)):0;
    if(count&&quint64(bytes.size())==84+50*quint64(count)) {
        for(quint32 j=0;j<count;++j) {Triangle tri{};for(int k=0;k<9;++k) {
            quint32 bits=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()+84+j*50+12+k*4));float f;std::memcpy(&f,&bits,4);tri[k/3][k%3]=f;
        }append(tri);}
    } else {
        Triangle tri{};int vertex=0;
        for(const auto& line:QString::fromUtf8(bytes).split('\n')) {
            const auto s=line.trimmed();if(!s.startsWith(QStringLiteral("vertex "))) continue;
            const auto values=parseNumbers(s.mid(7));if(values.size()!=3) throw std::runtime_error("Invalid ASCII STL vertex.");
            tri[vertex++]={values[0],values[1],values[2]};if(vertex==3) {append(tri);vertex=0;}
        }
        if(vertex) throw std::runtime_error("Incomplete ASCII STL triangle.");
    }
    if(mesh.triangles.empty()) throw std::runtime_error("No STL triangles.");return mesh;
}
struct Configuration {Parameters cfg{};double target=165,tolerance=10,step=.02;};
Configuration configure(const Json& request) {
    if(request.at("object_id")!=planeId) throw std::runtime_error("Plane optimization is registered only for chaomai.pipe_plate.plane.");
    if(digest(text(request.at("model_path")))!=QString::fromLatin1(modelHash)) throw std::runtime_error("STL is not the registered Chaomai pipe/plate model.");
    Configuration result;result.target=request.value("target_um",165.);result.tolerance=request.at("tolerance_percent");
    if(!std::isfinite(result.target)||!std::isfinite(result.tolerance)||result.target<=0||result.target>10000||result.tolerance<.1||result.tolerance>100) throw std::runtime_error("Invalid target thickness or tolerance.");
    const Json opts=request.value("prediction",Json::object());
    if(opts.value("model",std::string("paper_gaussian"))!="paper_gaussian") throw std::runtime_error("Only the PaperGaussian prediction model is supported.");
    if(opts.value("spray_axis",Json{0,0,1})!=Json{0,0,1}||opts.value("feed_axis",Json{0,1,0})!=Json{0,1,0}) throw std::runtime_error("XYZR requires local spray +Z and Gaussian major +Y.");
    const spraythickness::PaperGaussianParameters d;const spraythickness::HeatHistoryParameters h;
    auto& cfg=result.cfg;
#define PARAM(index, object, name) cfg[index]=opts.value(#name,object.name)
    PARAM(0,d,amplitudeMillimeters);PARAM(1,d,rotationRadians);PARAM(2,d,phiOffsetRadians);PARAM(3,d,psiOffsetRadians);
    PARAM(4,d,sigmaPhiRadians);PARAM(5,d,sigmaPsiRadians);PARAM(6,d,referenceDistanceMeters);PARAM(7,d,referenceAngleDegrees);PARAM(8,d,referenceExposureSeconds);
    PARAM(10,h,correctionAmplitude);PARAM(11,h,historyScaleSeconds);PARAM(12,h,referenceHistorySeconds);PARAM(13,h,coolingTimeSeconds);PARAM(14,h,activityThresholdRatio);
#undef PARAM
    cfg[6]*=1000;cfg[9]=1;cfg[15]=1e-10;cfg[16]=.01;
    cfg[17]=opts.value("history",true);cfg[18]=opts.value("bvh",true);cfg[19]=opts.value("auto_flip",true);
    for(double v:cfg) if(!std::isfinite(v)) throw std::runtime_error("Non-finite deposition parameters.");
    for(int k:{0,4,5,6,8,11,12,13}) if(cfg[k]<=0) throw std::runtime_error("Gaussian widths, reference distance/exposure and heat time scales must be positive.");
    result.step=opts.value("time_step_s",.02);
    if(!std::isfinite(result.step)||result.step<.001||result.step>.05) throw std::runtime_error("Integration step must be 0.001..0.05 seconds.");
    return result;
}
// Versioned native cache: all prediction inputs plus implementation fingerprint.
// Target/tolerance are re-evaluated on every request. Failed IO is just a miss.
QByteArray predictionKey(const Surface& mesh,const Grid& grid,const Plan& plan,const Parameters& cfg,double step) {
    QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(implementationSignature());
    auto add=[&](const auto& values) {
        const quint64 count=values.size();hash.addData(reinterpret_cast<const char*>(&count),sizeof(count));
        if(count) hash.addData(reinterpret_cast<const char*>(values.data()),int(count*sizeof(values[0])));
    };
    add(mesh.triangles);add(grid.positions);add(grid.normals);add(cfg);hash.addData(reinterpret_cast<const char*>(&step),sizeof(step));
    add(plan.p);add(plan.q);add(plan.time);add(plan.on);
    hash.addData(QByteArray::fromStdString(plan.strokes.dump()));return hash.result().toHex();
}
std::vector<double> evaluate(const Surface& mesh,const Grid& grid,const Plan& plan,const Parameters& cfg,double step,
    const QString& cacheDir,const Context& ctx,double base,double span) {
    ctx.check();const bool cached=!cacheDir.isEmpty()&&qEnvironmentVariable("RS2026_OPTIMIZATION_NO_CACHE")!=QStringLiteral("1");
    const auto key=cached?predictionKey(mesh,grid,plan,cfg,step):QByteArray{};
    const auto file=cacheDir+QStringLiteral("/")+QString::fromLatin1(key)+QStringLiteral(".field");
    if(cached) {
        QFile f(file);
        if(f.open(QIODevice::ReadOnly)) {
            const auto data=f.readAll();const int size=int(grid.positions.size()*sizeof(double));
            if(data.size()==128+size&&data.left(64)==key&&data.mid(64,64)==sha(data.mid(128))) {
                std::vector<double> values(grid.positions.size());std::memcpy(values.data(),data.constData()+128,size);
                bool valid=true;for(double x:values) if(!std::isfinite(x)||x<0) valid=false;
                if(valid) {ctx.report(base+span,"Reused identical native prediction inputs; checking current metrics");return values;}
            }
        }
    }
    auto values=predict(mesh,grid,plan,cfg,step,ctx,base,span);
    if(cached) {
        const QByteArray bytes(reinterpret_cast<const char*>(values.data()),int(values.size()*sizeof(double)));
        if(QDir().mkpath(cacheDir)) {QSaveFile f(file);if(f.open(QIODevice::WriteOnly)) {const auto payload=key+sha(bytes)+bytes;if(f.write(payload)==payload.size()) f.commit();}}
    }
    return values;
}
QString exportPlan(const Plan& plan,const QString& out,bool accepted) {
    const QString stem=accepted?QStringLiteral("optimized"):QStringLiteral("candidate_not_accepted");
    QByteArray txt,csv;QTextStream t(&txt,QIODevice::WriteOnly),c(&csv,QIODevice::WriteOnly);
    t.setLocale(QLocale::c());c.setLocale(QLocale::c());t.setRealNumberPrecision(12);c.setRealNumberPrecision(17);
    c<<"time_s,tcp_x_mm,tcp_y_mm,tcp_z_mm,R_deg,spray_effective_arrival,spray_command_arrival,phase,row_1based\n";
    for(size_t i=0;i<plan.p.size();++i) {
        const double yaw=plan.q[i][3],co=std::cos(yaw),si=std::sin(yaw);
        const auto& p=plan.p[i];const std::array<double,17> values{0,co,-si,p[0]*1000,0,si,co,p[1]*1000,1,0,0,p[2]*1000,0,0,0,1,plan.time[i]};
        for(int k=0;k<17;++k) {if(k) t<<' ';t<<values[k];}t<<'\n';
        c<<plan.time[i]<<','<<p[0]*1000<<','<<p[1]*1000<<','<<p[2]*1000<<','<<yaw*180/pi<<','<<plan.on[i]<<','<<plan.command[i]<<','<<QString::fromStdString(plan.phase[i])<<','<<plan.row[i]+1<<'\n';
    }
    t.flush();c.flush();saveBytes(out+"/"+stem+"_TCP.txt",txt);saveBytes(out+"/"+stem+"_process.csv",csv);saveJson(out+"/"+stem+"_plan.json",planToJson(plan));return stem;
}
void exportFields(const QString& out,const Grid& grid,const std::vector<double>& before,const std::vector<double>& after) {
    QByteArray csv;QTextStream c(&csv,QIODevice::WriteOnly);c.setLocale(QLocale::c());c.setRealNumberPrecision(17);
    c<<"x_mm,y_mm,z_mm,nx,ny,nz,before_um,after_um,is_copper\n";
    for(size_t i=0;i<before.size();++i) {
        for(double v:grid.positions[i]) c<<v<<',';for(double v:grid.normals[i]) c<<v<<',';
        c<<before[i]<<','<<after[i]<<','<<int(grid.copper[i])<<'\n';
    }
    c.flush();saveBytes(out+"/thickness_before_after.csv",csv);
}
QColor heatColor(double value,double max) {
    // Shared continuous scale for before/after. Empty/occluded cells stay blank.
    const double f=std::clamp(value/max,0.,1.);
    const std::array<QColor,6> stops{QColor("#30123b"),QColor("#4675ed"),QColor("#1bcfd4"),QColor("#a4fc3c"),QColor("#f9a031"),QColor("#7a0403")};
    const int k=std::min(4,int(f*5));const double t=f*5-k;
    return QColor::fromRgbF(stops[k].redF()*(1-t)+stops[k+1].redF()*t,stops[k].greenF()*(1-t)+stops[k+1].greenF()*t,stops[k].blueF()*(1-t)+stops[k+1].blueF()*t);
}
void drawGraph(QPainter& p,const Json& result,const Surface& surface,const Grid& grid,const std::vector<double>& before,const std::vector<double>& after) {
    p.setRenderHint(QPainter::Antialiasing);p.fillRect(QRectF(0,0,1800,1750),Qt::white);
    QFont font(QStringLiteral("Microsoft YaHei"),12);p.setFont(font);p.setPen(QColor("#243a4a"));
    auto title=[&](double y,const QString& s,int size=14) {auto f=font;f.setPointSize(size);f.setBold(true);p.setFont(f);p.drawText(QRectF(80,y,1650,48),Qt::AlignCenter,s);p.setFont(font);};
    title(20,QStringLiteral("超迈管面焊接件：铜片优化前后总厚度对比"),22);
    const double left=145,width=1450;
    auto x=[&](double mm) {return left+(mm+60)/120*width;};
    auto axes=[&](QRectF r,double lo,double hi,const QString& label) {
        p.setPen(QPen(QColor("#e1e7eb"),1));
        for(int i=0;i<=12;++i) {double xx=x(-60+i*10);p.drawLine(QPointF(xx,r.top()),QPointF(xx,r.bottom()));p.setPen(QColor("#526572"));p.drawText(QRectF(xx-25,r.bottom()+6,50,24),Qt::AlignCenter,QString::number(-60+i*10));p.setPen(QPen(QColor("#e1e7eb"),1));}
        for(int i=0;i<=4;++i) {const double yy=r.bottom()-r.height()*i/4;p.drawLine(QPointF(r.left(),yy),QPointF(r.right(),yy));p.setPen(QColor("#526572"));p.drawText(QRectF(58,yy-12,76,24),Qt::AlignRight,QString::number(lo+(hi-lo)*i/4,'f',1));p.setPen(QPen(QColor("#e1e7eb"),1));}
        p.setPen(QColor("#8395a2"));p.drawRect(r);p.drawText(QRectF(left,r.bottom()+29,width,26),Qt::AlignCenter,QStringLiteral("横向 X / mm"));
        p.save();p.translate(30,r.center().y());p.rotate(-90);p.drawText(QRectF(-r.height()/2,-18,r.height(),30),Qt::AlignCenter,label);p.restore();
    };
    const double mid=grid.heights[grid.heights.size()/2]*1000;
    title(77,QStringLiteral("真实 STL 中部截面（四幅图横向坐标对齐）"));
    const QRectF geom(left,135,width,29*width/120);axes(geom,-4,25,QStringLiteral("距铜片正面 / mm"));
    auto gy=[&](double y){return geom.bottom()-(y+4)/29*geom.height();};
    p.setPen(QPen(QColor("#7c8f9e"),2));
    for(const auto& tri:surface.triangles) {
        std::vector<Vec> hits;
        for(int k=0;k<3;++k) {const auto& a=tri[k];const auto& b=tri[(k+1)%3];const double da=a[2]*1000-mid,db=b[2]*1000-mid;if(da*db<0) {Vec h{};for(int c=0;c<3;++c) h[c]=a[c]+(b[c]-a[c])*da/(da-db);hits.push_back(h);}}
        if(hits.size()==2) p.drawLine(QPointF(x(hits[0][0]*1000),gy(140-hits[0][1]*1000)),QPointF(x(hits[1][0]*1000),gy(140-hits[1][1]*1000)));
    }
    double vmax=double(result["target_um"])*(1+double(result["tolerance_percent"])/100);
    for(size_t i=0;i<before.size();++i) if(grid.copper[i]) vmax=std::max({vmax,before[i],after[i]});vmax*=1.06;
    const double target=result["target_um"],tol=result["tolerance_percent"];
    title(545,QStringLiteral("中部截面总预测厚度（橙色：优化前；青色：优化后）"));
    const QRectF line(left,605,width,270);axes(line,0,vmax,QStringLiteral("总预测厚度 / μm"));
    auto ly=[&](double v){return line.bottom()-v/vmax*line.height();};
    p.fillRect(QRectF(left,ly(target*(1+tol/100)),width,ly(target*(1-tol/100))-ly(target*(1+tol/100))),QColor(111,177,133,45));
    p.setPen(QPen(QColor("#6f957d"),1,Qt::DashLine));p.drawLine(QPointF(left,ly(target)),QPointF(left+width,ly(target)));
    std::vector<size_t> ids;
    for(size_t i=0;i<grid.positions.size();++i) if(grid.copper[i]&&std::abs(grid.positions[i][2]-mid)<1e-5) ids.push_back(i);
    std::sort(ids.begin(),ids.end(),[&](size_t a,size_t b){return grid.positions[a][0]<grid.positions[b][0];});
    for(int which=0;which<2;++which) {
        const auto& vals=which?after:before;QPainterPath path;bool started=false;double prev=0;
        for(auto i:ids) {const double xx=grid.positions[i][0];if(!started||xx-prev>.4) path.moveTo(x(xx),ly(vals[i]));else path.lineTo(x(xx),ly(vals[i]));started=true;prev=xx;}
        p.setPen(QPen(QColor(which?"#157c82":"#e18b32"),2.5));p.drawPath(path);
    }
    for(int which=0;which<2;++which) {
        const auto& vals=which?after:before;const auto& m=result[which?"after":"before"];
        const double y=which?1340:1015;const QRectF heat(left,y,width,210);
        title(y-60,QStringLiteral("%1：%2～%3 μm；CV %4%；最大偏差 %5%")
            .arg(which?QStringLiteral("优化后候选"):QStringLiteral("优化前"))
            .arg(double(m["min_um"]),0,'f',2).arg(double(m["max_um"]),0,'f',2).arg(double(m["cv_percent"]),0,'f',3).arg(double(m["max_relative_error_percent"]),0,'f',3));
        const size_t count=grid.cells.size();p.setPen(Qt::NoPen);
        p.setRenderHint(QPainter::Antialiasing, false);
        for(size_t k=0;k<grid.heights.size();++k) {
            const double a=k?(grid.heights[k-1]+grid.heights[k])*.5:grid.heights.front();
            const double b=k+1<grid.heights.size()?(grid.heights[k]+grid.heights[k+1])*.5:grid.heights.back();
            const double top=heat.bottom()-(b-grid.heights.front())/.2*heat.height(),bottom=heat.bottom()-(a-grid.heights.front())/.2*heat.height();
            for(size_t j=0;j<count;++j) if(grid.copper[k*count+j]) {
                const double xa=grid.cells[j].a[0]*1000,xb=grid.cells[j].b[0]*1000;
                p.fillRect(QRectF(x(std::min(xa,xb)),top,std::abs(x(xb)-x(xa)),bottom-top),heatColor(vals[k*count+j],vmax));
            }
        }
        p.setRenderHint(QPainter::Antialiasing, true);
        axes(heat,0,200,QStringLiteral("工件高度 / mm"));
        for(int k=0;k<210;++k) p.fillRect(QRectF(1620,y+k,24,1.5),heatColor(vmax*(1-k/210.),vmax));
        p.setPen(QColor("#526572"));p.drawText(QRectF(1650,y-5,100,30),QString::number(vmax,'f',1));p.drawText(QRectF(1650,y+190,100,30),QStringLiteral("0 μm"));
    }
    p.setPen(QColor("#526572"));p.drawText(QRectF(110,1640,1600,75),Qt::AlignLeft|Qt::TextWordWrap,
        QStringLiteral("%1。指标仅评价外露铜片正面；管面不计入均匀性。\n模型预测，非实测。空白为遮挡/非铜片区域，不代表零厚度。前后使用同一组参数。")
            .arg(bool(result["accepted"])?QStringLiteral("密集采样复核达标"):QStringLiteral("未达到设定范围，候选未接受")));
}
void draw(const Json& result,const Surface& surface,const Grid& grid,const std::vector<double>& before,const std::vector<double>& after,const QString& out) {
    QImage image(1800,1750,QImage::Format_ARGB32_Premultiplied);
    {QPainter painter(&image);if(!painter.isActive()) throw std::runtime_error("Cannot create graph painter.");drawGraph(painter,result,surface,grid,before,after);}
    QSaveFile file(out+QStringLiteral("/thickness_comparison.png"));
    if(!file.open(QIODevice::WriteOnly)||!image.save(&file,"PNG")||!file.commit()) throw std::runtime_error("Cannot save thickness PNG.");
    QSaveFile pdf(out+QStringLiteral("/thickness_comparison.pdf"));if(!pdf.open(QIODevice::WriteOnly)) throw std::runtime_error("Cannot write thickness PDF.");
    {QPdfWriter writer(&pdf);writer.setTitle(QStringLiteral("铜片轨迹优化厚度对比"));writer.setResolution(150);writer.setPageSize(QPageSize(QSizeF(300,292),QPageSize::Millimeter));writer.setPageMargins(QMarginsF(0,0,0,0));QPainter painter(&writer);if(!painter.isActive()) throw std::runtime_error("Cannot create PDF painter.");painter.scale(writer.width()/1800.,writer.height()/1750.);drawGraph(painter,result,surface,grid,before,after);}
    if(!pdf.commit()) throw std::runtime_error("Cannot commit thickness PDF.");
}
}
QJsonObject runNativePlaneOptimization(const QJsonObject& requestQt,const QString& out,bool compareOnly,
    const std::atomic_bool& cancelled,const std::function<void(double,const QString&)>& progress) {
    Context ctx{cancelled,[&](double v,const std::string& s){if(progress) progress(v,QString::fromStdString(s));}};
    ctx.check();Json request=fromQt(requestQt),result;
    if(compareOnly) request=readJson(out+"/request.json");
    auto settings=configure(request);const auto cfg=settings.cfg;const double target=settings.target,tol=settings.tolerance,step=settings.step;
    const Json sourceRequest=request;const QString originalModel=text(request["model_path"]),originalTrajectory=text(request["trajectory_path"]);
    if(!QDir().mkpath(out)) throw std::runtime_error("Cannot create run directory.");
    if(compareOnly) {
        result=readJson(out+"/result.json");
        if(result.at("object_id")!=planeId||result.at("target_um")!=target||result.at("tolerance_percent")!=tol||result.at("effective_kernel_parameters")!=Json(cfg)) throw std::runtime_error("Recorded optimization parameters changed; rerun optimization.");
        if(digest(originalModel)!=text(result["model_sha256"])||digest(originalTrajectory)!=text(result["trajectory_sha256"])) throw std::runtime_error("Input files changed after optimization; re-import and optimize.");
        request=readJson(out+"/input_snapshot_request.json");
        if(digest(text(request["model_path"]))!=text(result["model_sha256"])||digest(text(request["trajectory_path"]))!=text(result["trajectory_sha256"])) throw std::runtime_error("Input snapshots changed after verification.");
    } else {
        const auto modelBytes=readBytes(originalModel),trajectoryBytes=readBytes(originalTrajectory);
        if(sha(modelBytes)!=QByteArray(modelHash)) throw std::runtime_error("Model changed while creating input snapshot.");
        QDir().mkpath(out+"/inputs");const QString modelCopy=out+"/inputs/workpiece.stl";
        const QString trajectoryCopy=out+"/inputs/trajectory."+QFileInfo(originalTrajectory).suffix().toLower();
        saveBytes(modelCopy,modelBytes);saveBytes(trajectoryCopy,trajectoryBytes);
        request["model_path"]=modelCopy.toStdString();request["trajectory_path"]=trajectoryCopy.toStdString();
        saveJson(out+"/request.json",sourceRequest);saveJson(out+"/input_snapshot_request.json",request);
    }
    const auto original=loadPlan(text(request["trajectory_path"]));const auto surface=loadSurface(text(request["model_path"]));
    ctx.report(.02,"Validated model identity and imported 36-scan trajectory; native C++ execution");
    const auto profile=sampleProfile(surface,original,.00025,ctx);const auto grid=makeGrid(profile,heightLevels(false));
    const QString cacheDir=compareOnly?QString{}:QFileInfo(out).dir().absoluteFilePath("prediction_cache_cpp");
    auto evaluator=[&](const Grid& g,const Plan& p,const Parameters& c,double dt,double base,double span) {return evaluate(surface,g,p,c,dt,cacheDir,ctx,base,span);};
    auto before=evaluator(grid,original,cfg,step,.03,compareOnly?.40:.15);
    Plan candidate;Json config;
    if(compareOnly) {
        const QString planPath=out+"/"+text(result["trajectory_stem"])+"_plan.json";
        if(digest(planPath)!=text(result["optimized_plan_sha256"])) throw std::runtime_error("Optimized trajectory changed after verification.");
        candidate=loadPlan(planPath,false);
    } else {
        config=Json::parse(initialSolution());config["target_um"]=target;config["max_relative_error"]=tol/100;
        candidate=buildPlan(original,profile,config,ctx);
    }
    auto after=evaluator(grid,candidate,cfg,step,compareOnly?.45:.18,compareOnly?.43:.15);
    if(!compareOnly) {
        if(double(metrics(after,grid,target,tol)["max_relative_error_percent"])>tol) {
            ctx.report(.35,"Retaining geometry; refitting bounded axial speed for current parameters/tolerance");
            config=refit(surface,original,profile,config,cfg,step,target,tol,ctx,evaluator);
            candidate=buildPlan(original,profile,config,ctx);after=evaluator(grid,candidate,cfg,step,.65,.10);
        }
        ctx.report(.76,"Independent dense cross-section and halved time-step validation");
        const auto denseProfile=sampleProfile(surface,original,.000025,ctx);
        const auto denseGrid=makeGrid(denseProfile,heightLevels(true),true);
        const auto field=evaluator(denseGrid,candidate,cfg,std::min(step/2,.01),.76,.20);
        const auto fine=metrics(field,denseGrid,target,tol),bstats=metrics(before,grid,target,tol),astats=metrics(after,grid,target,tol);
        const bool accepted=std::max(double(fine["max_relative_error_percent"]),double(astats["max_relative_error_percent"]))<=tol+1e-8;
        result={{"schema_version",1},{"implementation","native_cpp"},{"implementation_signature",implementationSignature()},{"object_id",planeId},{"accepted",accepted},{"status",accepted?"accepted":"tolerance_not_met"},
            {"target_um",target},{"tolerance_percent",tol},{"metric","max(abs(thickness/target-1))*100"},{"before",bstats},{"after",astats},{"dense_validation",fine},
            {"dense_samples",denseGrid.positions.size()},{"dense_profile_pitch_mm",.025},{"validation_time_step_s",std::min(step/2,.01)},
            {"original_duration_s",original.time.back()},{"optimized_duration_s",candidate.time.back()},
            {"model_sha256",digest(text(request["model_path"])).toStdString()},{"trajectory_sha256",digest(text(request["trajectory_path"])).toStdString()},
            {"original_input_paths",{{"model",originalModel.toStdString()},{"trajectory",originalTrajectory.toStdString()}}},
            {"prediction",request.value("prediction",Json::object())},{"effective_kernel_parameters",cfg},{"inferred_spray_flags",original.inferredSprayFlags},
            {"scope","Exposed copper front only; tube uniformity excluded. Finite sampling model prediction."},
            {"preserved_source_passes",36},{"preserved_normal_passes",20},{"output_dir",out.toStdString()},{"comparison_png",(out+"/thickness_comparison.png").toStdString()}};
        ctx.check();const auto stem=exportPlan(candidate,out,accepted);result["trajectory_stem"]=stem.toStdString();result["optimized_plan_sha256"]=digest(out+"/"+stem+"_plan.json").toStdString();
        saveJson(out+"/fitted_solution.json",config);
    } else {
        result["before"]=metrics(before,grid,target,tol);result["after"]=metrics(after,grid,target,tol);result["comparison_recomputed"]=true;
        const bool accepted=std::max(double(result["dense_validation"]["max_relative_error_percent"]),double(result["after"]["max_relative_error_percent"]))<=tol+1e-8;
        result["accepted"]=accepted;result["status"]=accepted?"accepted":"tolerance_not_met";
    }
    ctx.check();exportFields(out,grid,before,after);draw(result,surface,grid,before,after,out);ctx.check();
    // Final record is committed atomically only after all exports and graph IO.
    saveJson(out+"/result.json",result);
    ctx.report(1,compareOnly?"Recomputed both thickness fields and comparison graph":bool(result["accepted"])?"Accepted after dense/time-step verification":"Candidate retained; requested tolerance not achieved");
    return toQt(result);
}
}
