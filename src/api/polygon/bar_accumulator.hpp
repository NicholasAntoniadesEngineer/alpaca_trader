#ifndef BAR_ACCUMULATOR_HPP
#define BAR_ACCUMULATOR_HPP

#include "trader/data_structures/data_structures.hpp"
#include <vector>
#include <mutex>
#include <chrono>
#include <string>

namespace AlpacaTrader {
namespace API {
namespace Polygon {

class BarAccumulator {
public:
    BarAccumulator(int firstLevelAccumulationSecondsValue, int secondLevelAccumulationSecondsValue, int maxBarHistorySizeValue);
    
    void addBar(const Core::Bar& incomingBarData);
    std::vector<Core::Bar> getAccumulatedBars(int maximumBarsRequested) const;
    size_t getAccumulatedBarsCount() const;
    size_t getFirstLevelBarsCount() const;
    size_t getSecondLevelBarsCount() const;
    void clearAccumulatedBars();
    
private:
    int firstLevelAccumulationSecondsValueParameter;
    int secondLevelAccumulationSecondsValueParameter;
    int maxBarHistorySizeValueParameter;
    mutable std::mutex accumulatorStateMutex;
    
    std::vector<Core::Bar> firstLevelAccumulatedBarsHistory;
    Core::Bar currentFirstLevelAccumulatingBarData;
    int currentFirstLevelAccumulationCountValue;
    long long currentFirstLevelAccumulationWindowStartTimestamp;
    
    std::vector<Core::Bar> secondLevelAccumulatedBarsHistory;
    Core::Bar currentSecondLevelAccumulatingBarData;
    int currentSecondLevelAccumulationCountValue;
    long long currentSecondLevelAccumulationWindowStartTimestamp;
    
    void finalizeCurrentFirstLevelAccumulatedBar();
    void finalizeCurrentSecondLevelAccumulatedBar();
    void processCompletedFirstLevelBar(const Core::Bar& completedFirstLevelBarData);
};

} // namespace Polygon
} // namespace API
} // namespace AlpacaTrader

#endif // BAR_ACCUMULATOR_HPP

