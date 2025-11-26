// Kryon JSX Runtime - Custom JSX transform for Bun
// This implements the automatic JSX runtime (react-jsx)

import type { IRNode, BaseProps } from './types.ts';

export type { IRNode };

// JSX namespace for TypeScript type checking
export namespace JSX {
  export interface Element extends IRNode {}

  export interface IntrinsicElements {
    // Layout components
    container: ContainerProps;
    row: RowProps;
    column: ColumnProps;
    center: CenterProps;

    // Content components
    text: TextProps;
    button: ButtonProps;
    input: InputProps;
    checkbox: CheckboxProps;
    dropdown: DropdownProps;

    // Media components
    image: ImageProps;
    canvas: CanvasProps;
    markdown: MarkdownProps;
  }

  export interface ElementChildrenAttribute {
    children: {};
  }
}

// Component Props Interfaces
interface ContainerProps extends BaseProps {
  tag?: string;
}

interface RowProps extends BaseProps {
  gap?: number;
  wrap?: boolean;
  justify?: 'start' | 'center' | 'end' | 'space-between' | 'space-around' | 'space-evenly';
  align?: 'start' | 'center' | 'end';
}

interface ColumnProps extends BaseProps {
  gap?: number;
  wrap?: boolean;
  justify?: 'start' | 'center' | 'end' | 'space-between' | 'space-around' | 'space-evenly';
  align?: 'start' | 'center' | 'end';
}

interface CenterProps extends BaseProps {}

interface TextProps extends BaseProps {
  content?: string;
  color?: string;
  fontSize?: number;
  fontFamily?: string;
  bold?: boolean;
  italic?: boolean;
}

interface ButtonProps extends BaseProps {
  title?: string;
  onClick?: () => void;
  disabled?: boolean;
}

interface InputProps extends BaseProps {
  placeholder?: string;
  value?: string;
  onChange?: (value: string) => void;
  onSubmit?: (value: string) => void;
}

interface CheckboxProps extends BaseProps {
  checked?: boolean;
  label?: string;
  onChange?: (checked: boolean) => void;
}

interface DropdownProps extends BaseProps {
  options?: string[];
  selected?: number;
  onChange?: (index: number) => void;
}

interface ImageProps extends BaseProps {
  src?: string;
  alt?: string;
}

interface CanvasProps extends BaseProps {
  onDraw?: (ctx: any) => void;
}

interface MarkdownProps extends BaseProps {
  content?: string;
}

// Flatten children array (handles nested arrays from map() etc.)
function flattenChildren(children: any[]): (IRNode | string)[] {
  const result: (IRNode | string)[] = [];

  for (const child of children) {
    if (child === null || child === undefined || child === false) {
      continue;
    }
    if (Array.isArray(child)) {
      result.push(...flattenChildren(child));
    } else if (typeof child === 'string' || typeof child === 'number') {
      result.push(String(child));
    } else if (typeof child === 'object' && 'type' in child) {
      result.push(child as IRNode);
    }
  }

  return result;
}

// JSX Factory function - called by Bun's JSX transform
export function jsx(
  type: string | Function,
  props: Record<string, any>,
  key?: string
): IRNode {
  // Handle functional components
  if (typeof type === 'function') {
    return type(props);
  }

  // Extract children from props
  const { children: rawChildren, ...restProps } = props;

  // Normalize children to array
  let children: (IRNode | string)[] = [];
  if (rawChildren !== undefined) {
    if (Array.isArray(rawChildren)) {
      children = flattenChildren(rawChildren);
    } else if (typeof rawChildren === 'string' || typeof rawChildren === 'number') {
      children = [String(rawChildren)];
    } else if (typeof rawChildren === 'object' && 'type' in rawChildren) {
      children = [rawChildren];
    }
  }

  return {
    type,
    props: restProps,
    children,
    key,
  };
}

// jsxs is used when there are multiple static children
export const jsxs = jsx;

// jsxDEV is used in development mode
export const jsxDEV = jsx;

// Fragment support
export function Fragment(props: { children?: any }): IRNode {
  return {
    type: 'fragment',
    props: {},
    children: props.children ? flattenChildren(
      Array.isArray(props.children) ? props.children : [props.children]
    ) : [],
  };
}
