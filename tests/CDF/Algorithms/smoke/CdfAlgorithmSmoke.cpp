#include "CDFAlgorithms/CdfDocument.h"

#include <iostream>

int main()
{
    cdf::CdfDocument document;

    document.loadDemo(cdf::DemoCase::Repair);
    const cdf::PlanningResult repair = document.result();
    std::cout << "Repair demo min clearance: "
              << repair.statistics.minClearanceAfterRepair
              << ", collision-free: "
              << (repair.statistics.collisionFree ? "yes" : "no")
              << '\n';

    document.loadDemo(cdf::DemoCase::OmplSeed);
    const cdf::PlanningResult seeded = document.result();
    std::cout << "Seed demo min clearance: "
              << seeded.statistics.minClearanceAfterRepair
              << ", collision-free: "
              << (seeded.statistics.collisionFree ? "yes" : "no")
              << '\n';

    const bool ok = repair.statistics.collisionFree && seeded.statistics.collisionFree;
    return ok ? 0 : 1;
}
