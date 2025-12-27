#!/usr/bin/env bun
/**
 * TSX to KIR Converter
 * Parses Kryon TSX/JSX and converts to Kryon Intermediate Representation
 */

import { parseArgs } from "util";

interface KIRComponent {
  id?: number;
  type: string;
  [key: string]: any;  // Props are flattened at top level
  children?: KIRComponent[];
  text?: string;
}

let nextId = 1;

/**
 * Parse JSX element recursively
 */
function parseJSXElement(node: any): KIRComponent | null {
  // Handle text nodes
  if (typeof node === 'string') {
    return {
      id: nextId++,
      type: "Text",
      text: node
    };
  }

  if (typeof node === 'number') {
    return {
      id: nextId++,
      type: "Text",
      text: String(node)
    };
  }

  // Handle JSX elements
  if (node && typeof node === 'object') {
    // Extract type
    let type = node.type;
    if (typeof type === 'function') {
      type = type.name;
    }
    if (!type) return null;

    // Create component with ID and type
    const component: KIRComponent = {
      id: nextId++,
      type
    };

    // Automatically set flexDirection based on component type
    if (type === 'Row') {
      component.flexDirection = 'row';
    } else if (type === 'Column') {
      component.flexDirection = 'column';
    }

    // Flatten props directly onto component (KIR v3.0 format)
    if (node.props) {
      for (const [key, value] of Object.entries(node.props)) {
        // Skip children and key
        if (key === 'children' || key === 'key') continue;

        // Handle special prop names
        if (key === 'className') {
          component.class = value;
        } else {
          component[key] = value;
        }
      }
    }

    // Extract children
    const children: KIRComponent[] = [];
    if (node.props?.children) {
      const childNodes = Array.isArray(node.props.children)
        ? node.props.children
        : [node.props.children];

      for (const child of childNodes) {
        const parsed = parseJSXElement(child);
        if (parsed) {
          children.push(parsed);
        }
      }
    }

    if (children.length > 0) {
      component.children = children;
    }

    return component;
  }

  return null;
}

/**
 * Transpile TSX to JSX and evaluate to get component tree
 */
async function parseKryonTSX(source: string): Promise<KIRComponent> {
  // Create mock Kryon components that return JSX data structures
  const mockComponents: Record<string, any> = {};

  const componentTypes = [
    'Column', 'Row', 'Container', 'Text', 'Button', 'Input',
    'Checkbox', 'Image', 'Link', 'Canvas', 'Center',
    'Table', 'TableHead', 'TableBody', 'TableFoot', 'TableRow',
    'TableCell', 'TableHeaderCell',
    'Heading', 'Paragraph', 'Blockquote', 'CodeBlock',
    'HorizontalRule', 'List', 'ListItem',
    'TabGroup', 'TabBar', 'Tab', 'TabContent', 'TabPanel',
    'Dropdown'
  ];

  // Create mock components
  for (const type of componentTypes) {
    mockComponents[type] = (props: any) => ({
      type,
      props,
    });
  }

  // Special handler for kryonApp
  const kryonApp = (config: any) => {
    if (config.render && typeof config.render === 'function') {
      return config.render();
    }
    return { type: 'Container', props: config };
  };

  try {
    // Transpile TSX to JS using Bun's transpiler
    const transpiler = new Bun.Transpiler({
      loader: "tsx",
    });

    let code = await transpiler.transform(source);

    // Remove import statements (we provide mocks)
    code = code.replace(/import\s+.*?from\s+['"].*?['"];?/g, '');
    code = code.replace(/import\s+['"].*?['"];?/g, '');

    // Replace exports
    code = code.replace(/export\s+default\s+/, 'const __kryonApp = ');
    code = code.replace(/export\s+(const|let|var|function)\s+/g, '$1 ');

    // JSX factory function (handles both development and production mode)
    const jsxFactory = (type: any, props: any, key?: any, source?: any, self?: any) => {
      const allProps = { ...(props || {}) };

      // Remove __source and __self (dev mode props)
      delete allProps.__source;
      delete allProps.__self;

      if (typeof type === 'string') {
        return { type, props: allProps };
      } else if (typeof type === 'function') {
        return type(allProps);
      }
      return { type: 'Unknown', props: allProps };
    };

    // Create evaluation context with all possible JSX runtime functions
    const evalContext = {
      ...mockComponents,
      kryonApp,
      // Standard React API
      React: {
        createElement: jsxFactory,
        Fragment: ({ children }: any) => children,
      },
      // New JSX transform (automatic runtime)
      jsx: jsxFactory,
      jsxs: jsxFactory,
      jsxDEV: jsxFactory,
      Fragment: ({ children }: any) => children,
    };

    // Add dynamic JSX functions (Bun generates random names like jsxDEV_7x81h0kn)
    const jsxFunctionPattern = /\b(jsx\w+)\(/g;
    let match;
    const jsxFunctions = new Set<string>();
    while ((match = jsxFunctionPattern.exec(code)) !== null) {
      jsxFunctions.add(match[1]);
    }

    for (const fnName of jsxFunctions) {
      (evalContext as any)[fnName] = jsxFactory;
    }

    // Evaluate the transpiled code
    const evalCode = `
      ${Object.keys(evalContext).map(key => `const ${key} = __context.${key};`).join('\n')}
      ${code}
      return __kryonApp;
    `;

    if (process.env.DEBUG_TSX_PARSER) {
      console.error('=== Transpiled Code ===');
      console.error(evalCode);
      console.error('======================');
    }

    const AsyncFunction = Object.getPrototypeOf(async function(){}).constructor;
    const fn = new AsyncFunction('__context', evalCode);
    const result = await fn(evalContext);

    // Parse the result
    if (process.env.DEBUG_TSX_PARSER) {
      console.error('Result:', JSON.stringify(result, null, 2));
    }

    const parsed = parseJSXElement(result);

    if (!parsed) {
      console.error('Parse failed. Result was:', result);
      console.error('Result type:', typeof result);
      throw new Error('Failed to parse component tree');
    }

    return parsed;

  } catch (error) {
    console.error('Error parsing TSX:', error);
    throw error;
  }
}

/**
 * Main entry point
 */
async function main() {
  const args = process.argv.slice(2);

  if (args.length === 0) {
    console.error("Usage: tsx_to_kir.ts <input.tsx>");
    process.exit(1);
  }

  const inputFile = args[0];

  try {
    // Read the source file
    const file = Bun.file(inputFile);
    const source = await file.text();

    // Reset ID counter
    nextId = 1;

    // Parse to KIR
    const root = await parseKryonTSX(source);

    // Output KIR v3.0 format
    const output = {
      format_version: "3.0",
      component_definitions: [],
      root
    };

    console.log(JSON.stringify(output, null, 2));

  } catch (error) {
    console.error("Error:", error);
    process.exit(1);
  }
}

main();
