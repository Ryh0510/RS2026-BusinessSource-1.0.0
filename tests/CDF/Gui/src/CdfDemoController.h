#pragma once

#include "CDFAlgorithms/CdfDocument.h"
#include "CdfDemoViewModel.h"
#include "CdfDemoViewModelBuilder.h"

#include <QObject>
#include <QVector>

namespace cdf_gui
{
    class CdfDemoController final : public QObject
    {
        Q_OBJECT

    public:
        explicit CdfDemoController(QObject* parent = nullptr);

        CdfDemoViewModel currentViewModel() const;
        void planFromEndpoints(const QVector<double>& startJoints, const QVector<double>& goalJoints);

    public slots:
        void showDistanceFieldCase();
        void showOmplSeedCase();
        void showRepairCase();
        void setTargetClearance(double clearance);
        void rerun();

    signals:
        void viewModelChanged(const cdf_gui::CdfDemoViewModel& model);
        void statusMessageRequested(const QString& message, int timeoutMs);

    private:
        void publish();

        cdf::CdfDocument m_document;
        CdfDemoViewModelBuilder m_builder;
    };
}
