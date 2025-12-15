// Kryon Renderer - Converts IRNode tree to C IR components

import { ffi, cstr } from './ffi.ts';
import { IRNode, ComponentType, DimensionType, Alignment, EventType, Pointer, Size } from './types.ts';

// Map JSX type names to C component types
const componentTypeMap: Record<string, ComponentType> = {
  container: ComponentType.Container,
  text: ComponentType.Text,
  button: ComponentType.Button,
  input: ComponentType.Input,
  checkbox: ComponentType.Checkbox,
  dropdown: ComponentType.Dropdown,
  row: ComponentType.Row,
  column: ComponentType.Column,
  center: ComponentType.Center,
  image: ComponentType.Image,
  canvas: ComponentType.Canvas,
  markdown: ComponentType.Markdown,
};

// Parse size value to dimension type and float value
function parseSize(size: Size): { type: DimensionType; value: number } {
  if (size === 'auto') {
    return { type: DimensionType.Auto, value: 0 };
  }
  if (typeof size === 'number') {
    return { type: DimensionType.Px, value: size };
  }
  if (typeof size === 'string') {
    if (size.endsWith('%')) {
      return { type: DimensionType.Percent, value: parseFloat(size) };
    }
    if (size.endsWith('px')) {
      return { type: DimensionType.Px, value: parseFloat(size) };
    }
  }
  return { type: DimensionType.Auto, value: 0 };
}

// Parse color string (hex or named) to RGBA
function parseColor(color: string): { r: number; g: number; b: number; a: number } {
  if (color.startsWith('#')) {
    const hex = color.slice(1);
    if (hex.length === 6) {
      return {
        r: parseInt(hex.slice(0, 2), 16),
        g: parseInt(hex.slice(2, 4), 16),
        b: parseInt(hex.slice(4, 6), 16),
        a: 255,
      };
    }
    if (hex.length === 8) {
      return {
        r: parseInt(hex.slice(0, 2), 16),
        g: parseInt(hex.slice(2, 4), 16),
        b: parseInt(hex.slice(4, 6), 16),
        a: parseInt(hex.slice(6, 8), 16),
      };
    }
  }
  // Named colors (basic support)
  const namedColors: Record<string, { r: number; g: number; b: number }> = {
    white: { r: 255, g: 255, b: 255 },
    black: { r: 0, g: 0, b: 0 },
    red: { r: 255, g: 0, b: 0 },
    green: { r: 0, g: 255, b: 0 },
    blue: { r: 0, g: 0, b: 255 },
    yellow: { r: 255, g: 255, b: 0 },
    cyan: { r: 0, g: 255, b: 255 },
    magenta: { r: 255, g: 0, b: 255 },
    gray: { r: 128, g: 128, b: 128 },
    grey: { r: 128, g: 128, b: 128 },
  };
  const named = namedColors[color.toLowerCase()];
  if (named) {
    return { ...named, a: 255 };
  }
  return { r: 0, g: 0, b: 0, a: 255 };
}

// Parse alignment string to enum
function parseAlignment(align: string): Alignment {
  const alignMap: Record<string, Alignment> = {
    start: Alignment.Start,
    center: Alignment.Center,
    end: Alignment.End,
    'space-between': Alignment.SpaceBetween,
    'space-around': Alignment.SpaceAround,
    'space-evenly': Alignment.SpaceEvenly,
  };
  return alignMap[align] ?? Alignment.Start;
}

// Event handler storage (maps component ID to handler)
const eventHandlers = new Map<number, Map<EventType, Function>>();
let nextHandlerId = 1;

// Register an event handler for a component
function registerHandler(componentId: number, eventType: EventType, handler: Function): void {
  if (!eventHandlers.has(componentId)) {
    eventHandlers.set(componentId, new Map());
  }
  eventHandlers.get(componentId)!.set(eventType, handler);
}

// Get handler for a component
export function getHandler(componentId: number, eventType: EventType): Function | undefined {
  return eventHandlers.get(componentId)?.get(eventType);
}

