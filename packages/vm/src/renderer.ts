/**
 * DOM Renderer for Kryon VM
 * Maps VM UI operations to DOM manipulation
 */

import { PropertyID } from './opcodes';
import { KRBValue, KRBModule } from './loader';
import { KryonVM, RendererInterface, ComponentRef } from './vm';

// ============================================================================
// Types
// ============================================================================

export interface ComponentNode {
  id: number;
  type: string;
  element: HTMLElement;
  children: ComponentNode[];
  parent: ComponentNode | null;
  props: Map<string, any>;
}

export interface RenderOptions {
  debug?: boolean;
}

// ============================================================================
// DOM Renderer
// ============================================================================

export class DOMRenderer implements RendererInterface {
  private container: HTMLElement;
  private components: Map<number, ComponentNode> = new Map();
  private root: ComponentNode | null = null;
  private debug: boolean;
  private needsRedraw: boolean = false;

  constructor(container: HTMLElement, options: RenderOptions = {}) {
    this.container = container;
    this.debug = options.debug ?? false;
  }

  /**
   * Initialize the renderer with a KRB module's UI data
   */
  initFromModule(module: KRBModule): void {
    if (!module.uiData) {
      console.warn('No UI data in module');
      return;
    }

    // Clear existing content
    this.container.innerHTML = '';
    this.components.clear();

    // Parse UI data and create DOM
    this.root = this.parseUIData(module.uiData);
    if (this.root) {
      this.container.appendChild(this.root.element);
    }
  }

  /**
   * Parse binary UI data into component tree
   * This is a simplified parser - real implementation would parse KIRB format
   */
  private parseUIData(data: Uint8Array): ComponentNode | null {
    // For now, create a simple root container
    // Full implementation would parse the binary UI tree from the module
    const root = this.createComponent(1, 'container');
    root.element.style.width = '100%';
    root.element.style.height = '100%';
    return root;
  }

  /**
   * Create a component node
   */
  private createComponent(id: number, type: string): ComponentNode {
    const element = this.createDOMElement(type);
    element.dataset.kryonId = String(id);
    element.dataset.kryonType = type;

    const node: ComponentNode = {
      id,
      type,
      element,
      children: [],
      parent: null,
      props: new Map(),
    };

    this.components.set(id, node);

    if (this.debug) {
      console.log(`Created component: ${type} #${id}`);
    }

    return node;
  }

  /**
   * Create appropriate DOM element for component type
   */
  private createDOMElement(type: string): HTMLElement {
    switch (type.toLowerCase()) {
      case 'button':
        const btn = document.createElement('button');
        btn.style.cursor = 'pointer';
        return btn;

      case 'text':
        return document.createElement('span');

      case 'input':
        const input = document.createElement('input');
        input.type = 'text';
        return input;

      case 'checkbox': {
        const label = document.createElement('label');
        const cb = document.createElement('input');
        cb.type = 'checkbox';
        label.appendChild(cb);
        return label;
      }

      case 'image':
        return document.createElement('img');

      case 'row':
      case 'column':
      case 'container':
      default:
        const div = document.createElement('div');
        if (type === 'row') {
          div.style.display = 'flex';
          div.style.flexDirection = 'row';
        } else if (type === 'column') {
          div.style.display = 'flex';
          div.style.flexDirection = 'column';
        }
        return div;
    }
  }

  // ============================================================================
  // RendererInterface implementation
  // ============================================================================

  getComponent(id: number): ComponentRef | null {
    return this.components.has(id) ? id : null;
  }

