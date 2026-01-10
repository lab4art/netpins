#pragma once

#include "SensorProcessor.h"
#include <vector>
#include <deque>
#include <algorithm>

/**
 * Simple moving average - averages last N values
 */
class MovingAverageProcessor : public SensorProcessor {
    private:
        size_t windowSize;
        std::deque<float> values;
        float sum;
        
    public:
        MovingAverageProcessor(size_t size)
            : SensorProcessor("MovingAverage"),
              windowSize(size),
              sum(0.0f) {
            
            if (size == 0) {
                Log::errorln("MovingAverage: Window size must be > 0");
                windowSize = 1;
            }
        }
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // Add new value
            values.push_back(data.value);
            sum += data.value;
            
            // Remove oldest value if window is full
            if (values.size() > windowSize) {
                sum -= values.front();
                values.pop_front();
            }
            
            // Calculate average
            result.value = sum / values.size();
            return result;
        }
        
        void reset() override {
            values.clear();
            sum = 0.0f;
        }
};

/**
 * Exponential moving average - gives more weight to recent values
 * Lower alpha = smoother but slower response
 * Higher alpha = faster response but less smoothing
 */
class ExponentialMovingAverageProcessor : public SensorProcessor {
    private:
        float alpha;  // Smoothing factor (0-1)
        float emaValue;
        bool initialized;
        
    public:
        ExponentialMovingAverageProcessor(float smoothingFactor = 0.3f)
            : SensorProcessor("ExponentialMovingAverage"),
              alpha(smoothingFactor),
              emaValue(0.0f),
              initialized(false) {
            
            if (alpha <= 0.0f || alpha > 1.0f) {
                Log::errorln("EMA: Alpha must be in range (0, 1]");
                alpha = 0.3f;
            }
        }
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            if (!initialized) {
                emaValue = data.value;
                initialized = true;
            } else {
                emaValue = alpha * data.value + (1.0f - alpha) * emaValue;
            }
            
            result.value = emaValue;
            return result;
        }
        
        void reset() override {
            initialized = false;
            emaValue = 0.0f;
        }
};

/**
 * Median filter - returns median of last N values
 * Excellent for removing outliers and spike noise
 */
class MedianFilterProcessor : public SensorProcessor {
    private:
        size_t windowSize;
        std::deque<float> values;
        
    public:
        MedianFilterProcessor(size_t size)
            : SensorProcessor("MedianFilter"),
              windowSize(size) {
            
            if (size == 0) {
                Log::errorln("MedianFilter: Window size must be > 0");
                windowSize = 1;
            }
        }
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // Add new value
            values.push_back(data.value);
            
            // Remove oldest value if window is full
            if (values.size() > windowSize) {
                values.pop_front();
            }
            
            // Calculate median
            std::vector<float> sorted(values.begin(), values.end());
            std::sort(sorted.begin(), sorted.end());
            
            size_t mid = sorted.size() / 2;
            if (sorted.size() % 2 == 0) {
                result.value = (sorted[mid - 1] + sorted[mid]) / 2.0f;
            } else {
                result.value = sorted[mid];
            }
            
            return result;
        }
        
        void reset() override {
            values.clear();
        }
};

/**
 * Weighted moving average - more recent values have more weight
 */
class WeightedMovingAverageProcessor : public SensorProcessor {
    private:
        size_t windowSize;
        std::deque<float> values;
        std::vector<float> weights;
        float totalWeight;
        
    public:
        WeightedMovingAverageProcessor(size_t size, const std::vector<float>& customWeights = {})
            : SensorProcessor("WeightedMovingAverage"),
              windowSize(size),
              totalWeight(0.0f) {
            
            if (customWeights.empty()) {
                // Linear weights: [1, 2, 3, ..., n]
                for (size_t i = 1; i <= size; i++) {
                    weights.push_back(static_cast<float>(i));
                    totalWeight += static_cast<float>(i);
                }
            } else {
                weights = customWeights;
                for (float w : weights) {
                    totalWeight += w;
                }
            }
        }
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            // Add new value
            values.push_back(data.value);
            
            // Remove oldest value if window is full
            if (values.size() > windowSize) {
                values.pop_front();
            }
            
            // Calculate weighted average
            float weightedSum = 0.0f;
            float appliedWeight = 0.0f;
            size_t idx = 0;
            
            for (size_t i = weights.size() - values.size(); i < weights.size(); i++) {
                weightedSum += values[idx] * weights[i];
                appliedWeight += weights[i];
                idx++;
            }
            
            result.value = weightedSum / appliedWeight;
            return result;
        }
        
        void reset() override {
            values.clear();
        }
};

/**
 * Low-pass filter - simple RC-style low-pass filter
 * Good for smoothing noisy signals
 */
class LowPassFilterProcessor : public SensorProcessor {
    private:
        float filterCoefficient;  // 0-1, higher = less filtering
        float filteredValue;
        bool initialized;
        
    public:
        LowPassFilterProcessor(float coefficient = 0.1f)
            : SensorProcessor("LowPassFilter"),
              filterCoefficient(coefficient),
              filteredValue(0.0f),
              initialized(false) {
            
            if (coefficient <= 0.0f || coefficient > 1.0f) {
                Log::errorln("LowPassFilter: Coefficient must be in range (0, 1]");
                filterCoefficient = 0.1f;
            }
        }
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            if (!initialized) {
                filteredValue = data.value;
                initialized = true;
            } else {
                filteredValue = filteredValue + filterCoefficient * (data.value - filteredValue);
            }
            
            result.value = filteredValue;
            return result;
        }
        
        void reset() override {
            initialized = false;
            filteredValue = 0.0f;
        }
};

/**
 * Kalman filter - optimal estimator for linear systems with noise
 * Good for sensors with known measurement noise characteristics
 */
class KalmanFilterProcessor : public SensorProcessor {
    private:
        float processNoise;      // Q - process variance
        float measurementNoise;  // R - measurement variance
        float estimate;          // Current estimate
        float errorCovariance;   // P - estimation error covariance
        bool initialized;
        
    public:
        KalmanFilterProcessor(float procNoise = 0.01f, float measNoise = 0.1f)
            : SensorProcessor("KalmanFilter"),
              processNoise(procNoise),
              measurementNoise(measNoise),
              estimate(0.0f),
              errorCovariance(1.0f),
              initialized(false) {}
        
        SensorData process(const SensorData& data) override {
            if (!enabled) return data;
            
            SensorData result = data;
            
            if (!initialized) {
                estimate = data.value;
                initialized = true;
                result.value = estimate;
                return result;
            }
            
            // Prediction step
            float predictedErrorCovariance = errorCovariance + processNoise;
            
            // Update step
            float kalmanGain = predictedErrorCovariance / (predictedErrorCovariance + measurementNoise);
            estimate = estimate + kalmanGain * (data.value - estimate);
            errorCovariance = (1.0f - kalmanGain) * predictedErrorCovariance;
            
            result.value = estimate;
            return result;
        }
        
        void reset() override {
            initialized = false;
            estimate = 0.0f;
            errorCovariance = 1.0f;
        }
};
