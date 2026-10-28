#include "PaintingAnalysisDialogService.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

namespace robot_qt_viewer
{
    QString PaintingAnalysisDialogService::selectModelFile(
        QWidget* parent,
        const QString& title,
        const QString& filter)
    {
        return QFileDialog::getOpenFileName(
            parent,
            title,
            QString(),
            filter);
    }

    bool PaintingAnalysisDialogService::selectModelUnitScale(
        QWidget* parent,
        const QString& modelPath,
        double& scaleToMeters)
    {
        QMessageBox dialog(parent);
        dialog.setWindowTitle(QStringLiteral("Model Unit"));
        dialog.setIcon(QMessageBox::Question);
        dialog.setText(QStringLiteral("Select the coordinate unit of %1.")
            .arg(QFileInfo(modelPath).fileName()));
        dialog.setInformativeText(QStringLiteral(
            "STL and OBJ files usually do not store unit metadata. "
            "Millimeters converts model coordinates to meters; meters keeps them unchanged."));

        QPushButton* millimeterButton = dialog.addButton(
            QStringLiteral("Millimeters (mm)"),
            QMessageBox::AcceptRole);
        QPushButton* meterButton = dialog.addButton(
            QStringLiteral("Meters (m)"),
            QMessageBox::AcceptRole);
        dialog.addButton(QMessageBox::Cancel);
        dialog.setDefaultButton(millimeterButton);
        dialog.exec();

        if(dialog.clickedButton() == millimeterButton) {
            scaleToMeters = 0.001;
            return true;
        }
        if(dialog.clickedButton() == meterButton) {
            scaleToMeters = 1.0;
            return true;
        }
        return false;
    }

    bool PaintingAnalysisDialogService::selectTrajectoryUnitScale(
        QWidget* parent,
        const QString& trajectoryPath,
        double& scaleToMeters)
    {
        QMessageBox dialog(parent);
        dialog.setWindowTitle(QStringLiteral("轨迹单位"));
        dialog.setIcon(QMessageBox::Question);
        dialog.setText(QStringLiteral("请选择轨迹文件 %1 的长度单位。")
            .arg(QFileInfo(trajectoryPath).fileName()));
        dialog.setInformativeText(QStringLiteral(
            "毫米 (mm)：位置和距离乘以 0.001 转换为米；米 (m)：保持不变。"
            "速度、加速度的长度单位同步转换；姿态不变，时间戳按秒读取。"
            "请按文件实际单位选择，模型和轨迹可以使用不同的源单位。"));
        QPushButton* millimeterButton = dialog.addButton(
            QStringLiteral("毫米 (mm)"), QMessageBox::AcceptRole);
        QPushButton* meterButton = dialog.addButton(
            QStringLiteral("米 (m)"), QMessageBox::AcceptRole);
        dialog.addButton(QMessageBox::Cancel);
        dialog.setDefaultButton(millimeterButton);
        dialog.exec();
        if(dialog.clickedButton() == millimeterButton) {
            scaleToMeters = 0.001;
            return true;
        }
        if(dialog.clickedButton() == meterButton) {
            scaleToMeters = 1.0;
            return true;
        }
        return false;
    }

    QString PaintingAnalysisDialogService::selectTrajectoryFile(
        QWidget* parent,
        const QString& title)
    {
        return QFileDialog::getOpenFileName(
            parent,
            title,
            QString(),
            QStringLiteral("Spray Trajectory (*.paq *.txt);;All Files (*.*)"));
    }
}
