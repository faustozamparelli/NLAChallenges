#include "image_filters.hpp"
#include <iostream>

using namespace challenge;

int main(int argc, char** argv) {
    try {
        const fs::path scratch = argc > 1 ? argv[1] : "build/test_scratch";
        fs::create_directories(scratch);
        Matrix tiny(2, 2);
        tiny << 12, 24, 36, 48;
        const Vector v = flatten(tiny);
        require(v(0) == 12 && v(1) == 24 && v(2) == 36 && v(3) == 48,
                "Flattening order");
        require((reshape(v, 2, 2) - tiny).norm() == 0, "Reshape inverse");
        const Sparse small_A1 = assemble(2, 2, smoothing_kernel());
        require(std::abs((small_A1 * v)(0) - 13.0) < 1e-14, "Top-left zero padding");
        const Sparse small_A2 = assemble(2, 2, sharpening_kernel());
        require(small_A2.coeff(0, 1) == -3 && small_A2.coeff(1, 0) == -1,
                "Sharpening orientation");
        require(small_A2.coeff(0, 2) == -1 && small_A2.coeff(2, 0) == -3,
                "Vertical sharpening orientation");
        const Sparse small_A3 = assemble(2, 2, edge_kernel());
        require(small_A3.coeff(0, 1) == 2 && small_A3.coeff(1, 0) == -2,
                "Sobel orientation");
        std::cout << "PASS: row-wise reshape, hand-computed border and kernel orientation\n";

        for (int m = 1; m <= 5; ++m) {
            for (int n = 1; n <= 6; ++n) {
                Matrix F(m, n);
                for (int i = 0; i < m; ++i)
                    for (int j = 0; j < n; ++j) F(i, j) = 0.25 + 11 * i - 3 * j;
                const std::array<Kernel, 3> kernels{
                    smoothing_kernel(), sharpening_kernel(), edge_kernel()};
                const std::array<long long, 3> counts{
                    (3LL * m - 2) * (3LL * n - 2),
                    5LL * m * n - 2LL * m - 2LL * n,
                    2LL * (n - 1) * (3LL * m - 2)};
                for (int k = 0; k < 3; ++k) {
                    const Sparse A = assemble(m, n, kernels[k]);
                    require(A.nonZeros() == counts[k], "Analytical nonzero count");
                    stencil_difference(A, flatten(F), m, n, kernels[k]);
                    for (int outer = 0; outer < A.outerSize(); ++outer)
                        for (Sparse::InnerIterator it(A, outer); it; ++it)
                            require(it.value() != 0.0, "Stored zero");
                    if (k == 0)
                        require(Sparse(A - Sparse(A.transpose())).norm() == 0,
                                "Smoothing symmetry");
                    if (k == 2)
                        require(Sparse(A + Sparse(A.transpose())).norm() == 0,
                                "Sobel skew-symmetry");
                }
                if (n > 1 && m > 1) {
                    const Sparse A = assemble(m, n, sharpening_kernel());
                    require(A.coeff(n - 1, n) == 0, "No row wrapping");
                }
            }
        }
        std::cout << "PASS: all 30 shapes, nonzero counts, three stencil products, no wrapping\n";

        const Matrix noisy = add_noise(tiny);
        require((noisy - add_noise(tiny)).norm() == 0, "Deterministic noise");
        require((noisy - tiny).minCoeff() >= -50 && (noisy - tiny).maxCoeff() <= 50,
                "Noise range");
        require(display_byte(-10) == 0 && display_byte(300) == 255 &&
                display_byte(12.4) == 12 && display_byte(12.5) == 13, "Display clipping/rounding");
        save_image(tiny, scratch / "tiny.png");
        require((load_image(scratch / "tiny.png") - tiny).norm() == 0, "PNG round trip/layout");
        const Matrix before_display = noisy;
        save_image(noisy, scratch / "noisy.png");
        require((noisy - before_display).norm() == 0, "Display altered numerical noise");
        const Matrix displayed_noise = load_image(scratch / "noisy.png");
        for (int i = 0; i < noisy.rows(); ++i)
            for (int j = 0; j < noisy.cols(); ++j)
                require(displayed_noise(i, j) == display_byte(noisy(i, j)),
                        "Noisy PNG clipping/layout");
        std::cout << "PASS: fixed-seed noise, range, PNG clipping/rounding and layout\n";

        Vector unequal(3);
        unequal << 2.5, -1.123456789012345, 4.234567890123456;
        save_lis_vector(unequal, scratch / "vector.mtx");
        require((unequal - load_lis_vector(scratch / "vector.mtx")).norm() == 0,
                "Precision-preserving vector round trip");
        {
            std::ofstream out(scratch / "shuffled.mtx");
            out << "%%MatrixMarket   vector coordinate real general\n% comment\n3\n"
                   "3 4\n\n% more comments\n1 2.5\n2 -1\n";
        }
        const Vector shuffled = load_lis_vector(scratch / "shuffled.mtx");
        require(shuffled(0) == 2.5 && shuffled(1) == -1 && shuffled(2) == 4,
                "Indexed records/comments");
        const std::array<std::string, 9> bad_records{
            "3\n1 2\n1 3\n3 4\n", "3\n0 2\n2 3\n3 4\n", "3\n1 2\n2 3\n",
            "3\n1 2\n2 3\n4 4\n", "3\n1 2 extra\n2 3\n3 4\n",
            "0\n", "-3\n", "3 extra\n1 2\n2 3\n3 4\n", "3\n1 nan\n2 3\n3 4\n"};
        for (const auto& records : bad_records) {
            {
                std::ofstream out(scratch / "bad.mtx");
                out << "%%MatrixMarket vector coordinate real general\n" << records;
            }
            bool rejected = false;
            try { load_lis_vector(scratch / "bad.mtx"); }
            catch (const std::runtime_error&) { rejected = true; }
            require(rejected, "Malformed vector accepted");
        }
        Sparse reloaded;
        require(Eigen::saveMarket(small_A2, (scratch / "matrix.mtx").string()), "Save sparse");
        require(Eigen::loadMarket(reloaded, (scratch / "matrix.mtx").string()), "Load sparse");
        require(Sparse(reloaded - small_A2).norm() == 0, "Sparse matrix round trip");
        require(Eigen::saveMarketVector(unequal, (scratch / "array.mtx").string()), "Save array");
        Vector array;
        require(Eigen::loadMarketVector(array, (scratch / "array.mtx").string()), "Load array");
        require((array - unequal).norm() == 0, "Eigen array round trip");
        std::cout << "PASS: matrix/vector round trips, shuffled LIS records, malformed inputs rejected\n";

        const Sparse A3 = assemble(5, 6, edge_kernel());
        const Sparse M = shifted_edge(A3);
        Sparse identity(30, 30);
        identity.setIdentity();
        require(Sparse(M - A3 - 4.0 * identity).norm() == 0.0 &&
                M.nonZeros() == A3.nonZeros() + 30, "Bulk diagonal assembly");
        Vector exact(30);
        for (int p = 0; p < 30; ++p) exact(p) = std::sin(0.3 * p) + 1;
        const auto solved = solve_eigen(M, M * exact);
        require((solved.solution - exact).norm() / exact.norm() < 1e-9,
                "Manufactured Eigen solution error");
        require(std::isfinite(solved.estimated_relative_residual) &&
                solved.estimated_relative_residual <= 1e-10 &&
                solved.relative_residual <= 1e-10, "BiCGSTAB residual checks");
        const auto zero = solve_eigen(M, Vector::Zero(30));
        require(zero.solution.norm() == 0 && zero.relative_residual == 0 &&
                zero.iterations == 0, "Zero RHS");
        std::cout << "PASS: manufactured Eigen solution and zero RHS\nALL TESTS PASSED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "TEST FAILURE: " << e.what() << '\n';
        return 1;
    }
}
