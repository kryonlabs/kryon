#!/usr/bin/env python3
"""
Kryon Test Data Generator
Generates test data, stress test scenarios, and edge case test files
"""

import json
import random
import string
import argparse
from typing import List, Dict, Any
from pathlib import Path
import math


class KryonTestDataGenerator:
    def __init__(self):
        self.colors = [
            "#FF0000FF", "#00FF00FF", "#0000FFFF", "#FFFF00FF", "#FF00FFFF", "#00FFFFFF",
            "#800000FF", "#008000FF", "#000080FF", "#808000FF", "#800080FF", "#008080FF",
            "#C0C0C0FF", "#808080FF", "#9999FFFF", "#993366FF", "#FFFFCCFF", "#CCFFFFCC",
            "#660066FF", "#FF8080FF", "#0066CCFF", "#CCCCFFFF", "#000080FF", "#FF00FFFF"
        ]
        
        self.fonts = [8, 10, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48]
        self.weights = [100, 200, 300, 400, 500, 600, 700, 800, 900]
        
        self.element_types = ["Container", "Text", "Button", "Input", "Checkbox", "Slider"]
        
        self.lorem_words = [
            "lorem", "ipsum", "dolor", "sit", "amet", "consectetur", "adipiscing", "elit",
            "sed", "do", "eiusmod", "tempor", "incididunt", "ut", "labore", "et", "dolore",
            "magna", "aliqua", "enim", "ad", "minim", "veniam", "quis", "nostrud",
            "exercitation", "ullamco", "laboris", "nisi", "aliquip", "ex", "ea", "commodo",
            "consequat", "duis", "aute", "irure", "in", "reprehenderit", "voluptate",
            "velit", "esse", "cillum", "fugiat", "nulla", "pariatur", "excepteur", "sint"
        ]
        
    def generate_random_text(self, min_words: int = 1, max_words: int = 10) -> str:
        """Generate random lorem ipsum text"""
        word_count = random.randint(min_words, max_words)
        words = random.choices(self.lorem_words, k=word_count)
        return " ".join(words).capitalize()
        
    def generate_random_color(self) -> str:
        """Generate random color"""
        return random.choice(self.colors)
        
    def generate_random_dimensions(self, min_size: int = 50, max_size: int = 500) -> Dict[str, int]:
        """Generate random width/height"""
        return {
            "width": random.randint(min_size, max_size),
            "height": random.randint(min_size, max_size)
        }
        
    def generate_style(self, name: str, element_type: str = "Container") -> str:
        """Generate a random style definition"""
        style = f'style "{name}" {{\n'
        
        # Basic properties
        style += f'    background_color: "{self.generate_random_color()}"\n'
        style += f'    text_color: "{self.generate_random_color()}"\n'
        
        # Layout properties
        if random.choice([True, False]):
            style += '    display: flex\n'
            style += f'    flex_direction: {random.choice(["row", "column"])}\n'
            style += f'    justify_content: {random.choice(["flex-start", "center", "flex-end", "space-between"])}\n'
            style += f'    align_items: {random.choice(["stretch", "flex-start", "center", "flex-end"])}\n'
            
        # Spacing
        style += f'    padding: {random.randint(5, 30)}\n'
        style += f'    margin: {random.randint(0, 20)}\n'
        style += f'    gap: {random.randint(5, 25)}\n'
        
        # Border
        if random.choice([True, False]):
            style += f'    border_width: {random.randint(1, 5)}\n'
            style += f'    border_color: "{self.generate_random_color()}"\n'
            style += f'    border_radius: {random.randint(0, 20)}\n'
            
        # Typography (for text elements)
        if element_type in ["Text", "Button"]:
            style += f'    font_size: {random.choice(self.fonts)}\n'
            style += f'    font_weight: {random.choice(self.weights)}\n'
            style += f'    text_alignment: {random.choice(["left", "center", "right"])}\n'
            
        # Sizing
        if random.choice([True, False]):
            dims = self.generate_random_dimensions()
            style += f'    width: {dims["width"]}\n'
            style += f'    height: {dims["height"]}\n'
            
        style += '}\n\n'
        return style
        
    def generate_element(self, element_type: str, id_prefix: str = "", depth: int = 0) -> str:
        """Generate a random element"""
        element_id = f"{id_prefix}_{element_type}_{random.randint(1000, 9999)}"
        style_name = f"style_{random.randint(1000, 9999)}"
        
        indent = "    " * (depth + 1)
        element = f'{indent}{element_type} {{\n'
        element += f'{indent}    id: "{element_id}"\n'
        
        if element_type == "Text":
            element += f'{indent}    text: "{self.generate_random_text()}"\n'
        elif element_type == "Button":
            element += f'{indent}    text: "{self.generate_random_text(1, 3)}"\n'
            element += f'{indent}    onClick: "handle_{element_id}"\n'
        elif element_type == "Input":
            input_types = ["text", "password", "email", "number"]
            element += f'{indent}    input_type: {random.choice(input_types)}\n'
            element += f'{indent}    placeholder: "{self.generate_random_text(1, 4)}"\n'
        elif element_type == "Checkbox":
            element += f'{indent}    text: "{self.generate_random_text(1, 4)}"\n'
            element += f'{indent}    checked: {random.choice(["true", "false"])}\n'
        elif element_type == "Slider":
            min_val = random.randint(0, 50)
            max_val = random.randint(min_val + 10, 100)
            element += f'{indent}    min_value: {min_val}\n'
            element += f'{indent}    max_value: {max_val}\n'
            element += f'{indent}    value: {random.randint(min_val, max_val)}\n'
            
        # Add style reference
        if random.choice([True, False]):
            element += f'{indent}    style: "{style_name}"\n'
            
        element += f'{indent}}}\n'
        
        return element, style_name
        
    def generate_stress_test_app(self, element_count: int, max_depth: int = 3) -> str:
        """Generate a stress test application with many elements"""
        app_content = f"# Stress Test - {element_count} Elements\n"
        app_content += f"# Auto-generated test file for performance testing\n\n"
        
        # Generate styles
        styles = []
        for i in range(min(element_count // 5, 50)):  # Limit styles to reasonable number
            style_name = f"stress_style_{i}"
            element_type = random.choice(self.element_types)
            style_def = self.generate_style(style_name, element_type)
            styles.append(style_def)
            
        app_content += "".join(styles)
        
        # Variables
        app_content += "@variables {\n"
        for i in range(random.randint(5, 15)):
            var_name = f"var_{i}"
            var_value = random.choice([
                f'"{self.generate_random_text()}"',
                str(random.randint(0, 1000)),
                random.choice(["true", "false"])
            ])
            app_content += f"    {var_name}: {var_value}\n"
        app_content += "}\n\n"
        
        # App definition
        app_content += "App {\n"
        app_content += f"    window_width: {random.randint(800, 1200)}\n"
        app_content += f"    window_height: {random.randint(600, 900)}\n"
        app_content += f'    window_title: "Stress Test - {element_count} Elements"\n'
        app_content += "    resizable: true\n"
        app_content += "    keep_aspect_ratio: false\n"
        app_content += f'    background_color: "{self.generate_random_color()}"\n\n'
        
        # Generate main container with nested elements
        elements_generated = 0
        app_content += self._generate_nested_elements(element_count, max_depth, 1)
        
        app_content += "}\n\n"
        
        # Generate Lua functions
        app_content += self._generate_lua_functions(element_count)
        
        return app_content
        
    def _generate_nested_elements(self, target_count: int, max_depth: int, current_depth: int) -> str:
        """Generate nested elements recursively"""
        if target_count <= 0 or current_depth > max_depth:
            return ""
            
        content = ""
        indent = "    " * current_depth
        elements_at_level = min(target_count, random.randint(1, 10))
        
        for i in range(elements_at_level):
            element_type = random.choice(self.element_types)
            element, style_name = self.generate_element(element_type, f"stress_{current_depth}", current_depth)
            
            if element_type == "Container" and current_depth < max_depth and target_count > elements_at_level:
                # Add nested elements to container
                container_lines = element.split('\n')
                container_content = '\n'.join(container_lines[:-2])  # Remove closing braces
                
                # Generate children
                child_count = min(target_count - elements_at_level, random.randint(1, 5))
                children = self._generate_nested_elements(child_count, max_depth, current_depth + 1)
                
                container_content += children
                container_content += f'{indent}    }}\n'
                content += container_content
                target_count -= child_count
            else:
                content += element
                
            target_count -= 1
            if target_count <= 0:
                break
                
        return content
        
    def _generate_lua_functions(self, element_count: int) -> str:
        """Generate Lua functions for the test"""
        functions = []
        
        # Counter function
        functions.append('''@function "lua" incrementCounter() {
    counter = counter + 1
    print("Counter: " .. counter)
}''')
        
        # Random color function
        functions.append(f'''@function "lua" randomizeColors() {{
    local colors = {{"{'", "'.join(random.choices(self.colors, k=5))}"}}
    local index = math.random(1, #colors)
    current_color = colors[index]
    print("Color changed to: " .. current_color)
}}''')
        
        # Performance test function
        functions.append(f'''@function "lua" performanceTest() {{
    local start_time = os.clock()
    
    -- Simulate some work
    for i = 1, {element_count} do
        local x = math.sin(i) * math.cos(i)
    end
    
    local end_time = os.clock()
    local duration = (end_time - start_time) * 1000
    
    print("Performance test completed in " .. string.format("%.2f", duration) .. "ms")
}}''')
        
        # Init function
        functions.append(f'''@init {{
    counter = 0
    current_color = "{random.choice(self.colors)}"
    print("Stress test initialized with {element_count} elements")
}}''')
        
        return "\n\n".join(functions) + "\n"
        
    def generate_edge_case_test(self, case_type: str) -> str:
        """Generate edge case test files"""
        if case_type == "empty":
            return self._generate_empty_test()
        elif case_type == "minimal":
            return self._generate_minimal_test()
        elif case_type == "unicode":
            return self._generate_unicode_test()
        elif case_type == "deep_nesting":
            return self._generate_deep_nesting_test()
        elif case_type == "large_text":
            return self._generate_large_text_test()
        elif case_type == "extreme_colors":
            return self._generate_extreme_colors_test()
        else:
            raise ValueError(f"Unknown edge case type: {case_type}")
            
    def _generate_empty_test(self) -> str:
        """Generate minimal empty test"""
        return '''# Empty Test Case
# Tests behavior with minimal content

App {
    window_width: 400
    window_height: 300
    window_title: "Empty Test"
}
'''
        
    def _generate_minimal_test(self) -> str:
        """Generate minimal viable test"""
        return '''# Minimal Test Case
# Simplest possible functional test

App {
    window_width: 200
    window_height: 150
    window_title: "Minimal"
    
    Text {
        text: "Test"
    }
}
'''
        
    def _generate_unicode_test(self) -> str:
        """Generate unicode text test"""
        unicode_texts = [
            "Hello 世界",
            "Тест",
            "🚀 🎨 🧪",
            "café naïve résumé",
            "العربية",
            "हिन्दी",
            "日本語",
            "한국어"
        ]
        
        content = '''# Unicode Test Case
# Tests unicode text rendering

style "unicode_style" {
    background_color: "#2C2C2CFF"
    text_color: "#FFFFFFFF"
    padding: 10
    font_size: 16
}

App {
    window_width: 600
    window_height: 400
    window_title: "Unicode Test"
    
    Container {
        display: flex
        flex_direction: column
        gap: 10
        padding: 20
        
'''
        
        for i, text in enumerate(unicode_texts):
            content += f'''        Text {{
            id: "unicode_{i}"
            text: "{text}"
            style: "unicode_style"
        }}
        
'''
        
        content += '''    }
}
'''
        return content
        
    def _generate_deep_nesting_test(self) -> str:
        """Generate deeply nested container test"""
        content = '''# Deep Nesting Test Case
# Tests performance with deeply nested containers

App {
    window_width: 800
    window_height: 600
    window_title: "Deep Nesting Test"
    
'''
        
        # Create 20 levels of nesting
        for i in range(20):
            indent = "    " * (i + 1)
            content += f'''{indent}Container {{
{indent}    id: "level_{i}"
{indent}    padding: {i + 1}
{indent}    background_color: "#{random.randint(0, 255):02x}{random.randint(0, 255):02x}{random.randint(0, 255):02x}FF"
        
'''
        
        # Add final text element
        indent = "    " * 21
        content += f'''{indent}Text {{
{indent}    text: "Deeply nested text at level 20"
{indent}    font_size: 12
{indent}}}
'''
        
        # Close all containers
        for i in range(19, -1, -1):
            indent = "    " * (i + 1)
            content += f'''{indent}}}
'''
        
        content += "}\n"
        return content
        
    def _generate_large_text_test(self) -> str:
        """Generate test with very large text content"""
        large_text = " ".join(self.lorem_words * 100)  # Very long text
        
        return f'''# Large Text Test Case
# Tests rendering of very large text content

style "large_text_style" {{
    background_color: "#FFFFFFFF"
    text_color: "#000000FF"
    padding: 20
    font_size: 14
    text_alignment: left
}}

App {{
    window_width: 800
    window_height: 600
    window_title: "Large Text Test"
    
    Container {{
        style: "large_text_style"
        
        Text {{
            text: "{large_text}"
        }}
    }}
}}
'''
        
    def _generate_extreme_colors_test(self) -> str:
        """Generate test with extreme color values"""
        extreme_colors = [
            "#00000000",  # Fully transparent
            "#FFFFFFFF",  # Fully opaque white
            "#000000FF",  # Fully opaque black
            "#FF000001",  # Nearly transparent red
            "#00FF00FE",  # Nearly opaque green
            "#0000FFFF",  # Fully opaque blue
        ]
        
        content = '''# Extreme Colors Test Case
# Tests edge cases in color rendering

App {
    window_width: 600
    window_height: 400
    window_title: "Extreme Colors Test"
    
    Container {
        display: flex
        flex_direction: row
        flex_wrap: wrap
        gap: 10
        padding: 20
        
'''
        
        for i, color in enumerate(extreme_colors):
            content += f'''        Container {{
            id: "color_{i}"
            width: 100
            height: 100
            background_color: "{color}"
            border_width: 2
            border_color: "#808080FF"
            
            Text {{
                text: "Color {i}"
                font_size: 10
                text_color: "#FFFFFFFF"
                text_alignment: center
                padding: 40
            }}
        }}
        
'''
        
        content += '''    }
}
'''
        return content


def main():
    parser = argparse.ArgumentParser(description='Generate Kryon test data')
    parser.add_argument('--type', choices=['stress', 'edge'], default='stress',
                       help='Type of test to generate')
    parser.add_argument('--output', '-o', required=True,
                       help='Output file path')
    parser.add_argument('--count', type=int, default=100,
                       help='Number of elements for stress test')
    parser.add_argument('--depth', type=int, default=5,
                       help='Maximum nesting depth')
    parser.add_argument('--edge-case', choices=['empty', 'minimal', 'unicode', 'deep_nesting', 'large_text', 'extreme_colors'],
                       help='Type of edge case test')
    
    args = parser.parse_args()
    
    generator = KryonTestDataGenerator()
    
    if args.type == 'stress':
        content = generator.generate_stress_test_app(args.count, args.depth)
    elif args.type == 'edge':
        if not args.edge_case:
            print("Error: --edge-case is required when --type is 'edge'")
            sys.exit(1)
        content = generator.generate_edge_case_test(args.edge_case)
    
    # Write to file
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"Generated test file: {output_path}")
    
    if args.type == 'stress':
        print(f"  Elements: {args.count}")
        print(f"  Max depth: {args.depth}")
    else:
        print(f"  Edge case: {args.edge_case}")


if __name__ == "__main__":
    main()