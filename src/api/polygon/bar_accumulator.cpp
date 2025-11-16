#include "bar_accumulator.hpp"
#include <algorithm>
#include <cmath>

namespace AlpacaTrader {
namespace API {
namespace Polygon {

BarAccumulator::BarAccumulator(int firstLevelAccumulationSecondsValue, int secondLevelAccumulationSecondsValue, int maxBarHistorySizeValue)
    : firstLevelAccumulationSecondsValueParameter(firstLevelAccumulationSecondsValue)
    , secondLevelAccumulationSecondsValueParameter(secondLevelAccumulationSecondsValue)
    , maxBarHistorySizeValueParameter(maxBarHistorySizeValue)
    , currentFirstLevelAccumulationCountValue(0)
    , currentFirstLevelAccumulationWindowStartTimestamp(0)
    , currentSecondLevelAccumulationCountValue(0)
    , currentSecondLevelAccumulationWindowStartTimestamp(0)
{
    if (firstLevelAccumulationSecondsValueParameter <= 0) {
        throw std::runtime_error("First level accumulation period must be greater than 0");
    }
    
    if (secondLevelAccumulationSecondsValueParameter <= 0) {
        throw std::runtime_error("Second level accumulation period must be greater than 0");
    }
    
    if (secondLevelAccumulationSecondsValueParameter % firstLevelAccumulationSecondsValueParameter != 0) {
        throw std::runtime_error("Second level accumulation period must be a multiple of first level accumulation period");
    }
    
    if (maxBarHistorySizeValueParameter <= 0) {
        throw std::runtime_error("Maximum bar history size must be greater than 0");
    }
    
    currentFirstLevelAccumulatingBarData = Core::Bar{};
    currentSecondLevelAccumulatingBarData = Core::Bar{};
}

void BarAccumulator::addBar(const Core::Bar& incomingBarData) {
    std::lock_guard<std::mutex> stateGuard(accumulatorStateMutex);
    
    try {
        // For crypto, reject bars with invalid OHLC values
        // Only accept bars with valid positive prices
        if (incomingBarData.open_price <= 0.0 || incomingBarData.close_price <= 0.0 ||
            incomingBarData.high_price <= 0.0 || incomingBarData.low_price <= 0.0) {
            return;
        }
        
        long long barTimestampValue = 0;
        try {
            barTimestampValue = std::stoll(incomingBarData.timestamp);
        } catch (const std::exception&) {
            return;
        }
        
        if (currentFirstLevelAccumulationCountValue == 0) {
            currentFirstLevelAccumulationWindowStartTimestamp = barTimestampValue;
            currentFirstLevelAccumulatingBarData.open_price = incomingBarData.open_price;
            currentFirstLevelAccumulatingBarData.high_price = incomingBarData.high_price;
            currentFirstLevelAccumulatingBarData.low_price = incomingBarData.low_price;
            currentFirstLevelAccumulatingBarData.close_price = incomingBarData.close_price;
            currentFirstLevelAccumulatingBarData.volume = incomingBarData.volume;
            currentFirstLevelAccumulatingBarData.timestamp = incomingBarData.timestamp;
            currentFirstLevelAccumulationCountValue = 1;
        } else {
            currentFirstLevelAccumulatingBarData.high_price = std::max(currentFirstLevelAccumulatingBarData.high_price, incomingBarData.high_price);
            currentFirstLevelAccumulatingBarData.low_price = std::min(currentFirstLevelAccumulatingBarData.low_price, incomingBarData.low_price);
            currentFirstLevelAccumulatingBarData.close_price = incomingBarData.close_price;
            currentFirstLevelAccumulatingBarData.volume += incomingBarData.volume;
            currentFirstLevelAccumulationCountValue++;
            
            if (currentFirstLevelAccumulationCountValue >= firstLevelAccumulationSecondsValueParameter) {
                finalizeCurrentFirstLevelAccumulatedBar();
            }
        }
    } catch (const std::exception&) {
    } catch (...) {
    }
}

std::vector<Core::Bar> BarAccumulator::getAccumulatedBars(int maximumBarsRequested) const {
    std::lock_guard<std::mutex> stateGuard(accumulatorStateMutex);
    
    try {
        if (maximumBarsRequested <= 0) {
            return std::vector<Core::Bar>();
        }
        
        std::vector<Core::Bar> returnedBars;
        
        size_t firstLevelBarsCount = firstLevelAccumulatedBarsHistory.size();
        if (currentFirstLevelAccumulationCountValue > 0 && 
            currentFirstLevelAccumulatingBarData.open_price > 0.0 && 
            currentFirstLevelAccumulatingBarData.close_price > 0.0) {
            firstLevelBarsCount++;
        }
        
        if (firstLevelBarsCount >= static_cast<size_t>(maximumBarsRequested)) {
            if (!firstLevelAccumulatedBarsHistory.empty()) {
                size_t barsToReturnCount = std::min(static_cast<size_t>(maximumBarsRequested), firstLevelAccumulatedBarsHistory.size());
                returnedBars.reserve(barsToReturnCount);
                
                size_t startIndexValue = firstLevelAccumulatedBarsHistory.size() - barsToReturnCount;
                for (size_t i = startIndexValue; i < firstLevelAccumulatedBarsHistory.size(); ++i) {
                    returnedBars.push_back(firstLevelAccumulatedBarsHistory[i]);
                }
            }
            if (returnedBars.size() < static_cast<size_t>(maximumBarsRequested) && 
                currentFirstLevelAccumulationCountValue > 0 && 
                currentFirstLevelAccumulatingBarData.open_price > 0.0 && 
                currentFirstLevelAccumulatingBarData.close_price > 0.0) {
                returnedBars.push_back(currentFirstLevelAccumulatingBarData);
            }
        } else {
            size_t secondLevelBarsCount = secondLevelAccumulatedBarsHistory.size();
            if (currentSecondLevelAccumulationCountValue > 0 && 
                currentSecondLevelAccumulatingBarData.open_price > 0.0 && 
                currentSecondLevelAccumulatingBarData.close_price > 0.0) {
                secondLevelBarsCount++;
            }
            
            if (secondLevelBarsCount >= static_cast<size_t>(maximumBarsRequested)) {
                if (!secondLevelAccumulatedBarsHistory.empty()) {
                    size_t barsToReturnCount = std::min(static_cast<size_t>(maximumBarsRequested), secondLevelAccumulatedBarsHistory.size());
                    returnedBars.reserve(barsToReturnCount);
                    
                    size_t startIndexValue = secondLevelAccumulatedBarsHistory.size() - barsToReturnCount;
                    for (size_t i = startIndexValue; i < secondLevelAccumulatedBarsHistory.size(); ++i) {
                        returnedBars.push_back(secondLevelAccumulatedBarsHistory[i]);
                    }
                }
                if (returnedBars.size() < static_cast<size_t>(maximumBarsRequested) && 
                    currentSecondLevelAccumulationCountValue > 0 && 
                    currentSecondLevelAccumulatingBarData.open_price > 0.0 && 
                    currentSecondLevelAccumulatingBarData.close_price > 0.0) {
                    returnedBars.push_back(currentSecondLevelAccumulatingBarData);
                }
            } else {
                if (!firstLevelAccumulatedBarsHistory.empty()) {
                    returnedBars.insert(returnedBars.end(), firstLevelAccumulatedBarsHistory.begin(), firstLevelAccumulatedBarsHistory.end());
                }
                if (currentFirstLevelAccumulationCountValue > 0 && 
                    currentFirstLevelAccumulatingBarData.open_price > 0.0 && 
                    currentFirstLevelAccumulatingBarData.close_price > 0.0) {
                    returnedBars.push_back(currentFirstLevelAccumulatingBarData);
                }
                if (returnedBars.size() < static_cast<size_t>(maximumBarsRequested) && !secondLevelAccumulatedBarsHistory.empty()) {
                    size_t remainingBarsNeeded = maximumBarsRequested - static_cast<int>(returnedBars.size());
                    size_t barsToAddCount = std::min(static_cast<size_t>(remainingBarsNeeded), secondLevelAccumulatedBarsHistory.size());
                    size_t startIndexValue = secondLevelAccumulatedBarsHistory.size() - barsToAddCount;
                    for (size_t i = startIndexValue; i < secondLevelAccumulatedBarsHistory.size(); ++i) {
                        returnedBars.push_back(secondLevelAccumulatedBarsHistory[i]);
                    }
                }
            }
        }
        
        if (returnedBars.empty()) {
            return returnedBars;
        }
        
        try {
            std::sort(returnedBars.begin(), returnedBars.end(), [](const Core::Bar& barAValue, const Core::Bar& barBValue) {
                try {
                    long long timestampAValue = std::stoll(barAValue.timestamp);
                    long long timestampBValue = std::stoll(barBValue.timestamp);
                    return timestampAValue < timestampBValue;
                } catch (...) {
                    return barAValue.timestamp < barBValue.timestamp;
                }
            });
            
            auto uniqueIterator = std::unique(returnedBars.begin(), returnedBars.end(), [](const Core::Bar& barAValue, const Core::Bar& barBValue) {
                return barAValue.timestamp == barBValue.timestamp;
            });
            returnedBars.erase(uniqueIterator, returnedBars.end());
            
            if (static_cast<int>(returnedBars.size()) > maximumBarsRequested) {
                size_t startIndexValue = returnedBars.size() - static_cast<size_t>(maximumBarsRequested);
                returnedBars.erase(returnedBars.begin(), returnedBars.begin() + startIndexValue);
            }
        } catch (const std::exception&) {
            return std::vector<Core::Bar>();
        } catch (...) {
            return std::vector<Core::Bar>();
        }
        
        return returnedBars;
    } catch (const std::exception&) {
        return std::vector<Core::Bar>();
    } catch (...) {
        return std::vector<Core::Bar>();
    }
}

size_t BarAccumulator::getAccumulatedBarsCount() const {
    std::lock_guard<std::mutex> stateGuard(accumulatorStateMutex);
    
    size_t secondLevelBarsCount = secondLevelAccumulatedBarsHistory.size();
    if (currentSecondLevelAccumulationCountValue > 0 && 
        currentSecondLevelAccumulatingBarData.open_price > 0.0 && 
        currentSecondLevelAccumulatingBarData.close_price > 0.0) {
        secondLevelBarsCount++;
    }
    
    size_t firstLevelBarsCount = firstLevelAccumulatedBarsHistory.size();
    if (currentFirstLevelAccumulationCountValue > 0 && 
        currentFirstLevelAccumulatingBarData.open_price > 0.0 && 
        currentFirstLevelAccumulatingBarData.close_price > 0.0) {
        firstLevelBarsCount++;
    }
    
    return std::max(secondLevelBarsCount, firstLevelBarsCount);
}

size_t BarAccumulator::getFirstLevelBarsCount() const {
    std::lock_guard<std::mutex> stateGuard(accumulatorStateMutex);
    return firstLevelAccumulatedBarsHistory.size();
}

size_t BarAccumulator::getSecondLevelBarsCount() const {
    std::lock_guard<std::mutex> stateGuard(accumulatorStateMutex);
    return secondLevelAccumulatedBarsHistory.size();
}

void BarAccumulator::clearAccumulatedBars() {
    std::lock_guard<std::mutex> stateGuard(accumulatorStateMutex);
    firstLevelAccumulatedBarsHistory.clear();
    secondLevelAccumulatedBarsHistory.clear();
    currentFirstLevelAccumulationCountValue = 0;
    currentFirstLevelAccumulatingBarData = Core::Bar{};
    currentFirstLevelAccumulationWindowStartTimestamp = 0;
    currentSecondLevelAccumulationCountValue = 0;
    currentSecondLevelAccumulatingBarData = Core::Bar{};
    currentSecondLevelAccumulationWindowStartTimestamp = 0;
}

void BarAccumulator::finalizeCurrentFirstLevelAccumulatedBar() {
    if (currentFirstLevelAccumulationCountValue > 0) {
        Core::Bar completedFirstLevelBarData = currentFirstLevelAccumulatingBarData;
        firstLevelAccumulatedBarsHistory.push_back(completedFirstLevelBarData);
        
        if (static_cast<int>(firstLevelAccumulatedBarsHistory.size()) > maxBarHistorySizeValueParameter) {
            size_t barsToKeepCount = static_cast<size_t>(maxBarHistorySizeValueParameter);
            firstLevelAccumulatedBarsHistory.erase(firstLevelAccumulatedBarsHistory.begin(), 
                                                  firstLevelAccumulatedBarsHistory.begin() + (firstLevelAccumulatedBarsHistory.size() - barsToKeepCount));
        }
        
        processCompletedFirstLevelBar(completedFirstLevelBarData);
    }
    
    currentFirstLevelAccumulationCountValue = 0;
    currentFirstLevelAccumulatingBarData = Core::Bar{};
    currentFirstLevelAccumulationWindowStartTimestamp = 0;
}

void BarAccumulator::finalizeCurrentSecondLevelAccumulatedBar() {
    if (currentSecondLevelAccumulationCountValue > 0) {
        secondLevelAccumulatedBarsHistory.push_back(currentSecondLevelAccumulatingBarData);
        
        if (static_cast<int>(secondLevelAccumulatedBarsHistory.size()) > maxBarHistorySizeValueParameter) {
            size_t barsToKeepCount = static_cast<size_t>(maxBarHistorySizeValueParameter);
            secondLevelAccumulatedBarsHistory.erase(secondLevelAccumulatedBarsHistory.begin(), 
                                                   secondLevelAccumulatedBarsHistory.begin() + (secondLevelAccumulatedBarsHistory.size() - barsToKeepCount));
        }
    }
    
    currentSecondLevelAccumulationCountValue = 0;
    currentSecondLevelAccumulatingBarData = Core::Bar{};
    currentSecondLevelAccumulationWindowStartTimestamp = 0;
}

void BarAccumulator::processCompletedFirstLevelBar(const Core::Bar& completedFirstLevelBarData) {
    long long barTimestampValue = 0;
    try {
        barTimestampValue = std::stoll(completedFirstLevelBarData.timestamp);
    } catch (const std::exception&) {
        return;
    }
    
    if (currentSecondLevelAccumulationCountValue == 0) {
        currentSecondLevelAccumulationWindowStartTimestamp = barTimestampValue;
        currentSecondLevelAccumulatingBarData.open_price = completedFirstLevelBarData.open_price;
        currentSecondLevelAccumulatingBarData.high_price = completedFirstLevelBarData.high_price;
        currentSecondLevelAccumulatingBarData.low_price = completedFirstLevelBarData.low_price;
        currentSecondLevelAccumulatingBarData.close_price = completedFirstLevelBarData.close_price;
        currentSecondLevelAccumulatingBarData.volume = completedFirstLevelBarData.volume;
        currentSecondLevelAccumulatingBarData.timestamp = completedFirstLevelBarData.timestamp;
        currentSecondLevelAccumulationCountValue = 1;
    } else {
        currentSecondLevelAccumulatingBarData.high_price = std::max(currentSecondLevelAccumulatingBarData.high_price, completedFirstLevelBarData.high_price);
        currentSecondLevelAccumulatingBarData.low_price = std::min(currentSecondLevelAccumulatingBarData.low_price, completedFirstLevelBarData.low_price);
        currentSecondLevelAccumulatingBarData.close_price = completedFirstLevelBarData.close_price;
        currentSecondLevelAccumulatingBarData.volume += completedFirstLevelBarData.volume;
        currentSecondLevelAccumulatingBarData.timestamp = completedFirstLevelBarData.timestamp;
        currentSecondLevelAccumulationCountValue++;
        
        int secondLevelBarsRequired = secondLevelAccumulationSecondsValueParameter / firstLevelAccumulationSecondsValueParameter;
        if (currentSecondLevelAccumulationCountValue >= secondLevelBarsRequired) {
            finalizeCurrentSecondLevelAccumulatedBar();
        }
    }
}


} // namespace Polygon
} // namespace API
} // namespace AlpacaTrader

