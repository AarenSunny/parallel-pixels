#pragma once

#include <filesystem>

#include "parallel_pixels/image.hpp"

namespace parallel_pixels {

[[nodiscard]] GrayImage read_pgm(const std::filesystem::path& path);
void write_pgm(const std::filesystem::path& path, const GrayImage& image);

}  // namespace parallel_pixels
