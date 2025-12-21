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

// Const map for resolving variable references like colors.accent
const constMap = new Map<string, any>();

// Map lowercase JSX component names to PascalCase type names (for JSON)
function toPascalCase(name: string): string {
  const map: Record<string, string> = {
    'container': 'Container',
    'text': 'Text',
    'button': 'Button',
    'input': 'Input',
    'checkbox': 'Checkbox',
    'dropdown': 'Dropdown',
    'row': 'Row',
    'column': 'Column',
    'center': 'Center',
    'image': 'Image',
    'canvas': 'Canvas',
    'markdown': 'Markdown',
    'sprite': 'Sprite',
    'tabgroup': 'TabGroup',
    'tabbar': 'TabBar',
    'tab': 'Tab',
    'tabcontent': 'TabContent',
    'tabpanel': 'TabPanel',
    'table': 'Table',
    'tablehead': 'TableHead',
    'tablebody': 'TableBody',
    'tablefoot': 'TableFoot',
    'tablerow': 'TableRow',
    'tablecell': 'TableCell',
    'tableheadercell': 'TableHeaderCell',
    'flowchart': 'Flowchart',
    'flowchartnode': 'FlowchartNode',
    'flowchartedge': 'FlowchartEdge',
    'flowchartsubgraph': 'FlowchartSubgraph',
    'flowchartlabel': 'FlowchartLabel',
  };

  return map[name.toLowerCase()] || name;
}

/**
 * Evaluate a simple expression to extract its value
 * Handles: literals, objects, arrays, member access
 */
function evaluateExpression(node: t.Node): any {
  if (t.isStringLiteral(node)) {
    return node.value;
  } else if (t.isNumericLiteral(node)) {
    return node.value;
  } else if (t.isBooleanLiteral(node)) {
    return node.value;
  } else if (t.isObjectExpression(node)) {
    const obj: { [key: string]: any } = {};
    for (const prop of node.properties) {
      if (t.isObjectProperty(prop)) {
        let key: string | null = null;
        if (t.isIdentifier(prop.key)) {
          key = prop.key.name;
        } else if (t.isStringLiteral(prop.key)) {
          key = prop.key.value;
        }
        if (key) {
          obj[key] = evaluateExpression(prop.value);
        }
      }
    }
    return obj;
  } else if (t.isArrayExpression(node)) {
    return node.elements.map(el => el ? evaluateExpression(el) : null);
  } else if (t.isIdentifier(node)) {
    // Reference to another const
    if (constMap.has(node.name)) {
      return constMap.get(node.name);
    }
    return null;
  } else if (t.isMemberExpression(node)) {
    const objValue = evaluateExpression(node.object);
    if (objValue && typeof objValue === 'object') {
      if (t.isIdentifier(node.property)) {
        return objValue[node.property.name];
      } else if (t.isStringLiteral(node.property)) {
        return objValue[node.property.value];
      }
    }
    return null;
  } else if (t.isTemplateLiteral(node)) {
    let result = '';
    let hasVarRef = false;
    for (let i = 0; i < node.quasis.length; i++) {
      result += node.quasis[i].value.cooked || '';
      if (i < node.expressions.length) {
        const expr = node.expressions[i];
        const exprValue = evaluateExpression(expr);
        if (exprValue !== null && exprValue !== undefined) {
          result += String(exprValue);
        } else {
          // Handle variable references and method calls in templates
          // e.g., ${twitter} or ${twitter.replace('@', '')}
          let varName: string | null = null;
          let transform: string | null = null;

          if (t.isIdentifier(expr)) {
            varName = expr.name;
          } else if (t.isCallExpression(expr) && t.isMemberExpression(expr.callee)) {
            // Handle expr.replace(a, b) pattern
            const callee = expr.callee;
            if (t.isIdentifier(callee.object)) {
              varName = callee.object.name;
              if (t.isIdentifier(callee.property) && callee.property.name === 'replace') {
                // Extract what to replace
                if (expr.arguments.length >= 2 &&
                    t.isStringLiteral(expr.arguments[0]) &&
                    t.isStringLiteral(expr.arguments[1])) {
                  const from = expr.arguments[0].value;
                  const to = expr.arguments[1].value;
                  transform = `replace:${from}:${to}`;
                }
              }
            }
          }

          if (varName) {
            hasVarRef = true;
            if (transform) {
              result += `{{${varName}|${transform}}}`;
            } else {
              result += `{{${varName}}}`;
            }
          }
        }
      }
    }
    // If we have variable references, return a template object
    if (hasVarRef) {
      return { template: result };
    }
    return result;
  } else if (t.isBinaryExpression(node)) {
    // Handle arithmetic expressions like spacing.xxxl * 2
    const left = evaluateExpression(node.left);
    const right = evaluateExpression(node.right);
    if (typeof left === 'number' && typeof right === 'number') {
      switch (node.operator) {
        case '+': return left + right;
        case '-': return left - right;
        case '*': return left * right;
        case '/': return left / right;
        case '%': return left % right;
      }
    }
    // Also handle string concatenation
    if (node.operator === '+' && (typeof left === 'string' || typeof right === 'string')) {
      return String(left ?? '') + String(right ?? '');
    }
    return null;
  }
  return null;
}

