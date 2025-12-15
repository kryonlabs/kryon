#!/usr/bin/env bun
/**
 * TSX to KIR Parser
 *
 * Parses TSX files and outputs KIR-compatible JSON.
 * Uses Bun's native TypeScript parser.
 *
 * Usage:
 *   bun scripts/tsx/parse_tsx.ts <input.tsx> [output.kir]
 */

import { parseSync, traverse, types as t, Node } from '@babel/core';
import * as parser from '@babel/parser';
import * as fs from 'fs';

// Component ID counter
let nextId = 1;

// Parsed state
interface ParsedState {
  componentDefs: Map<string, ComponentDef>;
  rootConfig: RootConfig;
  root: KirNode | null;
  reactiveVars: ReactiveVar[];
  logic: LogicBlock;
}

interface ComponentDef {
  name: string;
  props: { name: string; type: string; default?: string | number | boolean }[];
  state: { name: string; type: string; initial: string }[];
  template: KirNode | null;
}

interface RootConfig {
  width: number;
  height: number;
  title: string;
  background: string;
}

interface KirNode {
  id: number;
  type: string;
  [key: string]: any;
  children?: KirNode[];
}

interface ReactiveVar {
  id: number;
  name: string;
  type: string;
  initial_value: string;
  scope: string;
}

interface LogicBlock {
  functions: { [name: string]: any };
  event_bindings: { component_id: number; event: string; handler: string }[];
}

function parseValue(node: t.Node): any {
  if (t.isStringLiteral(node)) {
    return node.value;
  } else if (t.isNumericLiteral(node)) {
    return node.value;
  } else if (t.isBooleanLiteral(node)) {
    return node.value;
  } else if (t.isJSXExpressionContainer(node)) {
    return parseValue(node.expression);
  } else if (t.isIdentifier(node)) {
    return { var: node.name };
  } else if (t.isCallExpression(node)) {
    // String(value) -> template expression
    if (t.isIdentifier(node.callee) && node.callee.name === 'String') {
      const arg = node.arguments[0];
      if (t.isIdentifier(arg)) {
        return `{{${arg.name}}}`;
      }
    }
  } else if (t.isArrowFunctionExpression(node) || t.isFunctionExpression(node)) {
    // Event handler
    return { handler: true, body: node.body };
  } else if (t.isArrayExpression(node)) {
    return node.elements.map(el => el ? parseValue(el) : null);
  }
  return null;
}

function formatDimension(value: any): string {
  if (typeof value === 'number') {
    return `${value}.0px`;
  } else if (typeof value === 'string') {
    if (value.endsWith('%') || value.endsWith('px')) {
      return value;
    }
    return value;
  }
  return String(value);
}

function parseJSXElement(node: t.JSXElement, state: ParsedState): KirNode {
  const id = nextId++;
  const result: KirNode = { id, type: '' };

  // Get element name
  const openingElement = node.openingElement;
  if (t.isJSXIdentifier(openingElement.name)) {
    result.type = openingElement.name.name;
  }

  // Parse attributes
  for (const attr of openingElement.attributes) {
    if (t.isJSXAttribute(attr) && t.isJSXIdentifier(attr.name)) {
      const name = attr.name.name;
      const value = attr.value ? parseValue(attr.value) : true;

      // Map property names
      switch (name) {
        case 'width':
        case 'height':
          if (typeof value === 'number') {
            result[name] = `${value}.0px`;
          } else if (typeof value === 'string' && value.endsWith('%')) {
            result[name] = value.replace('%', '.0%');
          } else {
            result[name] = value;
          }
          break;
        case 'background':
          result.background = value;
          break;
        case 'title':
          result.windowTitle = value;
          break;
        case 'text':
          if (typeof value === 'string' && value.startsWith('{{')) {
            result.text_expression = value;
            result.text = value;
          } else {
            result.text = value;
          }
          break;
        case 'onClick':
          if (value && typeof value === 'object' && value.handler) {
            // Register event handler
            const handlerName = `handler_${id}_click`;
            result.events = result.events || [];
            result.events.push({ type: 'click', logic_id: handlerName });

            // Parse handler body
            if (t.isCallExpression(value.body)) {
              const call = value.body;
              if (t.isIdentifier(call.callee)) {
                const setterName = call.callee.name;
                // Extract variable name from setter (setValue -> value)
                if (setterName.startsWith('set')) {
                  const varName = setterName.charAt(3).toLowerCase() + setterName.slice(4);
                  const arg = call.arguments[0];

                  if (t.isBinaryExpression(arg)) {
                    const left = arg.left;
                    const right = arg.right;
                    const op = arg.operator;

                    if (t.isIdentifier(left) && t.isNumericLiteral(right)) {
                      state.logic.functions[handlerName] = {
                        universal: {
                          statements: [{
                            op: 'assign',
                            target: varName,
                            expr: {
                              op: op === '+' ? 'add' : 'sub',
                              left: { var: varName },
                              right: right.value
                            }
                          }]
                        }
                      };
                      state.logic.event_bindings.push({
                        component_id: id,
                        event: 'click',
                        handler: handlerName
                      });
                    }
                  }
                }
              }
            }
          }
          break;
        default:
          result[name] = value;
      }
    }
  }

  // Parse children
  const children: KirNode[] = [];
  for (const child of node.children) {
    if (t.isJSXElement(child)) {
      children.push(parseJSXElement(child, state));
    } else if (t.isJSXFragment(child)) {
      for (const fragChild of child.children) {
        if (t.isJSXElement(fragChild)) {
          children.push(parseJSXElement(fragChild, state));
        }
      }
    }
  }

  if (children.length > 0) {
    result.children = children;
  }

  return result;
}

