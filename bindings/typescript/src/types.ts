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
  Sprite = 12,
  TabGroup = 13,
  TabBar = 14,
  Tab = 15,
  TabContent = 16,
  TabPanel = 17,
  Table = 18,
  TableHead = 19,
  TableBody = 20,
  TableFoot = 21,
  TableRow = 22,
  TableCell = 23,
  TableHeaderCell = 24,
  Flowchart = 25,
  FlowchartNode = 26,
  FlowchartEdge = 27,
  FlowchartSubgraph = 28,
  FlowchartLabel = 29,
  Link = 30,
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
  // Individual margin properties (override unified margin for specific sides)
  marginTop?: number;
  marginBottom?: number;
  marginLeft?: number;
  marginRight?: number;
  // Individual padding properties (override unified padding for specific sides)
  paddingTop?: number;
  paddingBottom?: number;
  paddingLeft?: number;
  paddingRight?: number;
  backgroundColor?: Color;
  borderColor?: Color;
  borderWidth?: number;
  borderRadius?: number;
  opacity?: number;
  zIndex?: number;
  children?: IRNode | IRNode[] | string;
}

// Flowchart props
export interface FlowchartProps extends BaseProps {
  direction?: 'TB' | 'BT' | 'LR' | 'RL';  // Top-Bottom, Bottom-Top, Left-Right, Right-Left
}

export interface FlowchartNodeProps {
  id: string;
  shape: 'rectangle' | 'rounded' | 'stadium' | 'diamond' | 'circle' |
         'hexagon' | 'cylinder' | 'subroutine' | 'asymmetric';
  label: string;
}

export interface FlowchartEdgeProps {
  from: string;
  to: string;
  type: 'arrow' | 'dotted' | 'open' | 'thick' | 'bidirectional';
  label?: string;
}

export interface FlowchartSubgraphProps {
  id: string;
  title: string;
}

// App configuration
export interface AppConfig {
  title: string;
  width: number;
  height: number;
}
