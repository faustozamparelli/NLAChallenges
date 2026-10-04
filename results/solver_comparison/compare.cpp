#include "image_filters.hpp"
#include <unsupported/Eigen/IterativeSolvers>

#include <chrono>
#include <iostream>
#include <type_traits>

using namespace challenge;
using Clock = std::chrono::steady_clock;
using Gmres = Eigen::GMRES<Sparse, Eigen::IdentityPreconditioner>;
using Bicgstab = Eigen::BiCGSTAB<Sparse, Eigen::DiagonalPreconditioner<double>>;

constexpr int trials = 3;
constexpr double eigen_tolerance = 1e-10;
constexpr double lis_tolerance = 1e-12;

template <class Solver>
void benchmark(const Sparse& A, const Vector& b, const char* name) {
    for (int trial = 1; trial <= trials; ++trial) {
        Solver solver;
        solver.setTolerance(eigen_tolerance);
        solver.setMaxIterations(2000);
        if constexpr (std::is_same_v<Solver, Gmres>) solver.set_restart(60);

        const auto start = Clock::now();
        solver.compute(A);
        require(solver.info() == Eigen::Success, "Comparison solver setup failed");
        const Vector x = solver.solve(b);
        const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
        require(solver.info() == Eigen::Success && x.allFinite(), "Comparison solve failed");
        const double relative = (b - A * x).norm() / b.norm();
        require(std::isfinite(relative) && relative <= eigen_tolerance,
                "Comparison true residual exceeds tolerance");
        std::cout << std::setprecision(17) << name << " trial=" << trial
                  << " iterations=" << solver.iterations() << " status=" << solver.info()
                  << " seconds=" << seconds << " reported=" << solver.error()
                  << " true_relative=" << relative << '\n';
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 1 || argc == 2, "Usage: compare [LIS_SOLUTION_PATH]");
        const Vector w = load_lis_vector("w.mtx");
        require(w.norm() > 0.0, "Comparison requires a nonzero RHS");
        if (argc == 2) {
            Sparse A;
            require(Eigen::loadMarket(A, "A2.mtx"), "A2 read failed");
            const Vector x = load_lis_vector(argv[1]);
            require(A.rows() == w.size() && A.cols() == x.size(),
                    "Comparison system dimensions differ");
            const double relative = (w - A * x).norm() / w.norm();
            require(std::isfinite(relative) && relative <= lis_tolerance,
                    "LIS comparison true residual exceeds tolerance");
            std::cout << std::setprecision(17) << "true_relative=" << relative << '\n';
            return 0;
        }

        const Matrix image = load_image("deer.jpg");
        require(image.size() == w.size(), "Comparison image/RHS dimensions differ");
        const Sparse M = shifted_edge(assemble(static_cast<int>(image.rows()),
                                              static_cast<int>(image.cols()), edge_kernel()));
        benchmark<Gmres>(M, w, "GMRES60_identity");
        benchmark<Bicgstab>(M, w, "BiCGSTAB_diagonal");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "COMPARISON FAILURE: " << e.what() << '\n';
        return 1;
    }
}