  setProperty(compRef: ComponentRef, propId: PropertyID, value: KRBValue): void {
    const node = this.components.get(compRef);
    if (!node) return;

    const el = node.element;
    const val = this.extractValue(value);

    switch (propId) {
      case PropertyID.TEXT:
        this.setTextContent(node, String(val));
        break;

      case PropertyID.VISIBLE:
        el.style.display = val ? '' : 'none';
        break;

      case PropertyID.ENABLED:
        if (el instanceof HTMLButtonElement || el instanceof HTMLInputElement) {
          el.disabled = !val;
        }
        break;

      case PropertyID.WIDTH:
        el.style.width = this.toCSSSize(val);
        break;

      case PropertyID.HEIGHT:
        el.style.height = this.toCSSSize(val);
        break;

      case PropertyID.X:
        el.style.left = this.toCSSSize(val);
        if (el.style.position === '') el.style.position = 'absolute';
        break;

      case PropertyID.Y:
        el.style.top = this.toCSSSize(val);
        if (el.style.position === '') el.style.position = 'absolute';
        break;

      case PropertyID.BG_COLOR:
        el.style.backgroundColor = this.toColor(val);
        break;

      case PropertyID.FG_COLOR:
        el.style.color = this.toColor(val);
        break;

      case PropertyID.BORDER_COLOR:
        el.style.borderColor = this.toColor(val);
        break;

      case PropertyID.BORDER_WIDTH:
        el.style.borderWidth = this.toCSSSize(val);
        el.style.borderStyle = 'solid';
        break;

      case PropertyID.BORDER_RADIUS:
        el.style.borderRadius = this.toCSSSize(val);
        break;

      case PropertyID.FONT_SIZE:
        el.style.fontSize = this.toCSSSize(val);
        break;

      case PropertyID.FONT_WEIGHT:
        el.style.fontWeight = String(val);
        break;

      case PropertyID.OPACITY:
        el.style.opacity = String(val);
        break;

      case PropertyID.PADDING:
        el.style.padding = this.toCSSSize(val);
        break;

      case PropertyID.MARGIN:
        el.style.margin = this.toCSSSize(val);
        break;

      case PropertyID.GAP:
        el.style.gap = this.toCSSSize(val);
        break;
    }

    node.props.set(String(propId), val);
    this.needsRedraw = true;
  }

  getProperty(compRef: ComponentRef, propId: PropertyID): KRBValue {
    const node = this.components.get(compRef);
    if (!node) return { type: 'null', value: null };

    const el = node.element;

    switch (propId) {
      case PropertyID.TEXT:
        return { type: 'string', value: el.textContent ?? '' };

      case PropertyID.VISIBLE:
        return { type: 'bool', value: el.style.display !== 'none' };

      case PropertyID.ENABLED:
        if (el instanceof HTMLButtonElement || el instanceof HTMLInputElement) {
          return { type: 'bool', value: !el.disabled };
        }
        return { type: 'bool', value: true };

      case PropertyID.WIDTH:
        return { type: 'int', value: el.offsetWidth };

      case PropertyID.HEIGHT:
        return { type: 'int', value: el.offsetHeight };

      default:
        const stored = node.props.get(String(propId));
        if (stored !== undefined) {
          return this.toKRBValue(stored);
        }
        return { type: 'null', value: null };
    }
  }

  setText(compRef: ComponentRef, text: string): void {
    const node = this.components.get(compRef);
    if (!node) return;

    this.setTextContent(node, text);
    this.needsRedraw = true;
  }

  setVisible(compRef: ComponentRef, visible: boolean): void {
    const node = this.components.get(compRef);
    if (!node) return;

    node.element.style.display = visible ? '' : 'none';
    this.needsRedraw = true;
  }

  addChild(parentRef: ComponentRef, childRef: ComponentRef): void {
    const parent = this.components.get(parentRef);
    const child = this.components.get(childRef);
    if (!parent || !child) return;

    // Remove from old parent
    if (child.parent) {
      const idx = child.parent.children.indexOf(child);
      if (idx >= 0) child.parent.children.splice(idx, 1);
      child.parent.element.removeChild(child.element);
    }

    // Add to new parent
    parent.children.push(child);
    child.parent = parent;
    parent.element.appendChild(child.element);
    this.needsRedraw = true;
  }

  removeChild(parentRef: ComponentRef, childRef: ComponentRef): void {
    const parent = this.components.get(parentRef);
    const child = this.components.get(childRef);
    if (!parent || !child) return;

    const idx = parent.children.indexOf(child);
    if (idx >= 0) {
      parent.children.splice(idx, 1);
      parent.element.removeChild(child.element);
      child.parent = null;
    }
    this.needsRedraw = true;
  }

  redraw(): void {
    this.needsRedraw = false;
    // DOM updates are immediate, so nothing to do here
    // Could be used for batched updates in the future
  }

