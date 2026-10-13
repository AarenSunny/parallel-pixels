#pragma once

#include <vector>

namespace parallel_pixels {

[[nodiscard]] double median(const std::vector<double>& samples);
[[nodiscard]] double median_absolute_deviation(const std::vector<double>& samples);

}  // namespace parallel_pixels