function parseComponentFunction(
  node: t.FunctionDeclaration | t.FunctionExpression,
  state: ParsedState,
  interfaceTypes: Map<string, Map<string, { type: string; optional: boolean }>>
) {
  const name = t.isFunctionDeclaration(node) && node.id ? node.id.name : 'Anonymous';
  const def: ComponentDef = {
    name,
    props: [],
    state: [],
    template: null
  };

  // Try to find interface name from type annotation
  let propsInterfaceName: string | null = null;
  if (node.params.length > 0) {
    const param = node.params[0];
    if (param.typeAnnotation && t.isTSTypeAnnotation(param.typeAnnotation)) {
      const typeAnn = param.typeAnnotation.typeAnnotation;
      if (t.isTSTypeReference(typeAnn) && t.isIdentifier(typeAnn.typeName)) {
        propsInterfaceName = typeAnn.typeName.name;
      }
    }
  }

  // Get interface props if available
  const interfaceProps = propsInterfaceName ? interfaceTypes.get(propsInterfaceName) : null;

  // Parse parameters (destructured props)
  if (node.params.length > 0) {
    const param = node.params[0];
    if (t.isObjectPattern(param)) {
      for (const prop of param.properties) {
        if (t.isObjectProperty(prop) && t.isIdentifier(prop.key)) {
          const propName = prop.key.name;
          const propDef: { name: string; type: string; default?: string } = {
            name: propName,
            type: 'any'
          };

          // Get type from interface if available
          if (interfaceProps && interfaceProps.has(propName)) {
            propDef.type = interfaceProps.get(propName)!.type;
          }

          // Check for default value (preserve native types)
          if (t.isAssignmentPattern(prop.value)) {
            const right = prop.value.right;
            if (t.isNumericLiteral(right)) {
              propDef.default = right.value;  // Keep as number
              if (propDef.type === 'any') propDef.type = 'int';
            } else if (t.isStringLiteral(right)) {
              propDef.default = right.value;  // Keep as string
              if (propDef.type === 'any') propDef.type = 'string';
            } else if (t.isBooleanLiteral(right)) {
              propDef.default = right.value;  // Keep as boolean
              if (propDef.type === 'any') propDef.type = 'bool';
            }
          }

          def.props.push(propDef);
        }
      }
    }
  }

  // Parse body for useState calls and return statement
  const body = node.body;
  if (t.isBlockStatement(body)) {
    for (const stmt of body.body) {
      // Look for useState calls
      if (t.isVariableDeclaration(stmt)) {
        for (const decl of stmt.declarations) {
          if (t.isArrayPattern(decl.id) && t.isCallExpression(decl.init)) {
            const call = decl.init;
            if (t.isIdentifier(call.callee) && call.callee.name === 'useState') {
              const elements = decl.id.elements;
              if (elements.length >= 1 && t.isIdentifier(elements[0])) {
                const varName = elements[0].name;
                const arg = call.arguments[0];
                let initial = '0';

                if (t.isNumericLiteral(arg)) {
                  initial = String(arg.value);
                } else if (t.isIdentifier(arg)) {
                  initial = `{"var":"${arg.name}"}`;
                }

                def.state.push({
                  name: varName,
                  type: 'int',
                  initial
                });

                // Add to reactive vars
                state.reactiveVars.push({
                  id: state.reactiveVars.length + 1,
                  name: `${name}:${varName}`,
                  type: 'int',
                  initial_value: initial,
                  scope: 'component'
                });
              }
            }
          }
        }
      }

      // Look for return statement with JSX
      if (t.isReturnStatement(stmt) && stmt.argument) {
        if (t.isJSXElement(stmt.argument)) {
          def.template = parseJSXElement(stmt.argument, state);
          if (def.template) {
            def.template.component_ref = name;
          }
        } else if (t.isParenthesizedExpression(stmt.argument)) {
          const inner = stmt.argument.expression;
          if (t.isJSXElement(inner)) {
            def.template = parseJSXElement(inner, state);
            if (def.template) {
              def.template.component_ref = name;
            }
          }
        }
      }
    }
  }

  state.componentDefs.set(name, def);
}

