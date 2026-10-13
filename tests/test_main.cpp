#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "parallel_pixels/blur.hpp"
#include "parallel_pixels/image.hpp"
#include "parallel_pixels/pgm.hpp"

namespace {

using parallel_pixels::GrayImage;

void expect(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void expect_throws(Function&& function, const std::string& message) {
  try {
    function();
  } catch (const std::exception&) {
    return;
  }
  throw std::runtime_error(message);
}

void test_constant_image_stays_constant() {
  GrayImage input(11, 7, std::vector<std::uint8_t>(77, 93));
  expect(parallel_pixels::gaussian_blur_scalar(input) == input, "constant image changed after blur");
}

void test_impulse_matches_kernel() {
  GrayImage input(3, 3);
  input.at(1, 1) = 160;
  const auto output = parallel_pixels::gaussian_blur_scalar(input);
  const std::array<std::uint8_t, 9> expected{{10, 20, 10, 20, 40, 20, 10, 20, 10}};
  expect(std::equal(output.pixels().begin(), output.pixels().end(), expected.begin()),
         "impulse response did not match the fixed-point Gaussian kernel");
}

void test_parallel_matches_scalar_across_shapes_and_workers() {
  for (const auto& [width, height] : std::array<std::pair<std::size_t, std::size_t>, 5>{{
           {1, 1}, {2, 7}, {7, 2}, {17, 19}, {64, 33},
       }}) {
    GrayImage input(width, height);
    for (std::size_t index = 0; index < input.pixels().size(); ++index) {
      input.pixels()[index] = static_cast<std::uint8_t>((index * 37U + width * 11U + height) % 256U);
    }
    const auto expected = parallel_pixels::gaussian_blur_scalar(input);
    for (const auto workers : {1U, 2U, 3U, 16U}) {
      expect(parallel_pixels::gaussian_blur_parallel(input, workers) == expected,
             "parallel output differed from scalar output");
    }
  }
}

void test_effective_worker_count_respects_image_height() {
  const GrayImage short_image(32, 3);
  expect(parallel_pixels::effective_worker_count(short_image, 2) == 2,
         "worker count changed when enough rows were available");
  expect(parallel_pixels::effective_worker_count(short_image, 16) == 3,
         "worker count was not capped by image height");
}

void test_pgm_round_trip() {
  const auto path = std::filesystem::temp_directory_path() / "parallel-pixels-round-trip.pgm";
  GrayImage image(4, 2, {0, 9, 10, 32, 127, 128, 254, 255});
  parallel_pixels::write_pgm(path, image);
  expect(parallel_pixels::read_pgm(path) == image, "PGM round trip changed image bytes");
  std::filesystem::remove(path);
}

void test_invalid_inputs_are_rejected() {
  expect_throws([] { static_cast<void>(GrayImage(0, 1)); }, "zero-width image was accepted");
  expect_throws([] {
    GrayImage image(1, 1);
    static_cast<void>(parallel_pixels::gaussian_blur_parallel(image, 0));
  }, "zero worker count was accepted");
  expect_throws([] {
    GrayImage image(1, 1);
    static_cast<void>(parallel_pixels::gaussian_blur_parallel(
        image, parallel_pixels::kMaximumWorkerCount + 1));
  }, "unsafe worker count was accepted");

  const auto path = std::filesystem::temp_directory_path() / "parallel-pixels-invalid.pgm";
  {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "P5\n2 2\n255\nabc";
  }
  expect_throws([&] { static_cast<void>(parallel_pixels::read_pgm(path)); },
                "truncated PGM raster was accepted");
  std::filesystem::remove(path);
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests{
      {"constant image stays constant", test_constant_image_stays_constant},
      {"impulse response matches kernel", test_impulse_matches_kernel},
      {"parallel output matches scalar output", test_parallel_matches_scalar_across_shapes_and_workers},
      {"effective worker count respects image height", test_effective_worker_count_respects_image_height},
      {"PGM round trip preserves bytes", test_pgm_round_trip},
      {"invalid inputs are rejected", test_invalid_inputs_are_rejected},
  };
  std::size_t passed = 0;
  for (const auto& [name, test] : tests) {
    try {
      test();
      ++passed;
      std::cout << "PASS " << name << '\n';
    } catch (const std::exception& error) {
      std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
  }
  std::cout << passed << '/' << tests.size() << " tests passed\n";
  return passed == tests.size() ? 0 : 1;
}
