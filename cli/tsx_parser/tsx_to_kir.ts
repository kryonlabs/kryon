#!/usr/bin/env bun
/**
 * TSX to KIR Converter - Enhanced Version
 * Parses Kryon TSX/JSX and converts to Kryon Intermediate Representation
 *
 * Features:
 * - Detects ALL event handlers (onClick, onChange, onSubmit, onFocus, etc.)
 * - Detects React hooks (useState, useEffect, useCallback, useMemo)
 * - Builds reactive_manifest with state variables and types
 * - Preserves hook dependencies for proper generation
 */

import { parseArgs } from "util";

interface KIRComponent {
  id?: number;
  type: string;
  [key: string]: any;  // Props are flattened at top level
  children?: KIRComponent[];
  text?: string;
  events?: Array<{
    type: string;
    logic_id: string;
    handler_data?: string;
  }>;
}

interface LogicFunction {
  name: string;
  sources: Array<{
    language: string;
    source: string;
  }>;
}

interface EventBinding {
  component_id: number;
  event_type: string;
  handler_name: string;
}

interface ReactiveVar {
  id: number;
  name: string;
  type: string;
  initial_value: string;
  setter_name?: string;
}

interface Hook {
  type: "useEffect" | "useCallback" | "useMemo" | "useReducer";
  callback: string;
  dependencies?: string;
}

let nextId = 1;
let nextHandlerId = 1;
const logicFunctions: LogicFunction[] = [];
const eventBindings: EventBinding[] = [];
const reactiveVars: ReactiveVar[] = [];
const hooks: Hook[] = [];

/**
 * Infer type from initial value
 */
function inferType(initialValue: string): string {
  const val = initialValue.trim();

  // Boolean
  if (val === "true" || val === "false") return "boolean";

  // Number (integer or float)
  if (/^-?\d+$/.test(val)) return "number";
  if (/^-?\d+\.\d+$/.test(val)) return "number";

  // String (quoted)
  if (val.startsWith("'") || val.startsWith('"') || val.startsWith("`")) return "string";

  // Null/undefined
  if (val === "null" || val === "undefined") return "any";

  // Array
  if (val.startsWith("[")) return "array";

  // Object
  if (val.startsWith("{")) return "object";

  return "any";
}

/**
 * Detect useState hooks in source code
 */
function detectUseState(source: string): void {
  // Match: const [varName, setVarName] = useState(initialValue)
  // Handles various formats including with type annotations
  const useStatePattern = /const\s+\[([a-zA-Z_$][a-zA-Z0-9_$]*)\s*,\s*([a-zA-Z_$][a-zA-Z0-9_$]*)\]\s*=\s*useState(?:<[^>]+>)?\(([^)]*)\)/g;

  let match;
  while ((match = useStatePattern.exec(source)) !== null) {
    const varName = match[1];
    const setterName = match[2];
    const initialValue = match[3].trim() || "undefined";

    const type = inferType(initialValue);

    reactiveVars.push({
      id: reactiveVars.length + 1,
      name: varName,
      type: type,
      initial_value: initialValue,
      setter_name: setterName
    });
  }
}

/**
 * Detect useEffect hooks in source code
 */
function detectUseEffect(source: string): void {
  // Match: useEffect(() => { ... }, [deps])
  // This is a simplified regex that captures most common patterns
  const useEffectPattern = /useEffect\(\s*((?:\([^)]*\)|[^,])*?)\s*,\s*(\[[^\]]*\])\s*\)/g;

  let match;
  while ((match = useEffectPattern.exec(source)) !== null) {
    const callback = match[1].trim();
    const dependencies = match[2].trim();

    hooks.push({
      type: "useEffect",
      callback: callback,
      dependencies: dependencies
    });
  }
}

/**
 * Detect useCallback hooks in source code
 */
