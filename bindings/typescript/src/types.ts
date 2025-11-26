// Kryon TypeScript Types - Matching C IR types

export type Pointer = number;

// Component types matching ir_core.h
export enum ComponentType {
  Container = 0,
  Text = 1,
  Button = 2,
  Input = 3,
  Checkbox = 4,
  Dropdown = 5,
  Row = 6,
  Column = 7,
  Center = 8,
  Image = 9,
  Canvas = 10,
  Markdown = 11,
}

// Dimension types
export enum DimensionType {
  Px = 0,
  Percent = 1,
  Auto = 2,
  Flex = 3,
}

// Alignment types
export enum Alignment {
  Start = 0,
  Center = 1,
  End = 2,
  SpaceBetween = 3,
  SpaceAround = 4,
  SpaceEvenly = 5,
}

// Event types
export enum EventType {
  Click = 0,
  Hover = 1,
  Focus = 2,
  Blur = 3,
  Change = 4,
  KeyDown = 5,
  KeyUp = 6,
}

// Dimension with type and value
export interface Dimension {
  type: DimensionType;
  value: number;
}

// IRNode - Virtual DOM node before rendering to C
export interface IRNode {
  type: string;
  props: Record<string, any>;
  children: (IRNode | string)[];
  key?: string;
}

// Color can be hex string or named color
export type Color = string;

// Size can be number (px), percentage string, or 'auto'
export type Size = number | `${number}%` | 'auto' | `${number}px`;

// Common props shared by all components
export interface BaseProps {
  width?: Size;
  height?: Size;
  minWidth?: Size;
  maxWidth?: Size;
  minHeight?: Size;
  maxHeight?: Size;
  padding?: number | [number, number] | [number, number, number, number];
  margin?: number | [number, number] | [number, number, number, number];
  backgroundColor?: Color;
  borderColor?: Color;
  borderWidth?: number;
  borderRadius?: number;
  opacity?: number;
  zIndex?: number;
  children?: IRNode | IRNode[] | string;
}

// App configuration
export interface AppConfig {
  title: string;
  width: number;
  height: number;
}
