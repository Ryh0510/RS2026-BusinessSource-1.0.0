#pragma once

#include "CDFAlgorithms/CdfDocument.h"
#include "CdfDemoViewModel.h"

namespace cdf_gui
{
    class CdfDemoViewModelBuilder final
    {
    public:
        CdfDemoViewModel build(const cdf::CdfDocument& document) const;
    };
}
