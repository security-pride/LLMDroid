#ifndef CODE_COVERAGE_MONITOR_H
#define CODE_COVERAGE_MONITOR_H

#include <vector>

class CodeCoverageMonitor {
public:
    CodeCoverageMonitor(int windowSize = 80, double minGrowthRate = 0.05, double adjustmentFactor = 1.0);

    /**
     * adjust threshold based on cv history
     * @param currentCoverage
     * @return pair: first: current growth rate,
     * second: minGrowthRate if total cv num < windowSize, else adjusted threshold
     * @note call from main thread
     */
    std::pair<double, double> update(double currentCoverage);

//    void setWindowSize(int wsize) { this->windowSize = wsize; }

private:
    std::vector<double> coverageHistory;
    const int windowSize;
    const double minGrowthRate;
    const double adjustmentFactor;
    const double _minThreshold = 0.01;
    double _adjustedThreshold;
    double _growthRateSum = 0.0;
    double calculateDynamicBaselineGrowthRate();
};

#endif // CODE_COVERAGE_MONITOR_H