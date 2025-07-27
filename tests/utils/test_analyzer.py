#!/usr/bin/env python3
"""
Kryon Test Result Analyzer
Analyzes test results, generates reports, and identifies patterns
"""

import json
import os
import sys
from datetime import datetime
from typing import Dict, List, Any, Tuple
import re
from pathlib import Path
import argparse
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict


class TestResultAnalyzer:
    def __init__(self, results_dir: str = "results"):
        self.results_dir = Path(results_dir)
        self.test_data = []
        self.performance_metrics = defaultdict(list)
        self.error_patterns = defaultdict(int)
        
    def load_results(self, pattern: str = "*.log") -> None:
        """Load test results from log files"""
        log_files = list(self.results_dir.glob(pattern))
        print(f"Found {len(log_files)} log files to analyze")
        
        for log_file in log_files:
            self._parse_log_file(log_file)
            
    def _parse_log_file(self, log_file: Path) -> None:
        """Parse a single log file for test results"""
        with open(log_file, 'r') as f:
            content = f.read()
            
        # Extract test results
        test_result = {
            'file': log_file.name,
            'timestamp': self._extract_timestamp(content),
            'tests_passed': self._count_pattern(content, r'✓|passed|PASS'),
            'tests_failed': self._count_pattern(content, r'❌|failed|FAIL'),
            'warnings': self._count_pattern(content, r'⚠️|warning|WARN'),
            'duration': self._extract_duration(content),
            'performance': self._extract_performance_metrics(content),
            'errors': self._extract_errors(content)
        }
        
        self.test_data.append(test_result)
        
        # Aggregate performance metrics
        for metric, value in test_result['performance'].items():
            self.performance_metrics[metric].append(value)
            
        # Track error patterns
        for error in test_result['errors']:
            self.error_patterns[self._categorize_error(error)] += 1
            
    def _extract_timestamp(self, content: str) -> str:
        """Extract timestamp from log content"""
        match = re.search(r'(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})', content)
        return match.group(1) if match else "Unknown"
        
    def _count_pattern(self, content: str, pattern: str) -> int:
        """Count occurrences of a pattern"""
        return len(re.findall(pattern, content, re.IGNORECASE))
        
    def _extract_duration(self, content: str) -> float:
        """Extract test duration in seconds"""
        match = re.search(r'completed in (\d+(?:\.\d+)?)\s*(?:seconds|s)', content, re.IGNORECASE)
        if match:
            return float(match.group(1))
        
        # Try minutes format
        match = re.search(r'(\d+)m\s*(\d+)s', content)
        if match:
            return int(match.group(1)) * 60 + int(match.group(2))
            
        return 0.0
        
    def _extract_performance_metrics(self, content: str) -> Dict[str, float]:
        """Extract performance metrics from log"""
        metrics = {}
        
        # FPS
        fps_match = re.search(r'(\d+(?:\.\d+)?)\s*FPS', content)
        if fps_match:
            metrics['fps'] = float(fps_match.group(1))
            
        # Memory usage
        mem_match = re.search(r'(\d+(?:\.\d+)?)\s*MB', content)
        if mem_match:
            metrics['memory_mb'] = float(mem_match.group(1))
            
        # CPU usage
        cpu_match = re.search(r'(\d+(?:\.\d+)?)\s*%\s*CPU', content)
        if cpu_match:
            metrics['cpu_percent'] = float(cpu_match.group(1))
            
        # Element count
        elem_match = re.search(r'(\d+)\s*elements', content)
        if elem_match:
            metrics['element_count'] = int(elem_match.group(1))
            
        # Render time
        render_match = re.search(r'(\d+(?:\.\d+)?)\s*ms.*render', content)
        if render_match:
            metrics['render_time_ms'] = float(render_match.group(1))
            
        return metrics
        
    def _extract_errors(self, content: str) -> List[str]:
        """Extract error messages from log"""
        errors = []
        
        # Common error patterns
        error_patterns = [
            r'Error:.*',
            r'Failed:.*',
            r'❌.*',
            r'panic:.*',
            r'thread.*panicked.*'
        ]
        
        for pattern in error_patterns:
            matches = re.findall(pattern, content, re.IGNORECASE | re.MULTILINE)
            errors.extend(matches)
            
        return errors
        
    def _categorize_error(self, error: str) -> str:
        """Categorize error by type"""
        error_lower = error.lower()
        
        if 'compile' in error_lower:
            return 'Compilation Error'
        elif 'render' in error_lower:
            return 'Rendering Error'
        elif 'layout' in error_lower:
            return 'Layout Error'
        elif 'memory' in error_lower or 'alloc' in error_lower:
            return 'Memory Error'
        elif 'panic' in error_lower:
            return 'Panic Error'
        elif 'timeout' in error_lower:
            return 'Timeout Error'
        else:
            return 'Other Error'
            
    def generate_summary_report(self) -> Dict[str, Any]:
        """Generate summary report of all test results"""
        if not self.test_data:
            return {'error': 'No test data loaded'}
            
        total_tests = len(self.test_data)
        total_passed = sum(t['tests_passed'] for t in self.test_data)
        total_failed = sum(t['tests_failed'] for t in self.test_data)
        total_warnings = sum(t['warnings'] for t in self.test_data)
        
        avg_duration = sum(t['duration'] for t in self.test_data) / total_tests
        
        # Calculate performance averages
        performance_avg = {}
        for metric, values in self.performance_metrics.items():
            if values:
                performance_avg[metric] = {
                    'average': np.mean(values),
                    'min': min(values),
                    'max': max(values),
                    'std_dev': np.std(values)
                }
                
        report = {
            'summary': {
                'total_test_runs': total_tests,
                'total_tests_passed': total_passed,
                'total_tests_failed': total_failed,
                'total_warnings': total_warnings,
                'average_duration_seconds': avg_duration,
                'success_rate': (total_passed / (total_passed + total_failed) * 100) if (total_passed + total_failed) > 0 else 0
            },
            'performance': performance_avg,
            'error_distribution': dict(self.error_patterns),
            'test_history': self.test_data
        }
        
        return report
        
    def generate_performance_charts(self, output_dir: str = "charts") -> None:
        """Generate performance charts"""
        os.makedirs(output_dir, exist_ok=True)
        
        # FPS over time
        if 'fps' in self.performance_metrics:
            self._plot_metric_trend('fps', 'FPS Over Test Runs', 'FPS', 
                                  os.path.join(output_dir, 'fps_trend.png'))
                                  
        # Memory usage
        if 'memory_mb' in self.performance_metrics:
            self._plot_metric_trend('memory_mb', 'Memory Usage Over Test Runs', 'Memory (MB)', 
                                  os.path.join(output_dir, 'memory_trend.png'))
                                  
        # Error distribution pie chart
        if self.error_patterns:
            self._plot_error_distribution(os.path.join(output_dir, 'error_distribution.png'))
            
        # Test success rate over time
        self._plot_success_rate_trend(os.path.join(output_dir, 'success_rate_trend.png'))
        
    def _plot_metric_trend(self, metric: str, title: str, ylabel: str, output_file: str) -> None:
        """Plot a metric trend over time"""
        values = self.performance_metrics[metric]
        if not values:
            return
            
        plt.figure(figsize=(10, 6))
        plt.plot(range(len(values)), values, marker='o', linestyle='-', linewidth=2)
        plt.title(title)
        plt.xlabel('Test Run')
        plt.ylabel(ylabel)
        plt.grid(True, alpha=0.3)
        
        # Add average line
        avg = np.mean(values)
        plt.axhline(y=avg, color='r', linestyle='--', label=f'Average: {avg:.2f}')
        plt.legend()
        
        plt.tight_layout()
        plt.savefig(output_file)
        plt.close()
        
    def _plot_error_distribution(self, output_file: str) -> None:
        """Plot error distribution pie chart"""
        labels = list(self.error_patterns.keys())
        sizes = list(self.error_patterns.values())
        
        plt.figure(figsize=(8, 8))
        plt.pie(sizes, labels=labels, autopct='%1.1f%%', startangle=90)
        plt.title('Error Distribution')
        plt.axis('equal')
        plt.tight_layout()
        plt.savefig(output_file)
        plt.close()
        
    def _plot_success_rate_trend(self, output_file: str) -> None:
        """Plot test success rate trend"""
        success_rates = []
        
        for test in self.test_data:
            total = test['tests_passed'] + test['tests_failed']
            if total > 0:
                rate = (test['tests_passed'] / total) * 100
                success_rates.append(rate)
                
        if not success_rates:
            return
            
        plt.figure(figsize=(10, 6))
        plt.plot(range(len(success_rates)), success_rates, marker='o', linestyle='-', linewidth=2, color='green')
        plt.title('Test Success Rate Over Time')
        plt.xlabel('Test Run')
        plt.ylabel('Success Rate (%)')
        plt.ylim(0, 105)
        plt.grid(True, alpha=0.3)
        
        # Add average line
        avg = np.mean(success_rates)
        plt.axhline(y=avg, color='r', linestyle='--', label=f'Average: {avg:.2f}%')
        plt.legend()
        
        plt.tight_layout()
        plt.savefig(output_file)
        plt.close()
        
    def identify_regression(self, metric: str, threshold: float = 0.1) -> List[Tuple[int, float]]:
        """Identify performance regressions"""
        values = self.performance_metrics[metric]
        if len(values) < 2:
            return []
            
        regressions = []
        baseline = values[0]
        
        for i, value in enumerate(values[1:], 1):
            if metric in ['fps']:  # Higher is better
                if value < baseline * (1 - threshold):
                    regressions.append((i, (baseline - value) / baseline * 100))
            else:  # Lower is better (memory, cpu, render time)
                if value > baseline * (1 + threshold):
                    regressions.append((i, (value - baseline) / baseline * 100))
                    
        return regressions
        
    def export_json_report(self, output_file: str) -> None:
        """Export analysis results as JSON"""
        report = self.generate_summary_report()
        
        # Add regression analysis
        report['regressions'] = {}
        for metric in ['fps', 'memory_mb', 'cpu_percent', 'render_time_ms']:
            if metric in self.performance_metrics:
                regressions = self.identify_regression(metric)
                if regressions:
                    report['regressions'][metric] = [
                        {'test_run': run, 'regression_percent': pct} 
                        for run, pct in regressions
                    ]
                    
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
            
    def print_summary(self) -> None:
        """Print summary to console"""
        report = self.generate_summary_report()
        
        print("\n" + "="*60)
        print("KRYON TEST ANALYSIS SUMMARY")
        print("="*60)
        
        summary = report['summary']
        print(f"\nTest Runs Analyzed: {summary['total_test_runs']}")
        print(f"Total Tests Passed: {summary['total_tests_passed']}")
        print(f"Total Tests Failed: {summary['total_tests_failed']}")
        print(f"Total Warnings: {summary['total_warnings']}")
        print(f"Average Duration: {summary['average_duration_seconds']:.2f}s")
        print(f"Success Rate: {summary['success_rate']:.1f}%")
        
        print("\nPERFORMANCE METRICS:")
        for metric, stats in report['performance'].items():
            print(f"\n{metric}:")
            print(f"  Average: {stats['average']:.2f}")
            print(f"  Min: {stats['min']:.2f}")
            print(f"  Max: {stats['max']:.2f}")
            print(f"  Std Dev: {stats['std_dev']:.2f}")
            
        if report['error_distribution']:
            print("\nERROR DISTRIBUTION:")
            for error_type, count in sorted(report['error_distribution'].items(), key=lambda x: x[1], reverse=True):
                print(f"  {error_type}: {count}")
                
        # Check for regressions
        print("\nPERFORMANCE REGRESSIONS:")
        regression_found = False
        for metric in ['fps', 'memory_mb', 'cpu_percent']:
            if metric in self.performance_metrics:
                regressions = self.identify_regression(metric)
                if regressions:
                    regression_found = True
                    print(f"\n{metric}:")
                    for run, pct in regressions:
                        print(f"  Test run {run}: {pct:.1f}% regression")
                        
        if not regression_found:
            print("  No significant regressions detected")
            
        print("\n" + "="*60)


