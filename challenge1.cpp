#include "image_filters.hpp"
#include <iostream>

using namespace challenge;

int main(int argc, char** argv) {
    try {
        require(argc == 4, "Usage: challenge1 TASK_NUMBER IMAGE_PATH OUTPUT_DIRECTORY\n"
                           "Task 8 is run by the LIS driver, not this executable.");
        const std::string task_text = argv[1];
        std::size_t parsed = 0;
        const int task = std::stoi(task_text, &parsed);
        require(parsed == task_text.size() && task >= 1 && task <= 13 && task != 8,
                "Invalid C++ task number");
        const fs::path root = argv[3];
        for (const auto& name : {"images", "data", "metrics"})
            fs::create_directories(root / name);
        const auto data = root / "data", images = root / "images";
        std::ofstream metrics(root / "metrics" / ("task" + std::to_string(task) + ".txt"));
        require(metrics.good(), "Cannot open metrics file");
        metrics << std::setprecision(17);
        auto record = [&](const std::string& key, const auto& value) {
            metrics << key << '=' << value << '\n';
            std::cout << key << '=' << std::setprecision(17) << value << '\n';
        };
        auto save_matrix = [&](const Sparse& A, const std::string& name) {
            require(Eigen::saveMarket(A, (data / name).string()), "Matrix export failed");
        };
        auto load_matrix = [&](const std::string& name) {
            Sparse A;
            require(Eigen::loadMarket(A, (data / name).string()), "Matrix import failed");
            return A;
        };
        const Matrix F = load_image(argv[2]);
        const int m = static_cast<int>(F.rows()), n = static_cast<int>(F.cols()), N = m * n;
        auto read_vector = [&](const std::string& name) {
            Vector v = load_lis_vector(data / name);
            require(v.size() == N, "Loaded vector has wrong size");
            return v;
        };
        auto check_size = [&](const Sparse& A) {
            require(A.rows() == N && A.cols() == N, "Loaded matrix has wrong size");
        };
        std::cout << "Task " << task << '\n';
        switch (task) {
        case 1:
            record("height_m", m);
            record("width_n", n);
            record("pixels_N", N);
            record("original_min", F.minCoeff());
            record("original_max", F.maxCoeff());
            break;
        case 2: {
            const Matrix W = add_noise(F);
            const Matrix noise = W - F;
            require(noise.minCoeff() >= -50 && noise.maxCoeff() <= 50, "Noise out of range");
            save_lis_vector(flatten(W), data / "w.mtx");
            save_image(W, images / "task02_noisy.png");
            record("seed", noise_seed);
            record("noise_min", noise.minCoeff());
            record("noise_max", noise.maxCoeff());
            record("noisy_min", W.minCoeff());
            record("noisy_max", W.maxCoeff());
            record("clip_numerical_noise", "false");
            break;
        }
        case 3: {
            const Vector v = flatten(F), w = read_vector("w.mtx");
            require(v.size() == N && w.size() == N, "Reshaping failed");
            require((reshape(v, m, n) - F).norm() == 0.0, "Original reshaping failed");
            save_lis_vector(v, data / "v.mtx");
            record("v_components", v.size());
            record("w_components", w.size());
            record("v_euclidean_norm", v.norm());
            record("w_euclidean_norm", w.norm());
            break;
        }
        case 4: {
            const Sparse A1 = assemble(m, n, smoothing_kernel());
            const long long expected = (3LL * m - 2) * (3LL * n - 2);
            require(A1.nonZeros() == expected, "Wrong A1 nonzero count");
            record("operator_rows", N);
            record("operator_columns", N);
            record("A1_nonzeros", A1.nonZeros());
            record("expected_nonzeros", expected);
            break;
        }
        case 5: {
            const Sparse A1 = assemble(m, n, smoothing_kernel());
            const Vector w = read_vector("w.mtx"), smoothed = A1 * w;
            save_image(reshape(smoothed, m, n), images / "task05_smoothed.png");
            record("smoothed_min", smoothed.minCoeff());
            record("smoothed_max", smoothed.maxCoeff());
            break;
        }
        case 6: {
            const Sparse A2 = assemble(m, n, sharpening_kernel());
            const long long expected = 5LL * m * n - 2LL * m - 2LL * n;
            require(A2.nonZeros() == expected, "Wrong A2 nonzero count");
            const Sparse difference = A2 - Sparse(A2.transpose());
            save_matrix(A2, "A2.mtx");
            record("A2_nonzeros", A2.nonZeros());
            record("expected_nonzeros", expected);
            record("A2_symmetric", difference.norm() == 0 ? "true" : "false");
            record("A2_transpose_difference_frobenius", difference.norm());
            break;
        }
        case 7: {
            const Sparse A2 = load_matrix("A2.mtx");
            check_size(A2);
            const Vector v = read_vector("v.mtx"), sharpened = A2 * v;
            save_image(reshape(sharpened, m, n), images / "task07_sharpened.png");
            record("sharpened_min", sharpened.minCoeff());
            record("sharpened_max", sharpened.maxCoeff());
            break;
        }
        case 9: {
            const Sparse A2 = load_matrix("A2.mtx");
            check_size(A2);
            const Vector x = read_vector("x_lis.mtx"), w = read_vector("w.mtx");
            const double absolute = (w - A2 * x).norm();
            const double relative = absolute / w.norm();
            require(relative <= 1e-12, "LIS true residual exceeds tolerance");
            save_image(reshape(x, m, n), images / "task09_lis_solution.png");
            record("absolute_residual", absolute);
            record("relative_residual", relative);
            record("solution_min", x.minCoeff());
            record("solution_max", x.maxCoeff());
            break;
        }
        case 10: {
            const Sparse A3 = assemble(m, n, edge_kernel());
            require(A3.nonZeros() == 2LL * (n - 1) * (3LL * m - 2), "Wrong A3 nonzero count");
            const Sparse difference = A3 - Sparse(A3.transpose());
            const Sparse skew_check = A3 + Sparse(A3.transpose());
            require(skew_check.norm() == 0.0, "A3 is not skew-symmetric");
            record("A3_nonzeros", A3.nonZeros());
            record("A3_symmetric", difference.norm() == 0 ? "true" : "false");
            record("A3_transpose_difference_frobenius", difference.norm());
            record("A3_transpose_sum_frobenius", skew_check.norm());
            break;
        }
        case 11: {
            const Sparse A3 = assemble(m, n, edge_kernel());
            const Vector v = read_vector("v.mtx"), edges = A3 * v;
            save_image(reshape(edges, m, n), images / "task11_edges.png");
            record("edges_min", edges.minCoeff());
            record("edges_max", edges.maxCoeff());
            break;
        }
        case 12: {
            const Sparse A3 = assemble(m, n, edge_kernel());
            const Sparse M = shifted_edge(A3);
            const Vector w = read_vector("w.mtx");
            const auto result = solve_eigen(M, w);
            require(Eigen::saveMarketVector(result.solution, (data / "y_eigen.mtx").string()),
                    "Eigen vector export failed");
            record("solver", "BiCGSTAB");
            record("preconditioner", "DiagonalPreconditioner");
            record("tolerance", 1e-10);
            record("max_iterations", 2000);
            record("iterations", result.iterations);
            record("solver_relative_residual", result.estimated_relative_residual);
            record("relative_residual", result.relative_residual);
            record("absolute_residual", result.absolute_residual);
            break;
        }
        case 13: {
            Vector y;
            require(Eigen::loadMarketVector(y, (data / "y_eigen.mtx").string()),
                    "Eigen vector import failed");
            require(y.size() == N && y.allFinite(), "Invalid imported y");
            const Sparse A3 = assemble(m, n, edge_kernel());
            const Vector w = read_vector("w.mtx");
            const double relative = (w - shifted_edge(A3) * y).norm() / w.norm();
            require(relative <= 1e-10, "Reimported y fails tolerance");
            save_image(reshape(y, m, n), images / "task13_eigen_solution.png");
            record("imported_components", y.size());
            record("imported_relative_residual", relative);
            record("solution_min", y.minCoeff());
            record("solution_max", y.maxCoeff());
            break;
        }
        }
        metrics.flush();
        require(metrics.good(), "Metrics write failed");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