  // ============================================================================
  // Helper methods
  // ============================================================================

  private setTextContent(node: ComponentNode, text: string): void {
    if (node.type === 'button') {
      node.element.textContent = text;
    } else if (node.type === 'input') {
      (node.element as HTMLInputElement).value = text;
    } else {
      node.element.textContent = text;
    }
  }

  private extractValue(v: KRBValue): any {
    return v.value;
  }

  private toKRBValue(val: any): KRBValue {
    if (val === null || val === undefined) {
      return { type: 'null', value: null };
    }
    if (typeof val === 'boolean') {
      return { type: 'bool', value: val };
    }
    if (typeof val === 'number') {
      return Number.isInteger(val) ? { type: 'int', value: val } : { type: 'float', value: val };
    }
    if (typeof val === 'string') {
      return { type: 'string', value: val };
    }
    if (Array.isArray(val)) {
      return { type: 'array', value: val.map((v) => this.toKRBValue(v)) };
    }
    return { type: 'null', value: null };
  }

  private toCSSSize(val: any): string {
    if (typeof val === 'number') {
      return `${val}px`;
    }
    if (typeof val === 'string') {
      // Already has unit or is percentage
      if (/^\d+(\.\d+)?%$/.test(val) || /^\d+(\.\d+)?(px|em|rem|vh|vw)$/.test(val)) {
        return val;
      }
      // Numeric string
      if (/^\d+(\.\d+)?$/.test(val)) {
        return `${val}px`;
      }
      return val;
    }
    return 'auto';
  }

  private toColor(val: any): string {
    if (typeof val === 'string') {
      return val;
    }
    if (typeof val === 'number') {
      // Assume ARGB or RGB integer
      const hex = val.toString(16).padStart(6, '0');
      return `#${hex}`;
    }
    return 'transparent';
  }

  // ============================================================================
  // Event binding
  // ============================================================================

  /**
   * Wire DOM events to VM event dispatch
   */
  bindEvents(vm: KryonVM): void {
    const module = vm.getModule();
    if (!module) return;

    for (const binding of module.eventBindings) {
      const node = this.components.get(binding.componentId);
      if (!node) continue;

      const eventName = this.eventTypeToName(binding.eventType);
      if (!eventName) continue;

      node.element.addEventListener(eventName, (e) => {
        if (this.debug) {
          console.log(`Event: ${eventName} on #${binding.componentId}`);
        }
        vm.dispatchEvent(binding.componentId, binding.eventType);
      });
    }
  }

  private eventTypeToName(eventType: number): string | null {
    switch (eventType) {
      case 0:
        return 'click';
      case 1:
        return 'change';
      case 2:
        return 'submit';
      case 3:
        return 'input';
      case 4:
        return 'focus';
      case 5:
        return 'blur';
      case 6:
        return 'keydown';
      case 7:
        return 'keyup';
      case 8:
        return 'mouseenter';
      case 9:
        return 'mouseleave';
      default:
        return null;
    }
  }

  // ============================================================================
  // Component creation API (for dynamic UI)
  // ============================================================================

  /**
   * Create a new component dynamically
   */
  createComponentDynamic(id: number, type: string): ComponentRef {
    this.createComponent(id, type);
    return id;
  }

  /**
   * Get the root component
   */
  getRoot(): ComponentRef | null {
    return this.root?.id ?? null;
  }
}

// ============================================================================
// High-level API
// ============================================================================

/**
 * Render a KRB module to a DOM container
 */
export function render(vm: KryonVM, container: HTMLElement, options?: RenderOptions): DOMRenderer {
  const renderer = new DOMRenderer(container, options);
  const module = vm.getModule();

  if (module) {
    renderer.initFromModule(module);
    vm.setRenderer(renderer);
    renderer.bindEvents(vm);
  }

  return renderer;
}

/**
 * Create and run a Kryon app from a .krb URL
 */
export async function createApp(
  url: string,
  container: HTMLElement,
  options?: RenderOptions
): Promise<{ vm: KryonVM; renderer: DOMRenderer }> {
  const vm = new KryonVM({ debug: options?.debug });
  await vm.loadFromUrl(url);

  const renderer = render(vm, container, options);

  return { vm, renderer };
}