def main():
    parser = argparse.ArgumentParser(description='Analyze Kryon test results')
    parser.add_argument('--results-dir', default='results', help='Directory containing test results')
    parser.add_argument('--output-dir', default='analysis', help='Output directory for reports and charts')
    parser.add_argument('--json', help='Export JSON report to file')
    parser.add_argument('--charts', action='store_true', help='Generate performance charts')
    parser.add_argument('--pattern', default='*.log', help='Log file pattern to analyze')
    
    args = parser.parse_args()
    
    # Create analyzer
    analyzer = TestResultAnalyzer(args.results_dir)
    
    # Load results
    analyzer.load_results(args.pattern)
    
    # Print summary
    analyzer.print_summary()
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Generate charts if requested
    if args.charts:
        chart_dir = os.path.join(args.output_dir, 'charts')
        analyzer.generate_performance_charts(chart_dir)
        print(f"\nCharts generated in: {chart_dir}")
        
    # Export JSON if requested
    if args.json:
        analyzer.export_json_report(args.json)
        print(f"\nJSON report exported to: {args.json}")
    else:
        # Always export a default JSON report
        default_json = os.path.join(args.output_dir, 'analysis_report.json')
        analyzer.export_json_report(default_json)
        print(f"\nJSON report exported to: {default_json}")


if __name__ == "__main__":
    main()