// Render an IRNode tree to C IR components
export function renderNode(node: IRNode | string, parentPtr?: Pointer): Pointer {
  // Handle text nodes (strings) - wrap in Text component
  if (typeof node === 'string') {
    const textPtr = ffi.ir_text(cstr(node)) as unknown as Pointer;
    return textPtr;
  }

  // Handle fragments - render children directly
  if (node.type === 'fragment') {
    // Fragments don't create a component, children are added to parent directly
    // Return 0 to indicate no component created
    return 0;
  }

  // Create the component
  const componentType = componentTypeMap[node.type];
  if (componentType === undefined) {
    console.error(`Unknown component type: ${node.type}`);
    return 0;
  }

  let componentPtr: Pointer;

  // Use convenience creators where available
  switch (node.type) {
    case 'container':
      componentPtr = ffi.ir_container(cstr(node.props.tag || 'div')) as unknown as Pointer;
      break;
    case 'text':
      componentPtr = ffi.ir_text(cstr(node.props.content || '')) as unknown as Pointer;
      break;
    case 'button':
      componentPtr = ffi.ir_button(cstr(node.props.title || '')) as unknown as Pointer;
      break;
    case 'input':
      componentPtr = ffi.ir_input(cstr(node.props.placeholder || '')) as unknown as Pointer;
      break;
    case 'row':
      componentPtr = ffi.ir_row() as unknown as Pointer;
      break;
    case 'column':
      componentPtr = ffi.ir_column() as unknown as Pointer;
      break;
    case 'center':
      componentPtr = ffi.ir_center() as unknown as Pointer;
      break;
    case 'image':
      componentPtr = ffi.ir_create_component(componentType) as unknown as Pointer;
      if (node.props.src) {
        ffi.ir_set_custom_data(componentPtr, cstr(node.props.src));
      }
      if (node.props.alt) {
        ffi.ir_set_text_content(componentPtr, cstr(node.props.alt));
      }
      break;
    default:
      componentPtr = ffi.ir_create_component(componentType) as unknown as Pointer;
      break;
  }

  if (!componentPtr) {
    console.error(`Failed to create component: ${node.type}`);
    return 0;
  }

  // Apply styles
  const stylePtr = ffi.ir_create_style() as unknown as Pointer;
  let hasStyle = false;

  // Width/Height
  if (node.props.width !== undefined) {
    const dim = parseSize(node.props.width);
    ffi.ir_set_width(stylePtr, dim.type, dim.value);
    hasStyle = true;
  }
  if (node.props.height !== undefined) {
    const dim = parseSize(node.props.height);
    ffi.ir_set_height(stylePtr, dim.type, dim.value);
    hasStyle = true;
  }

  // Background color
  if (node.props.backgroundColor) {
    const color = parseColor(node.props.backgroundColor);
    ffi.ir_set_background_color(stylePtr, color.r, color.g, color.b, color.a);
    hasStyle = true;
  }

  // Padding
  if (node.props.padding !== undefined) {
    const p = node.props.padding;
    if (typeof p === 'number') {
      ffi.ir_set_padding(stylePtr, p, p, p, p);
    } else if (Array.isArray(p)) {
      if (p.length === 2) {
        ffi.ir_set_padding(stylePtr, p[0], p[1], p[0], p[1]);
      } else if (p.length === 4) {
        ffi.ir_set_padding(stylePtr, p[0], p[1], p[2], p[3]);
      }
    }
    hasStyle = true;
  }

  // Margin
  if (node.props.margin !== undefined) {
    const m = node.props.margin;
    if (typeof m === 'number') {
      ffi.ir_set_margin(stylePtr, m, m, m, m);
    } else if (Array.isArray(m)) {
      if (m.length === 2) {
        ffi.ir_set_margin(stylePtr, m[0], m[1], m[0], m[1]);
      } else if (m.length === 4) {
        ffi.ir_set_margin(stylePtr, m[0], m[1], m[2], m[3]);
      }
    }
    hasStyle = true;
  }

  // Opacity
  if (node.props.opacity !== undefined) {
    ffi.ir_set_opacity(stylePtr, node.props.opacity);
    hasStyle = true;
  }

  // Z-Index
  if (node.props.zIndex !== undefined) {
    ffi.ir_set_z_index(stylePtr, node.props.zIndex);
    hasStyle = true;
  }

  // Apply style to component if any style was set
  if (hasStyle) {
    ffi.ir_set_style(componentPtr, stylePtr);
  } else {
    ffi.ir_destroy_style(stylePtr);
  }

  // Apply layout for container-type components
  if (['row', 'column', 'container', 'center'].includes(node.type)) {
    const layoutPtr = ffi.ir_create_layout() as unknown as Pointer;
    let hasLayout = false;

    // Gap
    if (node.props.gap !== undefined) {
      const direction = node.type === 'row' ? 0 : 1; // 0 = row, 1 = column
      ffi.ir_set_flexbox(layoutPtr, node.props.wrap ?? false, node.props.gap, 0, 0);
      hasLayout = true;
    }

    // Justify content
    if (node.props.justify !== undefined) {
      const align = parseAlignment(node.props.justify);
      ffi.ir_set_justify_content(layoutPtr, align);
      hasLayout = true;
    }

    // Align items
    if (node.props.align !== undefined) {
      const align = parseAlignment(node.props.align);
      ffi.ir_set_align_items(layoutPtr, align);
      hasLayout = true;
    }

    if (hasLayout) {
      ffi.ir_set_layout(componentPtr, layoutPtr);
    } else {
      ffi.ir_destroy_layout(layoutPtr);
    }
  }

  // Register event handlers
  if (node.props.onClick) {
    const handlerId = nextHandlerId++;
    registerHandler(handlerId, EventType.Click, node.props.onClick);
    const eventPtr = ffi.ir_create_event(EventType.Click, cstr(`handler_${handlerId}`), cstr('')) as unknown as Pointer;
    ffi.ir_add_event(componentPtr, eventPtr);
  }

  // Render children
  for (const child of node.children) {
    if (typeof child === 'string') {
      // For container types, wrap string children in Text
      if (['row', 'column', 'container', 'center'].includes(node.type)) {
        const textPtr = ffi.ir_text(cstr(child)) as unknown as Pointer;
        ffi.ir_add_child(componentPtr, textPtr);
      }
    } else if (child.type === 'fragment') {
      // Fragment children are added directly
      for (const fragmentChild of child.children) {
        const childPtr = renderNode(fragmentChild);
        if (childPtr) {
          ffi.ir_add_child(componentPtr, childPtr);
        }
      }
    } else {
      const childPtr = renderNode(child);
      if (childPtr) {
        ffi.ir_add_child(componentPtr, childPtr);
      }
    }
  }

  return componentPtr;
}

// Render JSX tree and return root component pointer
export function render(jsx: IRNode): Pointer {
  return renderNode(jsx);
}
