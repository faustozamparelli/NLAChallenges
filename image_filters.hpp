#pragma once

#include <Eigen/Dense>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>
#include <unsupported/Eigen/SparseExtra>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Each executable has one translation unit, so the stb implementations above
// are instantiated exactly once in that executable.
namespace challenge {
using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;
using Sparse = Eigen::SparseMatrix<double, Eigen::RowMajor>;
using Kernel = Eigen::Matrix3d;
namespace fs = std::filesystem;
constexpr std::uint32_t noise_seed = 42;

inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

inline Kernel smoothing_kernel() {
    Kernel H;
    H << 1, 1, 1, 1, 4, 1, 1, 1, 1;
    return H / 12.0;
}
inline Kernel sharpening_kernel() {
    Kernel H;
    H << 0, -3, 0, -1, 9, -3, 0, -1, 0;
    return H;
}
inline Kernel edge_kernel() {
    Kernel H;
    H << -1, 0, 1, -2, 0, 2, -1, 0, 1;
    return H;
}

inline Matrix load_image(const fs::path& path) {
    int width = 0, height = 0, channels = 0;
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> data(
        stbi_load(path.c_str(), &width, &height, &channels, 1), stbi_image_free);
    require(data != nullptr, "Cannot load image: " + path.string());
    require(height > 0 && width > 0 &&
            static_cast<long long>(height) * width <= std::numeric_limits<int>::max(),
            "Invalid or unsupported image dimensions");
    Matrix F(height, width);
    for (int i = 0; i < height; ++i)
        for (int j = 0; j < width; ++j)
            F(i, j) = data.get()[i * width + j];
    return F;  // Pixel intensities stay in [0,255], not [0,1].
}

inline Vector flatten(const Matrix& F) {
    Vector v(F.size());
    for (Eigen::Index i = 0; i < F.rows(); ++i)
        for (Eigen::Index j = 0; j < F.cols(); ++j)
            v(i * F.cols() + j) = F(i, j);
    return v;
}
inline Matrix reshape(const Vector& v, int m, int n) {
    require(m > 0 && n > 0 && v.size() == static_cast<long long>(m) * n,
            "Vector/image dimension mismatch");
    Matrix F(m, n);
    for (int i = 0; i < m; ++i)
        for (int j = 0; j < n; ++j) F(i, j) = v(i * n + j);
    return F;
}

inline Matrix add_noise(const Matrix& F) {
    std::mt19937 generator(noise_seed);
    std::uniform_int_distribution<int> distribution(-50, 50);
    Matrix W = F;
    for (Eigen::Index i = 0; i < F.rows(); ++i) {
        for (Eigen::Index j = 0; j < F.cols(); ++j) {
            W(i, j) = std::clamp(F(i, j) + distribution(generator), 0.0, 255.0);
        }
    }
    return W;
}

inline unsigned char display_byte(double value) {
    require(std::isfinite(value), "Nonfinite image value");
    return static_cast<unsigned char>(std::lround(std::clamp(value, 0.0, 255.0)));
}
inline void save_image(const Matrix& F, const fs::path& path) {
    using Bytes = Eigen::Matrix<unsigned char, Eigen::Dynamic,
                                Eigen::Dynamic, Eigen::RowMajor>;
    Bytes pixels(F.rows(), F.cols());
    for (Eigen::Index i = 0; i < F.rows(); ++i)
        for (Eigen::Index j = 0; j < F.cols(); ++j)
            pixels(i, j) = display_byte(F(i, j));
    require(stbi_write_png(path.c_str(), static_cast<int>(F.cols()),
                          static_cast<int>(F.rows()), 1, pixels.data(),
                          static_cast<int>(F.cols())) != 0,
            "Cannot write PNG: " + path.string());
}

inline Sparse assemble(int m, int n, const Kernel& H) {
    require(m > 0 && n > 0 &&
            static_cast<long long>(m) * n <= std::numeric_limits<int>::max(),
            "Invalid operator dimensions");
    const int N = m * n;
    std::vector<Eigen::Triplet<double>> entries;
    entries.reserve(static_cast<std::size_t>(N) * 9);
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            for (int a = 0; a < 3; ++a) {
                for (int b = 0; b < 3; ++b) {
                    const int r = i + a - 1, s = j + b - 1;
                    if (r >= 0 && r < m && s >= 0 && s < n && H(a, b) != 0.0)
                        entries.emplace_back(i * n + j, r * n + s, H(a, b));
                }
            }
        }
    }
    Sparse A(N, N);
    A.setFromTriplets(entries.begin(), entries.end());
    A.makeCompressed();
    return A;
}