function detectUseCallback(source: string): void {
  const useCallbackPattern = /const\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*useCallback\(\s*((?:\([^)]*\)|[^,])*?)\s*,\s*(\[[^\]]*\])\s*\)/g;

  let match;
  while ((match = useCallbackPattern.exec(source)) !== null) {
    const varName = match[1];
    const callback = match[2].trim();
    const dependencies = match[3].trim();

    hooks.push({
      type: "useCallback",
      callback: callback,
      dependencies: dependencies
    });
  }
}

/**
 * Detect useMemo hooks in source code
 */
function detectUseMemo(source: string): void {
  const useMemoPattern = /const\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*useMemo\(\s*((?:\([^)]*\)|[^,])*?)\s*,\s*(\[[^\]]*\])\s*\)/g;

  let match;
  while ((match = useMemoPattern.exec(source)) !== null) {
    const varName = match[1];
    const callback = match[2].trim();
    const dependencies = match[3].trim();

    hooks.push({
      type: "useMemo",
      callback: callback,
      dependencies: dependencies
    });
  }
}

/**
 * Scan source code for all React hooks
 */
function scanForHooks(source: string): void {
  detectUseState(source);
  detectUseEffect(source);
  detectUseCallback(source);
  detectUseMemo(source);
}

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
    const componentEvents: Array<{type: string; logic_id: string; handler_data: string}> = [];

    if (node.props) {
      for (const [key, value] of Object.entries(node.props)) {
        // Skip children and key
        if (key === 'children' || key === 'key') continue;

        // Handle ALL event handlers (not just onClick)
        if (key.startsWith('on') && key.length > 2 && typeof value === 'function') {
          const eventType = key.slice(2).toLowerCase(); // onClick -> click, onChange -> change
          const handlerName = `handler_${nextHandlerId++}_${eventType}`;

          // Extract function source code
          const handlerSource = value.toString();

          // Create logic function
          logicFunctions.push({
            name: handlerName,
            sources: [{
              language: 'typescript',
              source: handlerSource
            }]
          });

          // Create event binding
          eventBindings.push({
            component_id: component.id!,
            event_type: eventType,
            handler_name: handlerName
          });

          // Add to component events array
          componentEvents.push({
            type: eventType,
            logic_id: handlerName,
            handler_data: handlerSource
          });

          continue; // Don't add event handler as a regular prop
        }

        // Handle special prop names
        if (key === 'className') {
          component.class = value;
        } else {
          component[key] = value;
        }
      }
    }

    // Add events array if there are any event handlers
    if (componentEvents.length > 0) {
      component.events = componentEvents;
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
  // FIRST: Scan source for React hooks
  scanForHooks(source);

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
      // Mock hooks (so code doesn't crash when evaluated)
      useState: (initial: any) => [initial, () => {}],
      useEffect: () => {},
      useCallback: (fn: any) => fn,
      useMemo: (fn: any) => fn(),
      useReducer: (reducer: any, initial: any) => [initial, () => {}],
      useRef: (initial: any) => ({ current: initial }),
      useContext: () => ({}),
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

    // Reset ID counters and logic storage
    nextId = 1;
    nextHandlerId = 1;
    logicFunctions.length = 0;
    eventBindings.length = 0;
    reactiveVars.length = 0;
    hooks.length = 0;

    // Parse to KIR
    const root = await parseKryonTSX(source);

    // Build output with logic_block and reactive_manifest
    const output: any = {
      format: "kir",
      metadata: {
        source_language: "tsx",
        compiler_version: "kryon-1.0.0"
      },
      root
    };

    // Add logic_block if there are any event handlers
    if (logicFunctions.length > 0) {
      output.logic_block = {
        functions: logicFunctions,
        event_bindings: eventBindings
      };
    }

    // Add reactive_manifest if there are any state variables or hooks
    if (reactiveVars.length > 0 || hooks.length > 0) {
      output.reactive_manifest = {
        variables: reactiveVars,
        hooks: hooks
      };
    }

    console.log(JSON.stringify(output, null, 2));

  } catch (error) {
    console.error("Error:", error);
    process.exit(1);
  }
}

main();
