#!/usr/bin/env python3
"""
Kryon Test Debug Helper
Interactive debugging utilities for test failures and issues
"""

import os
import sys
import json
import subprocess
import argparse
import re
from pathlib import Path
from typing import List, Dict, Any, Optional
import tempfile
import shutil


class KryonDebugHelper:
    def __init__(self, project_root: str = None):
        self.project_root = Path(project_root) if project_root else Path.cwd()
        self.debug_dir = self.project_root / "tests" / "debug_output"
        self.debug_dir.mkdir(parents=True, exist_ok=True)
        
    def analyze_compilation_error(self, kry_file: str) -> Dict[str, Any]:
        """Analyze compilation errors for a KRY file"""
        print(f"🔍 Analyzing compilation error for: {kry_file}")
        
        result = {
            "file": kry_file,
            "exists": os.path.exists(kry_file),
            "syntax_errors": [],
            "semantic_errors": [],
            "suggestions": []
        }
        
        if not result["exists"]:
            result["suggestions"].append("File does not exist - check the path")
            return result
            
        # Try to compile and capture error
        try:
            krb_file = kry_file.replace('.kry', '.krb')
            cmd = ['cargo', 'run', '--bin', 'kryon-compiler', '--', 'compile', kry_file, krb_file]
            
            process = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True, 
                cwd=self.project_root,
                timeout=30
            )
            
            if process.returncode == 0:
                result["suggestions"].append("Compilation successful - no errors found")
                return result
                
            error_output = process.stderr + process.stdout
            result["raw_error"] = error_output
            
            # Parse different types of errors
            result["syntax_errors"] = self._parse_syntax_errors(error_output)
            result["semantic_errors"] = self._parse_semantic_errors(error_output)
            result["suggestions"] = self._generate_error_suggestions(error_output)
            
        except subprocess.TimeoutExpired:
            result["suggestions"].append("Compilation timeout - file may be too complex")
        except Exception as e:
            result["suggestions"].append(f"Failed to run compiler: {e}")
            
        return result
        
    def _parse_syntax_errors(self, error_output: str) -> List[Dict[str, Any]]:
        """Parse syntax errors from compiler output"""
        syntax_errors = []
        
        # Common syntax error patterns
        patterns = [
            r'error:.*unexpected token.*line (\d+)',
            r'error:.*expected.*found.*line (\d+)',
            r'ParseError.*line (\d+)',
            r'SyntaxError.*line (\d+)'
        ]
        
        for pattern in patterns:
            matches = re.finditer(pattern, error_output, re.IGNORECASE)
            for match in matches:
                syntax_errors.append({
                    "type": "syntax",
                    "line": int(match.group(1)) if len(match.groups()) > 0 else None,
                    "message": match.group(0)
                })
                
        return syntax_errors
        
    def _parse_semantic_errors(self, error_output: str) -> List[Dict[str, Any]]:
        """Parse semantic errors from compiler output"""
        semantic_errors = []
        
        # Common semantic error patterns
        patterns = [
            r'error:.*undefined.*',
            r'error:.*type mismatch.*',
            r'error:.*property.*not found.*',
            r'SemanticError.*'
        ]
        
        for pattern in patterns:
            matches = re.finditer(pattern, error_output, re.IGNORECASE)
            for match in matches:
                semantic_errors.append({
                    "type": "semantic", 
                    "message": match.group(0)
                })
                
        return semantic_errors
        
    def _generate_error_suggestions(self, error_output: str) -> List[str]:
        """Generate helpful suggestions based on error output"""
        suggestions = []
        
        error_lower = error_output.lower()
        
        # Common error patterns and suggestions
        if 'unexpected token' in error_lower:
            suggestions.append("Check for missing braces, semicolons, or quotes")
            suggestions.append("Verify property syntax: property_name: value")
            
        if 'expected' in error_lower and 'found' in error_lower:
            suggestions.append("Check for syntax errors around the indicated position")
            suggestions.append("Ensure all blocks are properly closed with }")
            
        if 'undefined' in error_lower:
            suggestions.append("Check if all referenced variables are declared in @variables block")
            suggestions.append("Verify style names are defined before use")
            
        if 'property' in error_lower and 'not found' in error_lower:
            suggestions.append("Check property name spelling")
            suggestions.append("Refer to KRYON_PROPERTIES.md for valid properties")
            
        if 'type mismatch' in error_lower:
            suggestions.append("Verify property value types (string, number, boolean)")
            suggestions.append("Check color format: #RRGGBBAA")
            
        if not suggestions:
            suggestions.append("Enable verbose output for more detailed error information")
            suggestions.append("Check the KRY syntax guide for proper formatting")
            
        return suggestions
        
    def debug_runtime_issue(self, krb_file: str, renderer: str = "debug") -> Dict[str, Any]:
        """Debug runtime issues with a compiled KRB file"""
        print(f"🔧 Debugging runtime issue for: {krb_file} with {renderer} renderer")
        
        result = {
            "file": krb_file,
            "renderer": renderer,
            "exists": os.path.exists(krb_file),
            "runtime_errors": [],
            "performance_issues": [],
            "suggestions": []
        }
        
        if not result["exists"]:
            result["suggestions"].append("KRB file does not exist - compile the KRY file first")
            return result
            
        # Try to run with debug renderer
        try:
            cmd = ['cargo', 'run', '--bin', f'kryon-{renderer}', '--', krb_file]
            if renderer == "debug-interactive":
                cmd = ['cargo', 'run', '--bin', 'kryon-debug-interactive', '--', krb_file]
                
            process = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=self.project_root,
                timeout=10  # Short timeout for debugging
            )
            
            output = process.stdout + process.stderr
            result["raw_output"] = output
            
            # Analyze output for issues
            result["runtime_errors"] = self._parse_runtime_errors(output)
            result["performance_issues"] = self._parse_performance_issues(output)
            result["suggestions"] = self._generate_runtime_suggestions(output, process.returncode)
            
        except subprocess.TimeoutExpired:
            result["suggestions"].append("Runtime timeout - possible infinite loop or hang")
        except Exception as e:
            result["suggestions"].append(f"Failed to run renderer: {e}")
            
        return result
        
    def _parse_runtime_errors(self, output: str) -> List[Dict[str, Any]]:
        """Parse runtime errors from output"""
        errors = []
        
        patterns = [
            r'panic.*',
            r'Error:.*',
            r'FATAL:.*',
            r'thread.*panicked.*'
        ]
        
        for pattern in patterns:
            matches = re.finditer(pattern, output, re.MULTILINE)
            for match in matches:
                errors.append({
                    "type": "runtime",
                    "message": match.group(0).strip()
                })
                
        return errors
        
    def _parse_performance_issues(self, output: str) -> List[Dict[str, Any]]:
        """Parse performance warnings from output"""
        issues = []
        
        patterns = [
            r'WARN.*performance.*',
            r'slow.*render.*',
            r'FPS.*below.*',
            r'memory.*high.*'
        ]
        
        for pattern in patterns:
            matches = re.finditer(pattern, output, re.IGNORECASE | re.MULTILINE)
            for match in matches:
                issues.append({
                    "type": "performance",
                    "message": match.group(0).strip()
                })
                
        return issues
        
    def _generate_runtime_suggestions(self, output: str, return_code: int) -> List[str]:
        """Generate suggestions for runtime issues"""
        suggestions = []
        output_lower = output.lower()
        
        if return_code != 0:
            suggestions.append(f"Process exited with code {return_code}")
            
        if 'panic' in output_lower:
            suggestions.append("Check for array bounds, null pointer access, or assertion failures")
            suggestions.append("Enable debug symbols: cargo build --debug")
            
        if 'segmentation fault' in output_lower or 'sigsegv' in output_lower:
            suggestions.append("Memory safety issue - check for invalid memory access")
            suggestions.append("Run with valgrind or AddressSanitizer for detailed analysis")
            
        if 'permission denied' in output_lower:
            suggestions.append("Check file permissions and paths")
            suggestions.append("Ensure output directories are writable")
            
        if 'library not found' in output_lower or 'symbol not found' in output_lower:
            suggestions.append("Check system dependencies (SDL2, OpenGL, etc.)")
            suggestions.append("Install missing libraries or update LD_LIBRARY_PATH")
            
        if not suggestions:
            suggestions.append("No obvious issues detected - check console output for details")
            
        return suggestions
        
    def create_minimal_test_case(self, problematic_file: str) -> str:
        """Create a minimal test case that reproduces the issue"""
        print(f"🧪 Creating minimal test case for: {problematic_file}")
        
        # Read the problematic file
        try:
            with open(problematic_file, 'r') as f:
                content = f.read()
        except Exception as e:
            print(f"Error reading file: {e}")
            return ""
            
        # Extract minimal components
        minimal_content = self._extract_minimal_components(content)
        
        # Create minimal test file
        minimal_file = self.debug_dir / f"minimal_{Path(problematic_file).stem}.kry"
        
        with open(minimal_file, 'w') as f:
            f.write(minimal_content)
            
        print(f"📝 Minimal test case created: {minimal_file}")
        return str(minimal_file)
        
    def _extract_minimal_components(self, content: str) -> str:
        """Extract minimal components from a KRY file"""
        lines = content.split('\n')
        
        # Essential components for a minimal test
        minimal_lines = [
            "# Minimal test case generated by debug helper",
            "",
            "App {",
            "    window_width: 400",
            "    window_height: 300", 
            "    window_title: \"Debug Test\"",
            "",
            "    Text {",
            "        text: \"Test\"",
            "    }",
            "}"
        ]
        
        # Try to preserve any variables that might be causing issues
        in_variables = False
        for line in lines:
            line = line.strip()
            
            if line.startswith('@variables'):
                in_variables = True
                minimal_lines.insert(-5, "")
                minimal_lines.insert(-5, "@variables {")
                continue
            elif in_variables and line == '}':
                in_variables = False
                minimal_lines.insert(-5, "}")
                continue
            elif in_variables and line:
                minimal_lines.insert(-5, f"    {line}")
                
        return '\n'.join(minimal_lines)
        
    def compare_working_vs_broken(self, working_file: str, broken_file: str) -> Dict[str, Any]:
        """Compare a working file with a broken one to identify differences"""
        print(f"🔍 Comparing working vs broken files")
        
        result = {
            "working_file": working_file,
            "broken_file": broken_file,
            "differences": [],
            "suggestions": []
        }
        
        try:
            with open(working_file, 'r') as f:
                working_content = f.readlines()
        except Exception as e:
            result["suggestions"].append(f"Cannot read working file: {e}")
            return result
            
        try:
            with open(broken_file, 'r') as f:
                broken_content = f.readlines()
        except Exception as e:
            result["suggestions"].append(f"Cannot read broken file: {e}")
            return result
            
        # Simple line-by-line comparison
        import difflib
        
        diff = list(difflib.unified_diff(
            working_content,
            broken_content,
            fromfile=working_file,
            tofile=broken_file,
            lineterm=''
        ))
        
        result["differences"] = diff
        
        # Generate suggestions based on differences
        if len(diff) > 10:
            result["suggestions"].append("Many differences found - consider larger structural changes")
        elif len(diff) == 0:
            result["suggestions"].append("Files are identical - issue may be environmental")
        else:
            result["suggestions"].append("Few differences found - focus on changed lines")
            result["suggestions"].append("Check for typos in property names or values")
            
        return result
        
    def generate_debug_report(self, analysis_results: List[Dict[str, Any]]) -> str:
        """Generate a comprehensive debug report"""
        report_file = self.debug_dir / f"debug_report_{int(os.time.time())}.html"
        
        html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Kryon Debug Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }}
        .header {{ background-color: #e74c3c; color: white; padding: 20px; border-radius: 8px; }}
        .section {{ background-color: white; margin: 20px 0; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }}
        .error {{ color: #e74c3c; background-color: #fdf2f2; padding: 10px; border-radius: 4px; }}
        .warning {{ color: #f39c12; background-color: #fefbf3; padding: 10px; border-radius: 4px; }}
        .success {{ color: #27ae60; background-color: #f2fdf5; padding: 10px; border-radius: 4px; }}
        .code {{ background-color: #f8f9fa; padding: 10px; border-radius: 4px; font-family: monospace; overflow-x: auto; }}
        pre {{ white-space: pre-wrap; }}
        ul {{ margin: 10px 0; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🐛 Kryon Debug Report</h1>
        <p>Generated: {os.strftime('%Y-%m-%d %H:%M:%S')}</p>
    </div>
"""
        
        for i, result in enumerate(analysis_results):
            html_content += f"""
    <div class="section">
        <h2>Analysis {i+1}: {result.get('file', 'Unknown')}</h2>
"""
            
            # Add errors
            if result.get('syntax_errors') or result.get('semantic_errors') or result.get('runtime_errors'):
                html_content += '<h3>❌ Errors Found</h3>'
                
                for error_list in [result.get('syntax_errors', []), result.get('semantic_errors', []), result.get('runtime_errors', [])]:
                    for error in error_list:
                        html_content += f'<div class="error">{error.get("message", "Unknown error")}</div>'
                        
            # Add performance issues
            if result.get('performance_issues'):
                html_content += '<h3>⚠️ Performance Issues</h3>'
                for issue in result['performance_issues']:
                    html_content += f'<div class="warning">{issue.get("message", "Unknown issue")}</div>'
                    
            # Add suggestions
            if result.get('suggestions'):
                html_content += '<h3>💡 Suggestions</h3><ul>'
                for suggestion in result['suggestions']:
                    html_content += f'<li>{suggestion}</li>'
                html_content += '</ul>'
                
            # Add raw output if available
            if result.get('raw_error') or result.get('raw_output'):
                html_content += '<h3>📋 Raw Output</h3>'
                raw_text = result.get('raw_error', '') + result.get('raw_output', '')
                html_content += f'<div class="code"><pre>{raw_text}</pre></div>'
                
            html_content += '</div>'
            
        html_content += """
    <div class="section">
        <h2>🔧 General Debugging Tips</h2>
        <ul>
            <li>Use the debug renderer for detailed output: <code>cargo run --bin kryon-debug-interactive</code></li>
            <li>Enable verbose logging: <code>RUST_LOG=debug</code></li>
            <li>Check system dependencies are installed</li>
            <li>Validate KRY syntax against documentation</li>
            <li>Create minimal test cases to isolate issues</li>
            <li>Compare working vs non-working files</li>
        </ul>
    </div>
</body>
</html>
"""
        
        with open(report_file, 'w') as f:
            f.write(html_content)
            
        print(f"📊 Debug report generated: {report_file}")
        return str(report_file)
        
    def interactive_debug_session(self, file_path: str):
        """Run an interactive debugging session"""
        print(f"🎮 Starting interactive debug session for: {file_path}")
        print("Available commands:")
        print("  1. Analyze compilation errors")
        print("  2. Debug runtime issues") 
        print("  3. Create minimal test case")
        print("  4. Generate debug report")
        print("  5. Exit")
        
        analysis_results = []
        
        while True:
            try:
                choice = input("\nEnter command (1-5): ").strip()
                
                if choice == '1':
                    result = self.analyze_compilation_error(file_path)
                    analysis_results.append(result)
                    print("\n📊 Compilation Analysis:")
                    for suggestion in result.get('suggestions', []):
                        print(f"  💡 {suggestion}")
                        
                elif choice == '2':
                    krb_file = file_path.replace('.kry', '.krb')
                    result = self.debug_runtime_issue(krb_file)
                    analysis_results.append(result)
                    print("\n🔧 Runtime Analysis:")
                    for suggestion in result.get('suggestions', []):
                        print(f"  💡 {suggestion}")
                        
                elif choice == '3':
                    minimal_file = self.create_minimal_test_case(file_path)
                    print(f"\n🧪 Minimal test case: {minimal_file}")
                    
                elif choice == '4':
                    if analysis_results:
                        report_file = self.generate_debug_report(analysis_results)
                        print(f"\n📊 Debug report: {report_file}")
                    else:
                        print("\n❌ No analysis results to report")
                        
                elif choice == '5':
                    print("\n👋 Exiting debug session")
                    break
                    
                else:
                    print("\n❌ Invalid choice, please enter 1-5")
                    
            except KeyboardInterrupt:
                print("\n\n👋 Debug session interrupted")
                break
            except Exception as e:
                print(f"\n❌ Error: {e}")


def main():
    parser = argparse.ArgumentParser(description='Debug Kryon test issues')
    parser.add_argument('file', help='KRY or KRB file to debug')
    parser.add_argument('--mode', choices=['compile', 'runtime', 'minimal', 'compare', 'interactive'], 
                       default='interactive', help='Debug mode')
    parser.add_argument('--renderer', default='debug', help='Renderer to use for runtime debugging')
    parser.add_argument('--compare-with', help='Working file to compare against')
    parser.add_argument('--project-root', help='Project root directory')
    
    args = parser.parse_args()
    
    # Create debug helper
    debugger = KryonDebugHelper(args.project_root)
    
    if args.mode == 'interactive':
        debugger.interactive_debug_session(args.file)
    elif args.mode == 'compile':
        result = debugger.analyze_compilation_error(args.file)
        print(json.dumps(result, indent=2))
    elif args.mode == 'runtime':
        krb_file = args.file.replace('.kry', '.krb')
        result = debugger.debug_runtime_issue(krb_file, args.renderer)
        print(json.dumps(result, indent=2))
    elif args.mode == 'minimal':
        minimal_file = debugger.create_minimal_test_case(args.file)
        print(f"Minimal test case created: {minimal_file}")
    elif args.mode == 'compare':
        if not args.compare_with:
            print("Error: --compare-with is required for compare mode")
            sys.exit(1)
        result = debugger.compare_working_vs_broken(args.compare_with, args.file)
        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()