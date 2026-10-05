#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "parallel_pixels/blur.hpp"
#include "parallel_pixels/image.hpp"
#include "parallel_pixels/pgm.hpp"

namespace {

using Clock = std::chrono::steady_clock;
using parallel_pixels::GrayImage;

struct BenchmarkOptions {
  std::size_t width = 2048;
  std::size_t height = 2048;
  std::size_t workers = parallel_pixels::default_worker_count();
  std::size_t iterations = 7;
  bool json = false;
};

std::size_t parse_positive(std::string_view value, std::string_view option) {
  std::size_t consumed = 0;
  unsigned long long parsed = 0;
  try {
    parsed = std::stoull(std::string(value), &consumed);
  } catch (const std::exception&) {
    throw std::invalid_argument(std::string(option) + " must be a positive integer");
  }
  if (consumed != value.size() || parsed == 0 || parsed > std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument(std::string(option) + " must be a positive integer");
  }
  return static_cast<std::size_t>(parsed);
}

std::string_view option_value(int& index, int argc, char** argv) {
  if (index + 1 >= argc) throw std::invalid_argument(std::string(argv[index]) + " requires a value");
  return argv[++index];
}

BenchmarkOptions benchmark_options(int argc, char** argv) {
  BenchmarkOptions options;
  for (int index = 2; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--width") options.width = parse_positive(option_value(index, argc, argv), argument);
    else if (argument == "--height") options.height = parse_positive(option_value(index, argc, argv), argument);
    else if (argument == "--threads") options.workers = parse_positive(option_value(index, argc, argv), argument);
    else if (argument == "--iterations") options.iterations = parse_positive(option_value(index, argc, argv), argument);
    else if (argument == "--json") options.json = true;
    else throw std::invalid_argument("unknown benchmark option: " + std::string(argument));
  }
  if (options.width > 16'384 || options.height > 16'384) {
    throw std::invalid_argument("benchmark dimensions are capped at 16384 pixels");
  }
  if (options.workers > parallel_pixels::kMaximumWorkerCount) {
    throw std::invalid_argument("thread count is capped at 256");
  }
  if (options.iterations > 100) throw std::invalid_argument("iterations are capped at 100");
  return options;
}

GrayImage synthetic_image(std::size_t width, std::size_t height) {
  GrayImage image(width, height);
  std::uint32_t state = 0x9E3779B9U;
  for (auto& pixel : image.pixels()) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    pixel = static_cast<std::uint8_t>(state & 0xFFU);
  }
  return image;
}

double elapsed_ms(Clock::time_point begin, Clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

double median(std::vector<double> samples) {
  std::sort(samples.begin(), samples.end());
  const auto middle = samples.size() / 2;
  return samples.size() % 2 == 0 ? (samples[middle - 1] + samples[middle]) / 2.0 : samples[middle];
}

std::uint64_t checksum(const GrayImage& image) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto pixel : image.pixels()) {
    hash ^= pixel;
    hash *= 1099511628211ULL;
  }
  return hash;
}

int run_benchmark(int argc, char** argv) {
  const auto options = benchmark_options(argc, argv);
  const auto input = synthetic_image(options.width, options.height);
  const auto scalar_reference = parallel_pixels::gaussian_blur_scalar(input);
  const auto parallel_reference = parallel_pixels::gaussian_blur_parallel(input, options.workers);
  if (scalar_reference != parallel_reference) throw std::runtime_error("parallel result differs from scalar reference");

  std::vector<double> scalar_samples;
  std::vector<double> parallel_samples;
  scalar_samples.reserve(options.iterations);
  parallel_samples.reserve(options.iterations);
  for (std::size_t iteration = 0; iteration < options.iterations; ++iteration) {
    auto measure_scalar = [&] {
      const auto begin = Clock::now();
      const auto result = parallel_pixels::gaussian_blur_scalar(input);
      const auto end = Clock::now();
      if (result != scalar_reference) throw std::runtime_error("scalar result changed between iterations");
      scalar_samples.push_back(elapsed_ms(begin, end));
    };
    auto measure_parallel = [&] {
      const auto begin = Clock::now();
      const auto result = parallel_pixels::gaussian_blur_parallel(input, options.workers);
      const auto end = Clock::now();
      if (result != scalar_reference) throw std::runtime_error("parallel result differs from scalar reference");
      parallel_samples.push_back(elapsed_ms(begin, end));
    };
    if (iteration % 2 == 0) {
      measure_scalar();
      measure_parallel();
    } else {
      measure_parallel();
      measure_scalar();
    }
  }

  const auto scalar_median = median(scalar_samples);
  const auto parallel_median = median(parallel_samples);
  const auto speedup = scalar_median / parallel_median;
  if (options.json) {
    std::cout << std::fixed << std::setprecision(3)
              << "{\n"
              << "  \"width\": " << options.width << ",\n"
              << "  \"height\": " << options.height << ",\n"
              << "  \"threads\": " << options.workers << ",\n"
              << "  \"iterations\": " << options.iterations << ",\n"
              << "  \"scalarMedianMs\": " << scalar_median << ",\n"
              << "  \"parallelMedianMs\": " << parallel_median << ",\n"
              << "  \"speedup\": " << speedup << ",\n"
              << "  \"exactMatch\": true,\n"
              << "  \"outputChecksum\": \"" << std::hex << checksum(scalar_reference) << std::dec << "\",\n"
              << "  \"model\": \"row-partitioned shared-memory CPU threads\"\n"
              << "}\n";
  } else {
    std::cout << std::fixed << std::setprecision(3)
              << "Parallel Pixels benchmark\n"
              << "Image: " << options.width << 'x' << options.height << "\n"
              << "Threads: " << options.workers << "\n"
              << "Iterations: " << options.iterations << "\n"
              << "Scalar median: " << scalar_median << " ms\n"
              << "Parallel median: " << parallel_median << " ms\n"
              << "Speedup: " << speedup << "x\n"
              << "Exact match: yes\n";
  }
  return EXIT_SUCCESS;
}

int run_blur(int argc, char** argv) {
  if (argc < 4) throw std::invalid_argument("blur requires an input and output PGM path");
  std::size_t workers = parallel_pixels::default_worker_count();
  for (int index = 4; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--threads") workers = parse_positive(option_value(index, argc, argv), argument);
    else throw std::invalid_argument("unknown blur option: " + std::string(argument));
  }
  if (workers > parallel_pixels::kMaximumWorkerCount) {
    throw std::invalid_argument("thread count is capped at 256");
  }
  const auto input = parallel_pixels::read_pgm(argv[2]);
  const auto output = parallel_pixels::gaussian_blur_parallel(input, workers);
  parallel_pixels::write_pgm(argv[3], output);
  std::cout << "Wrote " << output.width() << 'x' << output.height() << " image using " << workers
            << " requested threads\n";
  return EXIT_SUCCESS;
}

void usage(std::ostream& output) {
  output << "Usage:\n"
         << "  parallel-pixels blur <input.pgm> <output.pgm> [--threads N]\n"
         << "  parallel-pixels benchmark [--width N] [--height N] [--threads N] [--iterations N] [--json]\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc < 2) {
      usage(std::cerr);
      return EXIT_FAILURE;
    }
    const std::string_view command(argv[1]);
    if (command == "benchmark") return run_benchmark(argc, argv);
    if (command == "blur") return run_blur(argc, argv);
    if (command == "help" || command == "--help" || command == "-h") {
      usage(std::cout);
      return EXIT_SUCCESS;
    }
    throw std::invalid_argument("unknown command: " + std::string(command));
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
