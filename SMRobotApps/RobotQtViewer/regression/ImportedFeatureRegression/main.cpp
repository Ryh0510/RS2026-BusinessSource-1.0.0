#include <CalibrationInstructionTranslation/Calibration/WorkpieceCalibration.h>
#include <CalibrationInstructionTranslation/TransformUtils.h>
#include <SprayThicknessPredictionOpenGL/OpenGLThicknessPredictionBackend.h>
#include <GLRuntime/GLRuntime.h>
#include <UVTrajPlan/UvPlanningSession.h>
#include <QTemporaryDir>
#include <QGuiApplication>
#include <fstream>
#include <filesystem>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

void require(bool value, const char* message)
{
    if(!value) throw std::runtime_error(message);
}

void calibrationTest()
{
    using namespace smrobot::spray::translation;
    const Eigen::Vector3d center(0.25, -0.15, 0.4);
    const Eigen::AngleAxisd rotation(0.43, Eigen::Vector3d(1,2,3).normalized());
    CalibrationPointList points;
    for(int i=0;i<12;++i) {
        const double angle=2*pi*(i%6)/6;
        points.push_back(center+rotation*Eigen::Vector3d(0.08*std::cos(angle),0.08*std::sin(angle),i<6?-0.1:0.1));
    }
    const auto fit=WorkpieceCalibrationSolver::fitCylinder3d(points);
    require(fit.ok(), "Tilted cylinder calibration failed");
    require(std::abs(fit.value.radiusMeters-0.08)<1e-8, "Calibration radius/unit mismatch");
    require((fit.value.axisPointBaseMeters-center).norm()<1e-8, "Calibration center mismatch");
    require(std::abs(fit.value.axisDirectionBase.dot(rotation*Eigen::Vector3d::UnitZ()))>1-1e-8,
        "Calibration axis mismatch");
    const auto top=center+0.1*(rotation*Eigen::Vector3d::UnitZ());
    const auto frame=WorkpieceCalibrationSolver::computeWorkpieceFrame(fit.value,top,0.2,
        center,center+0.15*(rotation*Eigen::Vector3d::UnitY()));
    require(frame.ok(), "Workpiece frame calibration failed");
    const auto decoded=transformComponents(frame.value.baseFromPlanning);
    require(decoded.ok() && (makeTransform(decoded.value).matrix()-frame.value.baseFromPlanning.matrix()).norm()<1e-10,
        "Calibrated transform round-trip failed");
    CalibrationPointList circle;
    for(int i=0;i<6;++i) circle.push_back(center+Eigen::Vector3d(0.08*std::cos(2*pi*i/6),0.08*std::sin(2*pi*i/6),0));
    const auto planar=WorkpieceCalibrationSolver::fitCircle2d(circle);
    require(planar.ok() && std::abs(planar.value.radiusMeters-0.08)<1e-8, "Six-point planar calibration failed");
    CalibrationPointList invalid(6,Eigen::Vector3d::Zero());
    require(!WorkpieceCalibrationSolver::fitCircle2d(invalid).ok(), "Degenerate calibration accepted");
    std::cout << "Tilted cylinder, workpiece frame, transform round-trip, invalid geometry: passed\n";
}