function parseKryonApp(node: t.CallExpression, state: ParsedState) {
  const arg = node.arguments[0];
  if (!t.isObjectExpression(arg)) return;

  for (const prop of arg.properties) {
    if (!t.isObjectProperty(prop) || !t.isIdentifier(prop.key)) continue;

    const name = prop.key.name;
    const value = prop.value;

    switch (name) {
      case 'width':
        if (t.isNumericLiteral(value)) {
          state.rootConfig.width = value.value;
        }
        break;
      case 'height':
        if (t.isNumericLiteral(value)) {
          state.rootConfig.height = value.value;
        }
        break;
      case 'title':
        if (t.isStringLiteral(value)) {
          state.rootConfig.title = value.value;
        }
        break;
      case 'background':
        if (t.isStringLiteral(value)) {
          state.rootConfig.background = value.value;
        }
        break;
      case 'render':
        if (t.isArrowFunctionExpression(value) || t.isFunctionExpression(value)) {
          const body = value.body;
          if (t.isJSXElement(body)) {
            state.root = parseJSXElement(body, state);
          } else if (t.isParenthesizedExpression(body)) {
            const inner = body.expression;
            if (t.isJSXElement(inner)) {
              state.root = parseJSXElement(inner, state);
            }
          }
        }
        break;
    }
  }
}

async function main() {
  const args = process.argv.slice(2);
  if (args.length < 1) {
    console.error('Usage: bun parse_tsx.ts <input.tsx> [output.kir]');
    process.exit(1);
  }

  const inputFile = args[0];
  const outputFile = args[1] || inputFile.replace(/\.tsx?$/, '.kir');

  const source = await fs.promises.readFile(inputFile, 'utf-8');

  // Parse with babel
  const ast = parser.parse(source, {
    sourceType: 'module',
    plugins: ['jsx', 'typescript']
  });

  const state: ParsedState = {
    componentDefs: new Map(),
    rootConfig: {
      width: 800,
      height: 600,
      title: 'Kryon App',
      background: '#1E1E1E'
    },
    root: null,
    reactiveVars: [],
    logic: {
      functions: {},
      event_bindings: []
    }
  };

  // First pass: Extract TypeScript interfaces for prop types
  const interfaceTypes = new Map<string, Map<string, { type: string; optional: boolean }>>();
  for (const node of ast.program.body) {
    if (t.isTSInterfaceDeclaration(node)) {
      const name = node.id.name;
      const props = new Map<string, { type: string; optional: boolean }>();
      for (const member of node.body.body) {
        if (t.isTSPropertySignature(member) && t.isIdentifier(member.key)) {
          const propName = member.key.name;
          const optional = member.optional || false;
          let type = 'any';
          if (member.typeAnnotation && t.isTSTypeAnnotation(member.typeAnnotation)) {
            const tsType = member.typeAnnotation.typeAnnotation;
            if (t.isTSNumberKeyword(tsType)) type = 'int';
            else if (t.isTSStringKeyword(tsType)) type = 'string';
            else if (t.isTSBooleanKeyword(tsType)) type = 'bool';
          }
          props.set(propName, { type, optional });
        }
      }
      interfaceTypes.set(name, props);
    }
  }

  // Second pass: Parse component functions and apply interface types
  for (const node of ast.program.body) {
    // Find function declarations (component definitions)
    if (t.isFunctionDeclaration(node) && node.id) {
      parseComponentFunction(node, state, interfaceTypes);
    }

    // Find export default kryonApp(...)
    if (t.isExportDefaultDeclaration(node)) {
      const decl = node.declaration;
      if (t.isCallExpression(decl)) {
        const callee = decl.callee;
        if (t.isIdentifier(callee) && callee.name === 'kryonApp') {
          parseKryonApp(decl, state);
        }
      }
    }
  }

  // Build KIR output
  const kir: any = {
    format_version: '3.0'
  };

  // Add component definitions
  if (state.componentDefs.size > 0) {
    kir.component_definitions = [];
    for (const [name, def] of state.componentDefs) {
      kir.component_definitions.push({
        name: def.name,
        props: def.props,
        state: def.state,
        template: def.template
      });
    }
  }

  // Add root
  if (state.root) {
    kir.root = {
      id: nextId++,
      type: 'Column',
      width: `${state.rootConfig.width}.0px`,
      height: `${state.rootConfig.height}.0px`,
      background: state.rootConfig.background,
      windowTitle: state.rootConfig.title,
      children: [state.root]
    };
  }

  // Add logic
  if (Object.keys(state.logic.functions).length > 0 || state.logic.event_bindings.length > 0) {
    kir.logic = state.logic;
  }

  // Add reactive manifest
  if (state.reactiveVars.length > 0) {
    kir.reactive_manifest = {
      variables: state.reactiveVars
    };
  }

  // Output
  const output = JSON.stringify(kir, null, 2);

  if (outputFile === '-') {
    console.log(output);
  } else {
    await fs.promises.writeFile(outputFile, output);
    console.log(`Generated: ${outputFile}`);
  }
}

main().catch(err => {
  console.error('Error:', err.message);
  process.exit(1);
});