inline void save_lis_vector(const Vector& v, const fs::path& path) {
    require(v.allFinite(), "Nonfinite vector");
    std::ofstream out(path);
    require(out.good(), "Cannot write vector: " + path.string());
    out << "%%MatrixMarket vector coordinate real general\n" << v.size() << '\n';
    out << std::setprecision(17) << std::scientific;
    for (Eigen::Index i = 0; i < v.size(); ++i) out << i + 1 << ' ' << v(i) << '\n';
    out.flush();
    require(out.good(), "Vector write failed: " + path.string());
}
inline Vector load_lis_vector(const fs::path& path) {
    std::ifstream in(path);
    require(in.good(), "Cannot read vector: " + path.string());
    std::string line;
    require(static_cast<bool>(std::getline(in, line)), "Missing vector header");
    {   // LIS writes extra spaces; accept whitespace, but not another format.
        std::istringstream header(line);
        std::string a, b, c, d, e, extra;
        require((header >> a >> b >> c >> d >> e) && !(header >> extra) &&
                a == "%%MatrixMarket" && b == "vector" && c == "coordinate" &&
                d == "real" && e == "general", "Unsupported LIS vector header");
    }
    auto next_data_line = [&]() {
        while (std::getline(in, line)) {
            const auto p = line.find_first_not_of(" \t\r");
            if (p != std::string::npos && line[p] != '%') return true;
        }
        return false;
    };
    require(next_data_line(), "Missing vector length");
    long long N = 0;
    std::string extra;
    std::istringstream dimensions(line);
    require((dimensions >> N) && !(dimensions >> extra) && N > 0 &&
            N <= std::numeric_limits<int>::max(), "Invalid vector length");
    Vector v = Vector::Zero(N);
    std::vector<bool> seen(static_cast<std::size_t>(N), false);
    long long count = 0;
    while (next_data_line()) {
        long long k = 0;
        double value = 0;
        std::istringstream record(line);
        require((record >> k >> value) && !(record >> extra) &&
                k >= 1 && k <= N && std::isfinite(value), "Invalid vector record");
        require(!seen[k - 1], "Duplicate vector index");
        seen[k - 1] = true;
        v(k - 1) = value;
        ++count;
    }
    require(count == N, "Missing vector entries");
    return v;
}

inline Sparse shifted_edge(const Sparse& A3) {
    // Bulk sparse addition: repeated coeffRef insertions into a compressed
    // matrix with an absent diagonal would repeatedly move/reallocate storage.
    Sparse identity(A3.rows(), A3.cols());
    identity.setIdentity();
    Sparse M = A3 + 4.0 * identity;
    M.makeCompressed();
    return M;
}
struct SolveResult {
    Vector solution;
    Eigen::Index iterations;
    double estimated_relative_residual;
    double relative_residual;
    double absolute_residual;
};
inline SolveResult solve_eigen(const Sparse& A, const Vector& b) {
    require(A.rows() == A.cols() && A.rows() == b.size() && b.allFinite(),
            "Invalid Eigen system dimensions or right-hand side");
    if (b.norm() == 0.0) return {Vector::Zero(b.size()), 0, 0.0, 0.0, 0.0};
    // The system is nonsymmetric. Its diagonal preconditioner is D=4I.
    Eigen::BiCGSTAB<Sparse, Eigen::DiagonalPreconditioner<double>> solver;
    solver.setTolerance(1e-10);
    solver.setMaxIterations(2000);
    solver.compute(A);
    require(solver.info() == Eigen::Success, "Eigen solver setup failed");
    Vector x = solver.solve(b);
    require(solver.info() == Eigen::Success && x.allFinite(), "Eigen solve failed");
    const double absolute = (b - A * x).norm();
    const double relative = b.norm() == 0.0 ? absolute : absolute / b.norm();
    require(relative <= 1e-10, "Eigen true residual exceeds tolerance");
    return {x, solver.iterations(), solver.error(), relative, absolute};
}
}  // namespace challenge
