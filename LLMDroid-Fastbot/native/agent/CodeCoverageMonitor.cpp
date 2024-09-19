#include "CodeCoverageMonitor.h"
#include <cmath>
#include "Base.h"

CodeCoverageMonitor::CodeCoverageMonitor(int windowSize, double minGrowthRate, double adjustmentFactor):
    windowSize(windowSize), minGrowthRate(minGrowthRate), adjustmentFactor(adjustmentFactor)
{
    _adjustedThreshold = minGrowthRate;
}

std::pair<double, double> CodeCoverageMonitor::update(double currentCoverage) {
    coverageHistory.push_back(currentCoverage);
    int n = coverageHistory.size();
    double currentGrowthRate = 0.0;
    if (n >= 2) {
        // gn = (xn - xn-1) / xn-1
        currentGrowthRate = (currentCoverage - coverageHistory[n - 2]) / coverageHistory[n - 2];
        // g1 + g2 + ... + gn
        _growthRateSum += currentGrowthRate;
        fastbotx::callJavaLogger(MAIN_THREAD, "[CV_Monitor] growth rate: %f, growthRateSum: %f", currentGrowthRate, _growthRateSum);
    }

    if (n >= windowSize) {
        // G
        double dynamicBaselineGrowthRate = _growthRateSum / (n - 1);
        // delta_g = gn - G
        double growthRateDifference = currentGrowthRate - dynamicBaselineGrowthRate;
        fastbotx::callJavaLogger(MAIN_THREAD, "[CV_Monitor] Dynamic Baseline: %f, delta_g: %f", dynamicBaselineGrowthRate, growthRateDifference);

        // Tn = T0 * exp(k * delta_g)
        double adjustedThreshold = _adjustedThreshold * std::exp(adjustmentFactor * growthRateDifference);
        _adjustedThreshold = adjustedThreshold > _minThreshold ?  adjustedThreshold : _minThreshold;
        return std::make_pair(currentGrowthRate, _adjustedThreshold);
    }

    return std::make_pair(currentGrowthRate, minGrowthRate);;
}

double CodeCoverageMonitor::calculateDynamicBaselineGrowthRate() {
    int n = coverageHistory.size();
    double sumGrowthRates = 0.0;
    for (int i = 1; i < n; i++) {
        double growthRate = (coverageHistory[i] - coverageHistory[i - 1]) / coverageHistory[i - 1];
        sumGrowthRates += growthRate;
    }

    return sumGrowthRates / (n - 1);
}
