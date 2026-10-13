#include "parallel_pixels/statistics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace parallel_pixels {

double median(const std::vector<double>& samples) {
  if (samples.empty()) throw std::invalid_argument("median requires at least one sample");
  auto sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  const auto middle = sorted.size() / 2;
  return sorted.size() % 2 == 0 ? (sorted[middle - 1] + sorted[middle]) / 2.0 : sorted[middle];
}

double median_absolute_deviation(const std::vector<double>& samples) {
  const auto center = median(samples);
  std::vector<double> deviations;
  deviations.reserve(samples.size());
  for (const auto sample : samples) deviations.push_back(std::abs(sample - center));
  return median(deviations);
}

}  // namespace parallel_pixels