/**
 * First pass: collect all top-level const declarations
 */
function collectConstants(ast: any) {
  for (const node of ast.program.body) {
    if (t.isVariableDeclaration(node) && node.kind === 'const') {
      for (const decl of node.declarations) {
        if (t.isIdentifier(decl.id) && decl.init) {
          const value = evaluateExpression(decl.init);
          if (value !== null) {
            constMap.set(decl.id.name, value);
          }
        }
      }
    }
  }
}

// Parsed state
interface ParsedState {
  componentDefs: Map<string, ComponentDef>;
  rootConfig: RootConfig;
  root: KirNode | null;
  reactiveVars: ReactiveVar[];
  logic: LogicBlock;
  expansionStack: string[];  // Track components being expanded (for recursion detection)
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
    // First check if this is a known const
    if (constMap.has(node.name)) {
      return constMap.get(node.name);
    }
    // Otherwise treat as a variable reference (for props)
    return { var: node.name };
  } else if (t.isMemberExpression(node)) {
    // Handle colors.accent, spacing.lg, etc.
    const resolved = evaluateExpression(node);
    if (resolved !== null && resolved !== undefined) {
      return resolved;
    }
    // Fallback to variable reference if can't resolve
    if (t.isIdentifier(node.object)) {
      return { var: node.object.name };
    }
    return null;
  } else if (t.isTemplateLiteral(node)) {
    // Handle `1px solid ${colors.border}`
    const resolved = evaluateExpression(node);
    if (resolved !== null && resolved !== undefined) {
      return resolved;
    }
    return null;
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
  } else if (t.isBinaryExpression(node)) {
    // Handle arithmetic expressions like spacing.xxxl * 2
    const left = evaluateExpression(node.left);
    const right = evaluateExpression(node.right);
    if (typeof left === 'number' && typeof right === 'number') {
      switch (node.operator) {
        case '+': return left + right;
        case '-': return left - right;
        case '*': return left * right;
        case '/': return left / right;
      }
    }
    return null;
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

/**
 * Deep clone a KIR node and all its descendants
 * Similar to C: clone_and_substitute_json_impl
 */
function deepCloneKirNode(node: KirNode): KirNode {
  const cloned: KirNode = {
    id: node.id, // Will be remapped later
    type: node.type,
  };

  // Copy all properties except children
  for (const [key, value] of Object.entries(node)) {
    if (key === 'children') continue;
    if (key === 'component_ref') continue; // Don't copy component_ref

    if (value === null || value === undefined) {
      continue;
    } else if (Array.isArray(value)) {
      cloned[key] = value.map(v =>
        typeof v === 'object' && v !== null && 'type' in v
          ? deepCloneKirNode(v)
          : v
      );
    } else if (typeof value === 'object' && value !== null && 'type' in value) {
      cloned[key] = deepCloneKirNode(value);
    } else {
      cloned[key] = value;
    }
  }

  // Recursively clone children
  if (node.children && Array.isArray(node.children)) {
    cloned.children = node.children.map(child => deepCloneKirNode(child));
  }

  return cloned;
}

/**
 * Remap all component IDs recursively to ensure uniqueness
 * Similar to C: remap_ids_recursive
 */
function remapIdsRecursive(node: KirNode): void {
  node.id = nextId++;

  if (node.children && Array.isArray(node.children)) {
    for (const child of node.children) {
      remapIdsRecursive(child);
    }
  }
}

/**
 * Apply props to template by substituting {var: "propName"} with actual values
 * Similar to C: build_state_from_def + clone_and_substitute_json
 */
function applyPropsToTemplate(
  template: KirNode,
  props: Record<string, any>,
  componentDef: ComponentDef
): void {
  // Build prop context (merge instance props with defaults)
  const propContext: Record<string, any> = {};

  for (const propDef of componentDef.props) {
    const propName = propDef.name;
    if (props[propName] !== undefined) {
      propContext[propName] = props[propName];
    } else if (propDef.default !== undefined) {
      propContext[propName] = propDef.default;
    }
  }

  // Recursively substitute {var: "name"} with actual values
  function substituteInNode(node: KirNode): void {
    for (const [key, value] of Object.entries(node)) {
      if (key === 'children') continue;

      // Handle {var: "propName"} objects
      if (typeof value === 'object' && value !== null && 'var' in value) {
        const varName = value.var;
        if (propContext[varName] !== undefined) {
          node[key] = propContext[varName];
        }
      }
      // Handle arrays that might contain {var: ...}
      else if (Array.isArray(value)) {
        node[key] = value.map(v =>
          typeof v === 'object' && v !== null && 'var' in v && propContext[v.var] !== undefined
            ? propContext[v.var]
            : v
        );
      }
    }

    // Recurse into children
    if (node.children && Array.isArray(node.children)) {
      for (const child of node.children) {
        substituteInNode(child);
      }
    }
  }

  substituteInNode(template);
}

/**
 * Inline a custom component by expanding its template with props
 * Returns the expanded node tree, or null if component not found
 */
function inlineComponent(
  componentName: string,
  props: Record<string, any>,
  children: KirNode[] | undefined,
  instanceId: number,
  state: ParsedState
): KirNode | null {
  const componentDef = state.componentDefs.get(componentName);
  if (!componentDef || !componentDef.template) {
    return null; // Not a custom component
  }

  // Detect recursion
  if (state.expansionStack.includes(componentName)) {
    console.warn(`Warning: Recursive component detected: ${componentName}`);
    return null;
  }

  if (state.expansionStack.length > 100) {
    console.error(`Error: Component expansion depth limit exceeded`);
    return null;
  }

  state.expansionStack.push(componentName);

  // Clone the template (deep copy)
  const expanded = deepCloneKirNode(componentDef.template);

  // Apply props to template (substitute {var: "propName"})
  applyPropsToTemplate(expanded, props, componentDef);

  // Handle children prop if provided
  if (children && children.length > 0) {
    // Check if template has a placeholder for children
    // For now, append to template's children array
    if (!expanded.children) {
      expanded.children = [];
    }
    expanded.children.push(...children);
  }

  // Remap all IDs to ensure uniqueness
  remapIdsRecursive(expanded);

  // Use the instance ID for the root element
  expanded.id = instanceId;

  // Remove component_ref if present (not needed in expanded form)
  delete expanded.component_ref;

  state.expansionStack.pop();

  return expanded;
}

function parseJSXElement(node: t.JSXElement, state: ParsedState): KirNode {
  const id = nextId++;
  const result: KirNode = { id, type: '' };

  // Get element name
  const openingElement = node.openingElement;
  let elementName = '';
  if (t.isJSXIdentifier(openingElement.name)) {
    elementName = openingElement.name.name;
    result.type = toPascalCase(elementName);

    // Check if this is a custom component (not a primitive)
    const isPrimitive = elementName.toLowerCase() in {
      container: 1, text: 1, button: 1, input: 1, checkbox: 1, dropdown: 1,
      row: 1, column: 1, center: 1, image: 1, canvas: 1, markdown: 1,
      sprite: 1, tabgroup: 1, tabbar: 1, tab: 1, tabcontent: 1, tabpanel: 1,
      table: 1, tablehead: 1, tablebody: 1, tablefoot: 1, tablerow: 1,
      tablecell: 1, tableheadercell: 1, flowchart: 1, flowchartnode: 1,
      flowchartedge: 1, flowchartsubgraph: 1, flowchartlabel: 1
    };

    if (!isPrimitive && state.componentDefs.has(result.type)) {
      result._needsExpansion = true;  // Mark for later expansion
    }
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
          // Keep title as-is for component props; app title is handled in parseKryonApp
          result.title = value;
          break;
        case 'text':
          if (typeof value === 'string' && value.startsWith('{{')) {
            result.text_expression = value;
            result.text = value;
          } else {
            result.text = value;
          }
          break;
        case 'href':
          result.href = value;
          break;
        case 'target':
          result.target = value;
          break;
        case 'source':
          // Markdown source content
          result.source = value;
          break;
        case 'theme':
          // Theme for Markdown component
          result.theme = value;
          break;
        case 'file':
          // Markdown file path (relative to project root)
          result.file = value;
          break;
        // Flowchart properties
        case 'direction':
          // Flowchart direction (TB, BT, LR, RL)
          if (result.type === 'Flowchart') {
            result.flowchart_config = result.flowchart_config || {};
            result.flowchart_config.direction = value;
          }
          break;
        case 'id':
          // FlowchartNode or FlowchartSubgraph id
          if (result.type === 'FlowchartNode') {
            result.node_data = result.node_data || {};
            result.node_data.id = value;
          } else if (result.type === 'FlowchartSubgraph') {
            result.subgraph_data = result.subgraph_data || {};
            result.subgraph_data.id = value;
          }
          break;
        case 'shape':
          // FlowchartNode shape
          if (result.type === 'FlowchartNode') {
            result.node_data = result.node_data || {};
            result.node_data.shape = value;
          }
          break;
        case 'label':
          // FlowchartNode or FlowchartEdge label
          if (result.type === 'FlowchartNode') {
            result.node_data = result.node_data || {};
            result.node_data.label = value;
          } else if (result.type === 'FlowchartEdge') {
            result.edge_data = result.edge_data || {};
            result.edge_data.label = value;
          }
          break;
        case 'from':
          // FlowchartEdge from node
          if (result.type === 'FlowchartEdge') {
            result.edge_data = result.edge_data || {};
            result.edge_data.from = value;
          }
          break;
        case 'to':
          // FlowchartEdge to node
          if (result.type === 'FlowchartEdge') {
            result.edge_data = result.edge_data || {};
            result.edge_data.to = value;
          }
          break;
        case 'type':
          // FlowchartEdge type (arrow, dotted, open, thick, bidirectional)
          if (result.type === 'FlowchartEdge') {
            result.edge_data = result.edge_data || {};
            result.edge_data.type = value;
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
    } else if (t.isJSXExpressionContainer(child)) {
      // Handle conditional rendering: {condition && <Element />}
      const expr = child.expression;
      if (t.isLogicalExpression(expr) && expr.operator === '&&') {
        // {variable && <Element />} - conditional rendering
        if (t.isJSXElement(expr.right)) {
          const childNode = parseJSXElement(expr.right, state);
          // Add condition for optional rendering
          if (t.isIdentifier(expr.left)) {
            childNode.when = { var: expr.left.name };
          }
          children.push(childNode);
        }
      } else if (t.isJSXElement(expr)) {
        // {<Element />} - direct element in expression
        children.push(parseJSXElement(expr, state));
      }
    }
  }

  if (children.length > 0) {
    result.children = children;
  }

  // If this is a custom component, inline it now
  if (result._needsExpansion) {
    delete result._needsExpansion;

    // Extract props (everything except id, type, children)
    const { id, type, children, _needsExpansion, ...props } = result as any;

    // Inline the component
    const expanded = inlineComponent(type, props, children, id, state);

    if (expanded) {
      return expanded;  // Return inlined tree instead
    } else {
      // Component definition not found - fall back to reference
      console.warn(`Warning: Component "${type}" referenced but not defined`);
    }
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
        const bgValue = evaluateExpression(value);
        if (bgValue !== null && typeof bgValue === 'string') {
          state.rootConfig.background = bgValue;
        } else if (t.isStringLiteral(value)) {
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

  // Clear and collect const declarations first
  constMap.clear();
  collectConstants(ast);

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
    },
    expansionStack: []
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
