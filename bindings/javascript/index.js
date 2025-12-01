/**
 * Kryon JavaScript Bindings
 *
 * Plain JavaScript API for building Kryon applications.
 * This wrapper provides a simple, object-based component creation API
 * that works without JSX or TypeScript compilation.
 *
 * @module kryon
 */

// Re-export TypeScript bindings for JavaScript consumption
// Note: Bun can import TypeScript files directly
export { run } from '../typescript/src/index.ts';

/**
 * Create a virtual DOM element (IR node)
 *
 * @param {string} type - Component type ('app', 'container', 'text', etc.)
 * @param {Object} props - Component properties
 * @param {...any} children - Child elements
 * @returns {Object} IR node
 */
export function createElement(type, props = {}, ...children) {
  return {
    type,
    props,
    children: children.flat().filter(Boolean)
  };
}

/**
 * Shorthand for createElement
 */
export const h = createElement;

/**
 * Component factory functions
 * Provides a clean, functional API for creating components
 *
 * @example
 * import { c } from 'kryon';
 *
 * const myApp = c.app({
 *   windowTitle: 'My App'
 * }, [
 *   c.text({ content: 'Hello World!' })
 * ]);
 */
export const c = {
  /**
   * Create an App component (root element)
   * @param {Object} props - App properties (windowTitle, windowWidth, windowHeight, backgroundColor)
   * @param {...any} children - Child elements
   */
  app: (props, ...children) => createElement('app', props, ...children),

  /**
   * Create a Container component (generic flex container)
   * @param {Object} props - Container properties (width, height, padding, margin, backgroundColor, etc.)
   * @param {...any} children - Child elements
   */
  container: (props, ...children) => createElement('container', props, ...children),

  /**
   * Create a Column component (vertical layout)
   * @param {Object} props - Column properties (gap, align, justify, etc.)
   * @param {...any} children - Child elements
   */
  column: (props, ...children) => createElement('column', props, ...children),

  /**
   * Create a Row component (horizontal layout)
   * @param {Object} props - Row properties (gap, align, justify, etc.)
   * @param {...any} children - Child elements
   */
  row: (props, ...children) => createElement('row', props, ...children),

  /**
   * Create a Center component (centered layout)
   * @param {Object} props - Center properties
   * @param {...any} children - Child elements
   */
  center: (props, ...children) => createElement('center', props, ...children),

  /**
   * Create a Text component
   * @param {Object} props - Text properties (content, fontSize, fontWeight, color, etc.)
   */
  text: (props) => createElement('text', props),

  /**
   * Create a Button component
   * @param {Object} props - Button properties (title, backgroundColor, onClick, etc.)
   */
  button: (props) => createElement('button', props),

  /**
   * Create an Input component
   * @param {Object} props - Input properties (placeholder, value, onChange, etc.)
   */
  input: (props) => createElement('input', props),

  /**
   * Create a Checkbox component
   * @param {Object} props - Checkbox properties (checked, onChange, label, etc.)
   */
  checkbox: (props) => createElement('checkbox', props),

  /**
   * Create a Dropdown component
   * @param {Object} props - Dropdown properties (options, selected, onChange, etc.)
   */
  dropdown: (props) => createElement('dropdown', props),

  /**
   * Create an Image component
   * @param {Object} props - Image properties (src, width, height, alt, etc.)
   */
  image: (props) => createElement('image', props),

  /**
   * Create a Canvas component
   * @param {Object} props - Canvas properties (width, height, onDraw, etc.)
   */
  canvas: (props) => createElement('canvas', props),

  /**
   * Create a Markdown component
   * @param {Object} props - Markdown properties (content, etc.)
   */
  markdown: (props) => createElement('markdown', props)
};

/**
 * Component factory functions (alternative export)
 * Same as 'c' but with full names
 */
export const components = c;
