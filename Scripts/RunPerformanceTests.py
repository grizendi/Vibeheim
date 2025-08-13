#!/usr/bin/env python3
"""
Performance regression test runner for CI/CD pipeline
Runs automated performance tests and validates results against targets
"""

import json
import sys
import os
import subprocess
import argparse
from datetime import datetime
from pathlib import Path

class PerformanceTestRunner:
    def __init__(self, project_path, engine_path):
        self.project_path = Path(project_path)
        self.engine_path = Path(engine_path)
        self.results_dir = self.project_path / "Saved" / "PerformanceTests"
        self.results_dir.mkdir(parents=True, exist_ok=True)
        
        # Performance targets (must match C++ constants)
        self.targets = {
            "averageGenerationTimeMs": 5.0,
            "p95GenerationTimeMs": 9.0,
            "lod0MemoryLimitMB": 64,
            "maxTrianglesPerChunk": 8000
        }
    
    def run_unreal_automation_test(self, test_name, additional_args=""):
        """Run a specific Unreal automation test"""
        editor_cmd = self.engine_path / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
        project_file = self.project_path / "Vibeheim.uproject"
        
        cmd = [
            str(editor_cmd),
            str(project_file),
            "-ExecCmds=Automation RunTests " + test_name,
            "-TestExit=Automation Test Queue Empty",
            "-ReportOutputDir=" + str(self.results_dir),
            "-NullRHI",
            "-Unattended",
            "-NoSplash",
            "-NoSound"
        ]
        
        if additional_args:
            cmd.extend(additional_args.split())
        
        print(f"Running test: {test_name}")
        print(f"Command: {' '.join(cmd)}")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)  # 10 minute timeout
            return result.returncode == 0, result.stdout, result.stderr
        except subprocess.TimeoutExpired:
            print(f"Test {test_name} timed out after 10 minutes")
            return False, "", "Test timed out"
    
    def run_performance_regression_tests(self):
        """Run all performance regression tests"""
        tests = [
            "WorldGen.Performance.ChunkGenerationRegression",
            "WorldGen.Performance.LOD0MemoryValidation", 
            "WorldGen.Performance.TriangleCountValidation",
            "WorldGen.Performance.StreamingPerformance",
            "WorldGen.Performance.ComprehensiveRegression"
        ]
        
        results = {}
        all_passed = True
        
        for test in tests:
            success, stdout, stderr = self.run_unreal_automation_test(test)
            results[test] = {
                "passed": success,
                "stdout": stdout,
                "stderr": stderr
            }
            
            if not success:
                all_passed = False
                print(f"FAILED: {test}")
                if stderr:
                    print(f"Error: {stderr}")
            else:
                print(f"PASSED: {test}")
        
        return all_passed, results
    
    def parse_performance_data(self, json_file_path):
        """Parse exported performance data JSON"""
        try:
            with open(json_file_path, 'r') as f:
                data = json.load(f)
            return data
        except (FileNotFoundError, json.JSONDecodeError) as e:
            print(f"Failed to parse performance data: {e}")
            return None
    
    def validate_performance_targets(self, performance_data):
        """Validate performance data against targets"""
        if not performance_data:
            return False, ["No performance data available"]
        
        failures = []
        
        # Calculate metrics from chunk data
        chunk_metrics = performance_data.get("chunkMetrics", [])
        if not chunk_metrics:
            return False, ["No chunk metrics available"]
        
        # Calculate generation time statistics
        generation_times = [m["generationTimeMs"] for m in chunk_metrics]
        if generation_times:
            avg_time = sum(generation_times) / len(generation_times)
            generation_times.sort()
            p95_index = int(len(generation_times) * 0.95)
            p95_time = generation_times[p95_index] if p95_index < len(generation_times) else generation_times[-1]
            
            if avg_time > self.targets["averageGenerationTimeMs"]:
                failures.append(f"Average generation time {avg_time:.2f}ms exceeds target {self.targets['averageGenerationTimeMs']}ms")
            
            if p95_time > self.targets["p95GenerationTimeMs"]:
                failures.append(f"P95 generation time {p95_time:.2f}ms exceeds target {self.targets['p95GenerationTimeMs']}ms")
        
        # Calculate memory usage for LOD0 chunks
        lod0_memory = sum(m["memoryUsageBytes"] for m in chunk_metrics if m["lodLevel"] == 0)
        lod0_memory_mb = lod0_memory / (1024 * 1024)
        
        if lod0_memory_mb > self.targets["lod0MemoryLimitMB"]:
            failures.append(f"LOD0 memory usage {lod0_memory_mb:.1f}MB exceeds target {self.targets['lod0MemoryLimitMB']}MB")
        
        # Check triangle counts
        max_triangles = max(m["triangleCount"] for m in chunk_metrics)
        if max_triangles > self.targets["maxTrianglesPerChunk"]:
            failures.append(f"Max triangle count {max_triangles} exceeds target {self.targets['maxTrianglesPerChunk']}")
        
        return len(failures) == 0, failures
    
    def generate_ci_report(self, test_results, performance_data, validation_results):
        """Generate CI/CD report"""
        report = {
            "timestamp": datetime.now().isoformat(),
            "testResults": test_results,
            "performanceTargets": self.targets,
            "validationPassed": validation_results[0],
            "validationFailures": validation_results[1],
            "summary": {
                "totalTests": len(test_results),
                "passedTests": sum(1 for r in test_results.values() if r["passed"]),
                "failedTests": sum(1 for r in test_results.values() if not r["passed"])
            }
        }
        
        if performance_data:
            chunk_metrics = performance_data.get("chunkMetrics", [])
            if chunk_metrics:
                generation_times = [m["generationTimeMs"] for m in chunk_metrics]
                memory_usage = [m["memoryUsageBytes"] for m in chunk_metrics]
                triangle_counts = [m["triangleCount"] for m in chunk_metrics]
                
                report["performanceMetrics"] = {
                    "averageGenerationTimeMs": sum(generation_times) / len(generation_times) if generation_times else 0,
                    "p95GenerationTimeMs": sorted(generation_times)[int(len(generation_times) * 0.95)] if generation_times else 0,
                    "totalMemoryUsageMB": sum(memory_usage) / (1024 * 1024) if memory_usage else 0,
                    "lod0MemoryUsageMB": sum(m["memoryUsageBytes"] for m in chunk_metrics if m["lodLevel"] == 0) / (1024 * 1024),
                    "averageTriangleCount": sum(triangle_counts) / len(triangle_counts) if triangle_counts else 0,
                    "maxTriangleCount": max(triangle_counts) if triangle_counts else 0
                }
        
        # Write report
        report_file = self.results_dir / "ci_performance_report.json"
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"CI report written to: {report_file}")
        return report
    
    def run_full_test_suite(self):
        """Run complete performance test suite for CI/CD"""
        print("Starting performance regression test suite...")
        
        # Run automation tests
        test_passed, test_results = self.run_performance_regression_tests()
        
        # Look for exported performance data
        performance_data_file = self.results_dir / "ComprehensiveRegressionResults.json"
        performance_data = self.parse_performance_data(performance_data_file)
        
        # Validate against targets
        validation_passed, validation_failures = self.validate_performance_targets(performance_data)
        
        # Generate CI report
        report = self.generate_ci_report(test_results, performance_data, (validation_passed, validation_failures))
        
        # Print summary
        print("\n=== PERFORMANCE TEST SUITE RESULTS ===")
        print(f"Automation Tests: {'PASS' if test_passed else 'FAIL'}")
        print(f"Performance Validation: {'PASS' if validation_passed else 'FAIL'}")
        
        if validation_failures:
            print("Validation Failures:")
            for failure in validation_failures:
                print(f"  - {failure}")
        
        overall_passed = test_passed and validation_passed
        print(f"Overall Result: {'PASS' if overall_passed else 'FAIL'}")
        print("=====================================")
        
        return overall_passed

def main():
    parser = argparse.ArgumentParser(description="Run performance regression tests for CI/CD")
    parser.add_argument("--project-path", required=True, help="Path to Unreal project")
    parser.add_argument("--engine-path", required=True, help="Path to Unreal Engine installation")
    parser.add_argument("--output-dir", help="Output directory for results (optional)")
    
    args = parser.parse_args()
    
    runner = PerformanceTestRunner(args.project_path, args.engine_path)
    
    if args.output_dir:
        runner.results_dir = Path(args.output_dir)
        runner.results_dir.mkdir(parents=True, exist_ok=True)
    
    success = runner.run_full_test_suite()
    
    # Exit with appropriate code for CI/CD
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()