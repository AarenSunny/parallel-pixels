#include "parallel_pixels/statistics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace parallel_pixels {
namespace {

double median_unchecked(std::vector<double> samples) {
  std::sort(samples.begin(), samples.end());
  const auto middle = samples.size() / 2;
  return samples.size() % 2 == 0 ? std::midpoint(samples[middle - 1], samples[middle])
                                 : samples[middle];
}

double absolute_distance(const double left, const double right) {
  if (std::signbit(left) == std::signbit(right)) return std::abs(left - right);

  const auto left_magnitude = std::abs(left);
  const auto right_magnitude = std::abs(right);
  if (left_magnitude > std::numeric_limits<double>::max() - right_magnitude) {
    // An out-of-range distance can only be on one side of the sample median.
    // Infinity preserves its tail rank without poisoning the finite MAD center.
    return std::numeric_limits<double>::infinity();
  }
  return left_magnitude + right_magnitude;
}

}  // namespace

double median(const std::vector<double>& samples) {
  if (samples.empty()) throw std::invalid_argument("median requires at least one sample");
  if (!std::all_of(samples.begin(), samples.end(),
                   [](const double sample) { return std::isfinite(sample); })) {
    throw std::invalid_argument("median samples must be finite");
  }
  return median_unchecked(samples);
}

double median_absolute_deviation(const std::vector<double>& samples) {
  const auto center = median(samples);
  std::vector<double> deviations;
  deviations.reserve(samples.size());
  for (const auto sample : samples) deviations.push_back(absolute_distance(sample, center));

  const auto result = median_unchecked(std::move(deviations));
  if (!std::isfinite(result)) {
    throw std::overflow_error("median absolute deviation exceeded the finite range");
  }
  return result;
}

}  // namespace parallel_pixels
