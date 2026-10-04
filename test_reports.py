"""Regression tests for comparison validation and data-driven report generation."""
import hashlib
from pathlib import Path
import re
import tempfile
import unittest

from comparison_report import checked_measurement, load_measurements, render_comparison
from make_report import parse_lis_diagnostics

class ComparisonReportTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory()
        self.addCleanup(self.scratch.cleanup)
        self.project = Path(self.scratch.name)
        self.root = self.project / "results"
        self.comparison = self.root / "solver_comparison"
        self.comparison.mkdir(parents=True)
        for method, label, iterations in (("gmres", "GMRES", 33), ("bicgstab", "BiCGSTAB", 17)):
            for trial in (1, 2, 3):
                (self.comparison / f"lis_{method}_{trial}.log").write_text(
                    "linear solver status  : normal end\nmax number of threads = 10\n"
                    "number of threads = 1\n"
                    f"linear solver : {label}\npreconditioner : ILU(0)\n"
                    f"{label}: number of iterations = {iterations}\n"
                    f"{label}: elapsed time = 0.5\n"
                    f"{label}: relative residual = 4e-13\ntrue_relative=4e-13\n")
        eigen_lines = [
            f"{label} trial={trial} iterations={iterations} status=0 seconds=0.5 "
            "reported=9e-11 true_relative=9e-11"
            for label, iterations in (("GMRES60_identity", 47), ("BiCGSTAB_diagonal", 52))
            for trial in (1, 2, 3)]
        (self.comparison / "eigen_comparison.log").write_text("\n".join(eigen_lines) + "\n")
        paths = ["deer.jpg", "results/data/A2.mtx", "results/data/w.mtx", "image_filters.hpp",
                 "results/solver_comparison/compare.cpp", "results/solver_comparison/run_comparison.sh",
                 "third_party/lis_test1.c",
                 "third_party/stb_image.h", "third_party/stb_image_write.h"]
        fingerprints = []
        for name in paths:
            path = self.project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
            fingerprints.append(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {name}")
        (self.comparison / "input_hashes.txt").write_text("\n".join(fingerprints) + "\n")

    def replace(self, name, before, after):
        path = self.comparison / name
        text = path.read_text()
        self.assertIn(before, text)
        path.write_text(text.replace(before, after, 1))

    def test_valid_logs_and_dynamic_timings(self):
        result = load_measurements(self.root)
        self.assertEqual(result["gmres"][0].iterations, 33)
        self.assertEqual(result["bicgstab"][0].iterations, 17)
        text = render_comparison(self.root)
        self.assertIn("| 8 | LIS GMRES(40)", text)
        # Changing all GMRES times must change the table, not leave fixed benchmark numbers.
        for trial in (1, 2, 3):
            path = self.comparison / f"lis_gmres_{trial}.log"
            path.write_text(re.sub(r"(elapsed time\s*=\s*)\S+", r"\g<1>1.000000e+00", path.read_text()))
        self.assertIn("| 1.000 s |", render_comparison(self.root))

    def test_submission_lis_diagnostics(self):
        log = (self.comparison / "lis_bicgstab_1.log").read_text()
        self.assertEqual(parse_lis_diagnostics(log), (17, 4e-13))
        text = render_comparison(self.root)
        self.assertIn("LIS BiCGSTAB + ILU(0), submission choice", text)
        self.assertIn("Eigen BiCGSTAB + diagonal, submission choice", text)
        self.assertNotIn("not switched", text)

    def test_wrong_or_invalid_submission_diagnostics_are_rejected(self):
        log = (self.comparison / "lis_bicgstab_1.log").read_text()
        for before, after in (("BiCGSTAB", "GMRES"), ("ILU(0)", "Jacobi"),
                              ("normal end", "failure"), ("relative residual = 4e-13", "relative residual = nan"),
                              ("relative residual = 4e-13", "relative residual = 1e-3"),
                              ("iterations = 17", "iterations = 0")):
            with self.subTest(before=before, after=after):
                with self.assertRaises(ValueError):
                    parse_lis_diagnostics(log.replace(before, after))
        with self.assertRaisesRegex(ValueError, "Missing/duplicate"):
            parse_lis_diagnostics(log + "BiCGSTAB: number of iterations = 17\n")

    def test_stale_inputs_are_rejected(self):
        (self.root / "data" / "w.mtx").write_text("different RHS")
        with self.assertRaisesRegex(ValueError, "Stale comparison"):
            render_comparison(self.root)

    def test_incomplete_hashes_are_rejected(self):
        (self.comparison / "input_hashes.txt").write_text("")
        with self.assertRaisesRegex(ValueError, "Incomplete comparison"):
            load_measurements(self.root)

    def test_failed_eigen_run_is_rejected(self):
        self.replace("eigen_comparison.log", "status=0", "status=2")
        with self.assertRaisesRegex(ValueError, "Failed/duplicate"):
            load_measurements(self.root)

    def test_missing_eigen_trial_is_rejected(self):
        path = self.comparison / "eigen_comparison.log"
        path.write_text("\n".join(path.read_text().splitlines()[:-1]) + "\n")
        with self.assertRaisesRegex(ValueError, "Incomplete Eigen"):
            load_measurements(self.root)

    def test_failed_lis_run_is_rejected(self):
        self.replace("lis_gmres_1.log", "normal end", "failure")
        with self.assertRaisesRegex(ValueError, "successful convergence"):
            load_measurements(self.root)

    def test_duplicate_diagnostics_are_rejected(self):
        path = self.comparison / "lis_bicgstab_1.log"
        path.write_text(path.read_text() + "true_relative=1e-13\n")
        with self.assertRaisesRegex(ValueError, "Missing/duplicate"):
            load_measurements(self.root)

    def test_invalid_numbers_and_excessive_residuals_are_rejected(self):
        for seconds, reported, relative in (("nan", 0, 0), (-1, 0, 0), (0, 0, 0),
                                            (1, "inf", 0), (1, 0, -1), (1, 0, 1e-3)):
            with self.subTest(seconds=seconds, reported=reported, relative=relative):
                with self.assertRaises(ValueError):
                    checked_measurement(1, seconds, reported, relative, 1e-10)

    def test_missing_comparison_has_no_speed_ranking(self):
        (self.comparison / "eigen_comparison.log").unlink()
        self.assertIn("no speed ranking", render_comparison(self.root))


if __name__ == "__main__":
    unittest.main()