void thicknessTest()
{
    QSurfaceFormat format;format.setVersion(4,3);format.setProfile(QSurfaceFormat::CoreProfile);
    QOpenGLContext context;context.setFormat(format);
    require(context.create(), "Cannot create OpenGL compute context");
    QOffscreenSurface surface;surface.setFormat(context.format());surface.create();
    require(surface.isValid() && context.makeCurrent(&surface), "Cannot activate OpenGL compute context");
    require(GLRuntime::instance().initialize(), "Cannot load OpenGL functions");
    std::cout << "GPU: " << GLRuntime::instance().renderer() << "\n";
    spraythickness::ThicknessPredictionTask task;
    task.options.enableHistoryCorrection=false;task.options.enableBvhOcclusion=false;
    task.options.periodicLocal.enabled=false;task.options.axisymmetricProfile.enabled=false;
    task.options.spatialFiltering.enabled=false;
    task.options.deposition.phiOffsetRadians=0;task.options.deposition.psiOffsetRadians=0;
    for(int y=-4;y<=4;++y) for(int x=-4;x<=4;++x) {
        sprayworkpiece::SurfaceSample sample;sample.position=Eigen::Vector3d(x*0.001,y*0.001,0);
        sample.normal=Eigen::Vector3d::UnitZ();task.workpiece.addSample(sample);
    }
    for(int y=0;y<8;++y) for(int x=0;x<8;++x) {
        const unsigned int i=y*9+x;
        task.workpiece.triangleIndices.insert(task.workpiece.triangleIndices.end(),{i,i+1,i+10,i,i+10,i+9});
    }
    spraytrajectory::SpraySegment segment;
    for(double time:{0.0,1.0}) {
        spraytrajectory::SprayPathPoint point;point.time=time;point.tcpPose.translation()=Eigen::Vector3d(0,0,0.12);
        point.sprayEnabled=true;segment.points.push_back(point);
    }
    task.trajectory.segments.push_back(segment);
    spraythickness::opengl::OpenGLThicknessPredictionBackend backend;
    const auto full=backend.predict(task);
    require(full.field.results.size()==81 && full.timing.valid, "GPU prediction output missing");
    const double middle=full.field.results[40].thickness;
    require(std::isfinite(middle) && middle>0 && middle>full.field.results.front().thickness,
        "Stationary spray footprint is invalid");
    require(std::abs(full.field.results.front().thickness-full.field.results.back().thickness)<middle*1e-4,
        "Centered footprint lost symmetry");
    task.options.spatialFiltering.enabled=true;task.options.spatialFiltering.fallbackToFullPrediction=false;
    const auto filtered=backend.predict(task);
    require(filtered.timing.spatialFiltering, "Spatial filtering silently fell back");
    double maximumError=0;
    for(std::size_t i=0;i<full.field.results.size();++i)
        maximumError=std::max(maximumError,std::abs(full.field.results[i].thickness-filtered.field.results[i].thickness));
    require(maximumError<std::max(1e-10,middle*1e-3), "Filtered GPU result differs from full prediction");
    task.trajectory.segments.front().points.back().time=2;
    const auto twice=backend.predict(task);
    require(std::abs(twice.field.results[40].thickness-2*filtered.field.results[40].thickness)<middle*1e-3,
        "Deposition does not follow exposure time");
    std::cout << "GPU full/filtered max error=" << maximumError << ", center thickness=" << middle
        << "; finite field, symmetry and exposure scaling: passed\n";
}

void uvTest()
{
    QTemporaryDir temporary;
    require(temporary.isValid(), "Cannot create UV test workspace");
    const auto directory=std::filesystem::u8path(temporary.path().toUtf8().constData());
    const auto mesh=directory/"sleeve.obj";
    {
        std::ofstream file(mesh);
        for(int ring=0;ring<3;++ring) for(int i=0;i<12;++i) {
            const double angle=6.283185307179586*i/12;
            const double radius=ring==1?30:40;
            file << "v " << radius*std::cos(angle) << ' ' << radius*std::sin(angle) << ' ' << ring*40 << '\n';
        }
        for(int ring=0;ring<2;++ring) for(int i=0;i<12;++i) {
            const int a=ring*12+i+1,b=ring*12+(i+1)%12+1,c=a+12,d=b+12;
            file << "f " << a << ' ' << b << ' ' << d << '\n';
            file << "f " << a << ' ' << d << ' ' << c << '\n';
        }
    }
    SprayTrajPlan::TrajPlanPipeline pipeline;
    require(pipeline.initialize(), "UV pipeline initialization failed");
    pipeline.setOptCutsRoot((directory/"optcuts").u8string());
    SprayTrajPlan::UvPlanningSession session;
    SprayTrajPlan::TrajectoryParams parameters;
    parameters.target_spacing=20;parameters.sample_step=5;parameters.trajectory_speed=50;
    require(!session.canExport(), "Empty UV session allows export");
    require(session.startUvMap(pipeline,mesh.u8string(),parameters), "UV unfolding failed");
    require(session.startGetTraj(pipeline,parameters), "UV scan trajectory generation failed");
    require(!pipeline.getTrajectory().empty(), "UV generator returned no segments");
    std::size_t count=0;
    for(const auto& segment:pipeline.getTrajectory()) for(const auto& point:segment.xyz) {
        require(point.allFinite(), "UV trajectory contains non-finite geometry");++count;
    }
    require(count>2, "UV trajectory has too few points");
    const auto exported=directory/"trajectory.paq";
    require(session.exportPaq(pipeline,exported.u8string(),parameters) && std::filesystem::file_size(exported)>0,
        "UV PAQ export failed");
    session.clearTrajectory(pipeline);
    require(session.state()==SprayTrajPlan::UvPlanningState::UVMapped && !session.canExport(),
        "Clearing UV trajectory leaves stale export enabled");
    std::cout << "Real UV unfolding, scan generation, PAQ export and stage invalidation: passed; points=" << count << '\n';
}

int main(int argc,char** argv)
{
    QGuiApplication app(argc,argv);
    try {
        if(argc>1 && std::string(argv[1])=="--calibration") calibrationTest();
        else if(argc>1 && std::string(argv[1])=="--uv-pipeline") uvTest();
        else thicknessTest();
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
