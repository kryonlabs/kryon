export interface RuntimeOptions {
  app?: AppMeta;
  target?: Element | string | null;
  webStyleSheets?: string | WebStyleSheet | Array<string | WebStyleSheet>;
}

export interface AppMeta {
  title: string;
  width: number;
  height: number;
  fps: number;
  frame: string;
  styles?: AppStyleMeta[];
  routes?: AppRouteMeta[];
}

export interface AppStyleMeta {
  kind: "file" | "builtin" | string;
  target: string;
  alias: string;
  source?: string;
}

export interface AppRouteMeta {
  id: string;
  title: string;
  group: string;
  page: string;
  path: string;
}

export interface Runtime {
  app: AppMeta | null;
  target: Element | string | null;
  frame: RuntimeItem[];
  statements: RuntimeItem[];
  hostCalls: RuntimeItem[];
  mounted: boolean;
  webCompositeStack?: WebCompositeContext[];
  webStyleSheets?: Array<string | WebStyleSheet>;
  instanceFrame: number;
  instances: Map<string, Map<bigint, { value: unknown; frameSeen: number }>>;
  input: {
    events: unknown[];
    focus: number;
    clipboard: string;
    selections: Map<number, { anchor: number; cursor: number }>;
    dropdownOpen: number | null;
    focusOrder: number[];
    lastFocusOrder: number[];
  };
  QueueText(text: string): void;
  QueueKey(key: number): void;
  QueueShiftKey(key: number): void;
  QueueShortcut(key: number): void;
  QueueTap(x: number, y: number): void;
  SetClipboardText(text: string): void;
  ClipboardText(): string;
  SetSelection(focusID: number, anchor: number, cursor: number): void;
  SetFocus(id: number): void;
  Focus(): number;
}

export interface WebCompositeContext {
  name: string;
  path: string;
}

export interface RuntimeItem {
  kind: string;
  name?: string;
  args?: unknown;
  meta?: Record<string, unknown> | null;
  text?: string;
}

export interface WebDocumentNode {
  index: number;
  kind: string;
  tag: string;
  key: string;
  name: string;
  webRef: string;
  path: string;
  parentPath: string;
  sourcePath: string;
  sourceLine: number;
  sourceColumn: number;
  sourceEndLine: number;
  sourceEndColumn: number;
  domId: string;
  domName: string;
  domValue: string;
  classes: string[];
  title: string;
  placeholder: string;
  tabIndex: number | null;
  text: string;
  value: unknown;
  level: number;
  href: string;
  target: string;
  rel: string;
  htmlFor: string;
  part: string;
  slot: string;
  dataAttrs: Record<string, string>;
  extraAttrs: Record<string, string>;
  inputType: string;
  formOwner: string;
  formAction: string;
  formMethod: string;
  formEncType: string;
  autoComplete: string;
  hidden: boolean;
  draggable: string;
  spellCheck: string;
  contentEditable: string;
  autoFocus: boolean;
  inert: boolean;
  autoCapitalize: string;
  enterKeyHint: string;
  download: string;
  formNoValidate: boolean;
  noValidate: boolean;
  clickable: boolean;
  popover: string;
  popoverTarget: string;
  popoverTargetAction: string;
  readOnly: boolean;
  required: boolean;
  min: string;
  max: string;
  step: string;
  minLength: string;
  maxLength: string;
  pattern: string;
  accept: string;
  multiple: boolean;
  inputMode: string;
  headers: string;
  scope: string;
  colSpan: string;
  rowSpan: string;
  alt: string;
  asset: string;
  role: string;
  ariaLabel: string;
  ariaDescription: string;
  ariaDescribedBy: string;
  ariaLabelledBy: string;
  ariaActiveDescendant: string;
  ariaControls: string;
  ariaOwns: string;
  ariaSort: string;
  ariaOrientation: string;
  ariaLevel: string;
  ariaPosInSet: string;
  ariaSetSize: string;
  ariaHasPopup: string;
  ariaMultiSelectable: string;
  ariaRowIndex: string;
  ariaColIndex: string;
  ariaRowCount: string;
  ariaColCount: string;
  ariaLive: string;
  ariaAttrs: Record<string, string>;
  onClick: string;
  onDoubleClick: string;
  onInput: string;
  onBeforeInput: string;
  onChange: string;
  onSelect: string;
  onKey: string;
  onKeyUp: string;
  onInvalid: string;
  onSubmit: string;
  onReset: string;
  onToggle: string;
  onClose: string;
  onCancel: string;
  onFocus: string;
  onBlur: string;
  onScroll: string;
  onMouseEnter: string;
  onMouseLeave: string;
  onMouseMove: string;
  onMouseDown: string;
  onMouseUp: string;
  onPointerEnter: string;
  onPointerLeave: string;
  onPointerMove: string;
  onPointerDown: string;
  onPointerUp: string;
  onPointerCancel: string;
  onWheel: string;
  onContextMenu: string;
  onDragStart: string;
  onDragEnd: string;
  onDragOver: string;
  onDrop: string;
  onCopy: string;
  onCut: string;
  onPaste: string;
  action: (() => unknown) | null;
  doubleClickAction: (() => unknown) | null;
  inputAction: ((value: unknown) => unknown) | null;
  beforeInputAction: ((value: string) => unknown) | null;
  changeAction: ((value: unknown) => unknown) | null;
  selectAction: ((value: string) => unknown) | null;
  keyAction: ((key: string) => unknown) | null;
  keyUpAction: ((key: string) => unknown) | null;
  invalidAction: ((value: unknown) => unknown) | null;
  submitAction: ((values: Record<string, unknown>) => unknown) | null;
  resetAction: ((values: Record<string, unknown>) => unknown) | null;
  toggleAction: (() => unknown) | null;
  closeAction: (() => unknown) | null;
  cancelAction: (() => unknown) | null;
  focusAction: (() => unknown) | null;
  blurAction: (() => unknown) | null;
  scrollAction: ((value: number) => unknown) | null;
  mouseEnterAction: (() => unknown) | null;
  mouseLeaveAction: (() => unknown) | null;
  mouseMoveAction: (() => unknown) | null;
  mouseDownAction: (() => unknown) | null;
  mouseUpAction: (() => unknown) | null;
  pointerEnterAction: (() => unknown) | null;
  pointerLeaveAction: (() => unknown) | null;
  pointerMoveAction: (() => unknown) | null;
  pointerDownAction: (() => unknown) | null;
  pointerUpAction: (() => unknown) | null;
  pointerCancelAction: (() => unknown) | null;
  wheelAction: ((value: number) => unknown) | null;
  contextMenuAction: (() => unknown) | null;
  dragStartAction: ((value: unknown) => unknown) | null;
  dragEndAction: ((value: unknown) => unknown) | null;
  dragOverAction: (() => unknown) | null;
  dropAction: ((value: unknown) => unknown) | null;
  copyAction: ((value: unknown) => unknown) | null;
  cutAction: ((value: unknown) => unknown) | null;
  pasteAction: ((value: unknown) => unknown) | null;
  pageTitle: string;
  pageDescription: string;
  pageCanonicalURL: string;
  pageThemeColor: string;
  bounds: { x: number; y: number; width: number; height: number };
  hasBounds: boolean;
  scrollLeft: number;
  scrollTop: number;
  state: {
    disabled: boolean;
    loading: boolean;
    selected: boolean;
    checked: boolean;
    invalid: boolean;
    expanded: boolean;
    open: boolean;
    hover: boolean;
    pressed: boolean;
    focus: boolean;
  };
  styleFacts: WebNodeStyleFacts;
}

export interface WebNodeStyleFacts {
  index: number;
  kind: string;
  tag: string;
  key: string;
  name: string;
  path: string;
  parentPath: string;
  ref: string;
  webRef: string;
  sourcePath: string;
  sourceLine: number;
  sourceColumn: number;
  sourceEndLine: number;
  sourceEndColumn: number;
  sourceRef: string;
  sourceColumnRef: string;
  sourceRangeRef: string;
  id: string;
  domName: string;
  title: string;
  tabIndex: number | null;
  domValue: string;
  href: string;
  target: string;
  rel: string;
  alt: string;
  asset: string;
  src: string;
  htmlFor: string;
  part: string;
  slot: string;
  inputType: string;
  formOwner: string;
  formAction: string;
  formMethod: string;
  formEncType: string;
  autoComplete: string;
  hidden: boolean;
  draggable: string;
  spellCheck: string;
  contentEditable: string;
  autoFocus: boolean;
  inert: boolean;
  autoCapitalize: string;
  enterKeyHint: string;
  download: string;
  formNoValidate: boolean;
  noValidate: boolean;
  clickable: boolean;
  popover: string;
  popoverTarget: string;
  popoverTargetAction: string;
  open: boolean;
  scrollLeft: number;
  scrollTop: number;
  readOnly: boolean;
  required: boolean;
  min: string;
  max: string;
  step: string;
  minLength: string;
  maxLength: string;
  pattern: string;
  accept: string;
  multiple: boolean;
  inputMode: string;
  headers: string;
  scope: string;
  colSpan: string;
  rowSpan: string;
  classes: string[];
  dataAttrs: Record<string, string>;
  ariaAttrs: Record<string, string>;
  extraAttrs: Record<string, string>;
  role: string;
  ariaLabel: string;
  ariaDescription: string;
  ariaDescribedBy: string;
  ariaLabelledBy: string;
  ariaActiveDescendant: string;
  ariaControls: string;
  ariaOwns: string;
  ariaSort: string;
  ariaOrientation: string;
  ariaLevel: string;
  ariaPosInSet: string;
  ariaSetSize: string;
  ariaHasPopup: string;
  ariaMultiSelectable: string;
  ariaRowIndex: string;
  ariaColIndex: string;
  ariaRowCount: string;
  ariaColCount: string;
  ariaLive: string;
  state: Record<string, boolean>;
}

export interface WebNodeIdentity {
  ref: string;
  aliases: string[];
  index: number;
  kind: string;
  tag: string;
  key: string;
  name: string;
  path: string;
  parentPath: string;
  webRef: string;
  domId: string;
  domName: string;
  sourcePath: string;
  sourceLine: number;
  sourceColumn: number;
  sourceEndLine: number;
  sourceEndColumn: number;
  sourceRef: string;
  sourceColumnRef: string;
  sourceRangeRef: string;
}

export interface WebDocumentFrame {
  app: AppMeta | null;
  nodes: WebDocumentNode[];
  metadata: {
    title: string;
    description: string;
    canonicalURL: string;
    themeColor: string;
  };
}

export interface WebAccessibilityNode {
  path: string;
  sourcePath: string;
  sourceLine: number;
  sourceColumn: number;
  sourceEndLine: number;
  sourceEndColumn: number;
  name: string;
  kind: string;
  tag: string;
  id: string;
  classes: string[];
  role: string;
  label: string;
  description: string;
  text: string;
  value: unknown;
  href: string;
  inputType: string;
  level: number;
  rowIndex: string;
  colIndex: string;
  rowCount: string;
  colCount: string;
  state: Record<string, boolean>;
}

export interface WebAccessibilitySnapshot {
  title: string;
  description: string;
  nodes: WebAccessibilityNode[];
}

export interface WebDOMObject {
  ref: string;
  node: WebDocumentNode;
  element: Element;
  readonly root: Element | null;
  readonly identity: WebNodeIdentity;
  readonly snapshot: WebDOMSnapshot | null;
  readonly parent: WebDOMObject | null;
  readonly children: WebDOMObject[];
  readonly relations: WebDOMRelations | null;
  readonly descendants: WebDOMObject[];
  query(selector: string): WebDOMObject | null;
  queryAll(selector: string): WebDOMObject[];
  matches(selector: string): boolean;
  closest(selector: string): WebDOMObject | null;
  listen(type: string, handler: (event: Event, object: WebDOMObject | null) => unknown,
    options?: boolean | AddEventListenerOptions): (() => void) | null;
  addClass(className: string): boolean;
  removeClass(className: string): boolean;
  toggleClass(className: string, force?: boolean): boolean;
  hasClass(className: string): boolean;
  getAttr(name: string): string | undefined;
  setAttr(name: string, value?: unknown): boolean;
  removeAttr(name: string): boolean;
  hasAttr(name: string): boolean;
  getProp(name: string): unknown;
  setProp(name: string, value: unknown): boolean;
  getStyle(name: string): string | undefined;
  setStyle(name: string, value?: unknown): boolean;
  removeStyle(name: string): boolean;
  computedStyle(name?: string): unknown;
  getState(name: string): boolean | undefined;
  setState(name: string, value: boolean): boolean;
  text(): string | undefined;
  text(text: unknown): boolean;
  value(): unknown;
  value(value: unknown): boolean;
  dispatch(type: string, init?: Record<string, unknown>): boolean;
  click(): boolean;
  focus(): boolean;
  blur(): boolean;
  submit(): boolean;
  reset(): boolean;
  rect(): { x: number; y: number; width: number; height: number; left: number; top: number; right: number; bottom: number } | null;
  scroll(): { left: number; top: number; width: number; height: number } | null;
  scroll(left: number, top?: number | null): boolean;
  scrollIntoView(options?: boolean | ScrollIntoViewOptions): boolean;
  showModal(): boolean;
  close(returnValue?: string): boolean;
  showPopover(): boolean;
  hidePopover(): boolean;
  togglePopover(force?: boolean): boolean;
  sync(): WebDOMObject | null;
}

export interface WebDOMRenderDetail {
  frame: WebDocumentFrame;
  root: Element;
  objects: WebDOMObject[];
}

export interface WebDOMLifecycleDetail {
  frame: WebDocumentFrame;
  root: Element;
  object: WebDOMObject;
  node: WebDocumentNode;
  element: Element;
}

export interface WebDOMObserveDetail {
  root: Element;
  frame: WebDocumentFrame | null;
  event: Event | null;
}

export interface WebDOMObserveOptions {
  immediate?: boolean;
}

export interface WebDOMRelations {
  describedBy: WebDOMObject[];
  describes: WebDOMObject[];
  controls: WebDOMObject[];
  controlledBy: WebDOMObject[];
  owns: WebDOMObject[];
  ownedBy: WebDOMObject[];
  headers: WebDOMObject[];
  rowHeaders: WebDOMObject[];
  columnHeaders: WebDOMObject[];
  rowGroupHeaders: WebDOMObject[];
  columnGroupHeaders: WebDOMObject[];
  labelFor: WebDOMObject | null;
  formOwner: WebDOMObject | null;
  labelledBy: WebDOMObject[];
  activeDescendant: WebDOMObject | null;
  popoverTarget: WebDOMObject | null;
  popoverInvokers: WebDOMObject[];
}

export interface WebNodeEventRefs {
  click: string;
  doubleClick: string;
  input: string;
  beforeInput: string;
  change: string;
  select: string;
  key: string;
  keyUp: string;
  invalid: string;
  submit: string;
  reset: string;
  toggle: string;
  close: string;
  cancel: string;
  focus: string;
  blur: string;
  scroll: string;
  mouseEnter: string;
  mouseLeave: string;
  mouseMove: string;
  mouseDown: string;
  mouseUp: string;
  pointerEnter: string;
  pointerLeave: string;
  pointerMove: string;
  pointerDown: string;
  pointerUp: string;
  pointerCancel: string;
  wheel: string;
  contextMenu: string;
  dragStart: string;
  dragEnd: string;
  dragOver: string;
  drop: string;
  copy: string;
  cut: string;
  paste: string;
}

export interface WebDOMBindHandlers {
  mount?: (object: WebDOMObject, detail: WebDOMObserveDetail) =>
    unknown | ((object: WebDOMObject, detail: WebDOMObserveDetail) => unknown);
  update?: (object: WebDOMObject, detail: WebDOMObserveDetail,
    previous: WebDOMObject | null) => unknown;
  unmount?: (object: WebDOMObject, detail: WebDOMObserveDetail) => unknown;
}

declare global {
  interface Element {
    readonly kryRef?: string;
    readonly kryPath?: string;
    readonly kryAliases?: string[];
    readonly kryIndex?: number;
    readonly kryKind?: string;
    readonly kryTag?: string;
    readonly kryName?: string;
    readonly kryKey?: string;
    readonly krySourceRef?: string;
    readonly krySourceColumnRef?: string;
    readonly krySourceRangeRef?: string;
    readonly krySourcePath?: string;
    readonly krySourceLine?: number;
    readonly krySourceColumn?: number;
    readonly krySourceEndLine?: number;
    readonly krySourceEndColumn?: number;
    readonly kryNode?: WebDocumentNode | null;
    readonly kryRoot?: Element | null;
    readonly kryObject?: WebDOMObject | null;
    readonly kryIdentity?: WebNodeIdentity | null;
    readonly krySnapshot?: WebDOMSnapshot | null;
    readonly kryParent?: WebDOMObject | null;
    readonly kryChildren?: WebDOMObject[];
    readonly kryRelations?: WebDOMRelations | null;
    kryDescendants?(): WebDOMObject[];
    readonly kryRuntime?: Runtime | null;
    readonly kryFrame?: WebDocumentFrame | null;
    readonly kryObjects?: WebDOMObject[];
    readonly kryObjectMap?: Map<string, WebDOMObject>;
    kryMatches?(selector: string): boolean;
    kryClosest?(selector: string): WebDOMObject | null;
    kryElement?(query: string): Element | null;
    kryObject?(query: string): WebDOMObject | null;
    kryQuery?(selector: string): WebDOMObject | null;
    kryQueryAll?(selector: string): WebDOMObject[];
    kryQueryWithin?(query: string, selector: string): WebDOMObject | null;
    kryQueryAllWithin?(query: string, selector: string): WebDOMObject[];
    kryAtSource?(sourcePath: string, sourceLine: number, sourceColumn?: number): WebDOMObject | null;
    kryAtSourceRange?(sourcePath: string, sourceLine: number, sourceColumn?: number): WebDOMObject | null;
    readonly krySourceMap?: WebDOMObject[];
    kryListen?: {
      (type: string, handler: (event: Event, object: WebDOMObject | null) => unknown,
        options?: boolean | AddEventListenerOptions): (() => void) | null;
      (query: string, type: string, handler: (event: Event, object: WebDOMObject | null) => unknown,
        options?: boolean | AddEventListenerOptions): (() => void) | null;
    };
    kryDelegate?(selector: string, type: string, handler: (event: Event, object: WebDOMObject) => unknown,
      options?: boolean | AddEventListenerOptions): (() => void) | null;
    kryObserve?(selector: string, handler: (objects: WebDOMObject[], detail: WebDOMObserveDetail) => unknown,
      options?: WebDOMObserveOptions): (() => void) | null;
    kryBind?(selector: string, handlers: WebDOMBindHandlers |
      ((object: WebDOMObject, detail: WebDOMObserveDetail) => unknown),
      options?: WebDOMObserveOptions): (() => void) | null;
    kryAddClass?(className: string): boolean;
    kryAddClass?(query: string, className: string): boolean;
    kryRemoveClass?(className: string): boolean;
    kryRemoveClass?(query: string, className: string): boolean;
    kryToggleClass?(className: string, force?: boolean): boolean;
    kryToggleClass?(query: string, className: string, force?: boolean): boolean;
    kryHasClass?(className: string): boolean;
    kryHasClass?(query: string, className: string): boolean;
    kryGetAttr?(name: string): string | undefined;
    kryGetAttr?(query: string, name: string): string | undefined;
    krySetAttr?(name: string, value?: unknown): boolean;
    krySetAttr?(query: string, name: string, value?: unknown): boolean;
    kryRemoveAttr?(name: string): boolean;
    kryRemoveAttr?(query: string, name: string): boolean;
    kryHasAttr?(name: string): boolean;
    kryHasAttr?(query: string, name: string): boolean;
    kryGetProp?(name: string): unknown;
    kryGetProp?(query: string, name: string): unknown;
    krySetProp?(name: string, value: unknown): boolean;
    krySetProp?(query: string, name: string, value: unknown): boolean;
    kryGetStyle?(name: string): string | undefined;
    kryGetStyle?(query: string, name: string): string | undefined;
    krySetStyle?(name: string, value?: unknown): boolean;
    krySetStyle?(query: string, name: string, value?: unknown): boolean;
    kryRemoveStyle?(name: string): boolean;
    kryRemoveStyle?(query: string, name: string): boolean;
    kryComputedStyle?(name?: string): unknown;
    kryComputedStyle?(query: string, name?: string): unknown;
    kryGetState?(name: string): boolean | undefined;
    kryGetState?(query: string, name: string): boolean | undefined;
    krySetState?(name: string, value: boolean): boolean;
    krySetState?(query: string, name: string, value: boolean): boolean;
    kryToggleState?(query: string, name: string, force?: boolean): boolean;
    kryText?: {
      (): string | undefined;
      (text: unknown): boolean;
      (query: string): string | undefined;
      (query: string, text: unknown): boolean;
    };
    kryValue?: {
      (): unknown;
      (value: unknown): boolean;
      (query: string): unknown;
      (query: string, value: unknown): boolean;
    };
    kryDispatch?(type: string, init?: Record<string, unknown>): boolean;
    kryDispatch?(query: string, type: string, init?: Record<string, unknown>): boolean;
    kryClick?(): boolean;
    kryClick?(query: string): boolean;
    kryFocus?(): boolean;
    kryFocus?(query: string): boolean;
    kryBlur?(): boolean;
    kryBlur?(query: string): boolean;
    krySubmit?(): boolean;
    krySubmit?(query: string): boolean;
    kryReset?(): boolean;
    kryReset?(query: string): boolean;
    kryRect?(): { x: number; y: number; width: number; height: number; left: number; top: number; right: number; bottom: number } | null;
    kryRect?(query: string): { x: number; y: number; width: number; height: number; left: number; top: number; right: number; bottom: number } | null;
    kryScroll?: {
      (): { left: number; top: number; width: number; height: number } | null;
      (left: number, top?: number | null): boolean;
      (query: string): { left: number; top: number; width: number; height: number } | null;
      (query: string, left: number, top?: number | null): boolean;
    };
    kryScrollIntoView?(options?: boolean | ScrollIntoViewOptions): boolean;
    kryScrollIntoView?(query: string, options?: boolean | ScrollIntoViewOptions): boolean;
    kryShowModal?(): boolean;
    kryShowModal?(query: string): boolean;
    kryClose?(returnValue?: string): boolean;
    kryClose?(query: string, returnValue?: string): boolean;
    kryShowPopover?(): boolean;
    kryShowPopover?(query: string): boolean;
    kryHidePopover?(): boolean;
    kryHidePopover?(query: string): boolean;
    kryTogglePopover?(force?: boolean): boolean;
    kryTogglePopover?(query: string, force?: boolean): boolean;
    krySync?(): WebDOMObject | WebDOMObject[] | null;
    krySync?(query: string): WebDOMObject | null;
  }

  interface Event {
    readonly kryRef?: string;
    readonly kryPath?: string;
    readonly kryAliases?: string[];
    readonly kryIndex?: number;
    readonly kryKind?: string;
    readonly kryTag?: string;
    readonly krySourceRef?: string;
    readonly krySourceColumnRef?: string;
    readonly krySourceRangeRef?: string;
    readonly krySourcePath?: string;
    readonly krySourceLine?: number;
    readonly krySourceColumn?: number;
    readonly krySourceEndLine?: number;
    readonly krySourceEndColumn?: number;
    readonly kryRoot?: Element | null;
    readonly kryObject?: WebDOMObject | null;
    readonly kryIdentity?: WebNodeIdentity | null;
    readonly krySnapshot?: WebDOMSnapshot | null;
  }
}

export interface WebDOMSnapshot {
  ref: string;
  aliases: string[];
  identity: WebNodeIdentity;
  index: number;
  kind: string;
  tag: string;
  role: string;
  path: string;
  webRef: string;
  parentPath: string;
  parentRef: string;
  childRefs: string[];
  relationRefs: {
    describedBy: string[];
    describes: string[];
    controls: string[];
    controlledBy: string[];
    owns: string[];
    ownedBy: string[];
    headers: string[];
    rowHeaders: string[];
    columnHeaders: string[];
    rowGroupHeaders: string[];
    columnGroupHeaders: string[];
    labelFor: string;
    formOwner: string;
    labelledBy: string[];
    activeDescendant: string;
    popoverTarget: string;
    popoverInvokers: string[];
  };
  eventRefs: WebNodeEventRefs;
  name: string;
  key: string;
  id: string;
  domName: string;
  classes: string[];
  sourcePath: string;
  sourceLine: number;
  sourceColumn: number;
  sourceEndLine: number;
  sourceEndColumn: number;
  sourceRef: string;
  sourceColumnRef: string;
  sourceRangeRef: string;
  styleFacts: WebNodeStyleFacts;
  text: string;
  value: unknown;
  state: Record<string, boolean>;
  attrs: Record<string, string>;
  dataset: Record<string, string>;
  style: Record<string, string>;
  rect: { x: number; y: number; width: number; height: number; left: number; top: number; right: number; bottom: number } | null;
  scroll: { left: number; top: number; width: number; height: number } | null;
}

export interface WebStyleSelector {
  kind: string;
  id: string;
  classes: string[];
  attrs: Record<string, string | null>;
  attrOps: Record<string, string>;
  pseudos: string[];
  not: WebStyleSelector[];
  matches: WebStyleSelector[];
  state: string;
  states: string[];
  specificity: number;
  combinator?: string;
  parts?: WebStyleSelector[];
}

export interface WebStyleRule {
  selector: WebStyleSelector;
  style: Record<string, unknown>;
  layer: number;
  order: number;
  score: number;
}

export interface WebStyleKeyframe {
  selector: string;
  style: Record<string, unknown>;
}

export interface WebStyleKeyframes {
  name: string;
  frames: WebStyleKeyframe[];
}

export interface WebStyleConditionalGroup {
  kind: "media" | "supports" | "container" | string;
  query: string;
  rules: WebStyleRule[];
}

export interface WebStyleSheet {
  pack?: string;
  rules: WebStyleRule[];
  keyframes?: WebStyleKeyframes[];
  groups?: WebStyleConditionalGroup[];
}

export interface Ref<T = unknown> {
  value: T;
  object: Record<string, T> | null;
  key: string;
}

export interface ColorValue {
  r: number;
  g: number;
  b: number;
  a: number;
}

export interface ThemeValue {
  name?: string;
  mode: number;
  colors: Record<string, ColorValue>;
  metrics: Record<string, number>;
}

export interface ThemeFamilyValue {
  name?: string;
  light: ThemeValue;
  dark: ThemeValue;
}

export function createRuntime(options?: RuntimeOptions): Runtime;
export function instanceState<T>(rt: Runtime, type: string, key: number | bigint,
  create: () => T): { value: T; frameSeen: number };
export function viewport(rt: Runtime, app?: AppMeta | null): {
  x: number;
  y: number;
  width: number;
  height: number;
};
export function beginFrame(rt: Runtime): Runtime;
export function endFrame(rt: Runtime): ReturnType<typeof snapshot>;
export function snapshot(rt: Runtime): {
  app: AppMeta | null;
  frame: RuntimeItem[];
  statements: RuntimeItem[];
  hostCalls: RuntimeItem[];
};
export function widget(rt: Runtime, name: string, args: string,
  state?: Record<string, unknown> | null, meta?: Record<string, unknown> | null): unknown;
export function beginWebComposite(rt: Runtime, name: string,
  meta?: Record<string, unknown> | null): WebCompositeContext | null;
export function endWebComposite(rt: Runtime,
  context?: WebCompositeContext | null): WebCompositeContext | null;
export function statement(rt: Runtime, text: string): RuntimeItem;
export function expr(text: string): { kind: "expr"; text: string };
export function struct(type: string, value: unknown): { type: string; value: unknown };
export function copyValue<T>(value: T): T;
export function recordValue<T>(type: string, value: T): T;
export function ref<T>(object: Record<string, T>, key: string): Ref<T>;
export function stateForModule(name?: string): Record<string, unknown>;
export function hostCall(host: unknown, method: string, args?: unknown[]): unknown;
export function webDocumentFrame(rt: Runtime): WebDocumentFrame;
export function webNodeStyleFacts(node: WebDocumentNode): WebNodeStyleFacts;
export function webNodeIdentity(node: WebDocumentNode): WebNodeIdentity;
export function webNodeEventRefs(node: WebDocumentNode | null): WebNodeEventRefs;
export function webSourceRef(sourcePath: string, sourceLine: number, sourceColumn?: number): string;
export function webAccessibilitySnapshot(source: Runtime | WebDocumentFrame): WebAccessibilitySnapshot;
export function parseWebStyleSheet(source: string): WebStyleSheet;
export function resolveWebStyle(node: WebDocumentNode, sheets?: string | WebStyleSheet | Array<string | WebStyleSheet>): Record<string, unknown>;
export function webStyleSelectorToCSS(selector: WebStyleSelector | Partial<WebStyleSelector>): string;
export function webStyleSheetToCSS(sheet: string | WebStyleSheet): string;
export function installWebStyleSheet(sheet: string | WebStyleSheet,
  target?: Element | null, id?: string): (() => void) | null;
export function loadAppWebStyleSheets(app: AppMeta | null): WebStyleSheet[];
export function installAppWebStyleSheets(app: AppMeta | null,
  target?: Element | null, id?: string): (() => void) | null;
export function setWebStyleSheets(rt: Runtime, sheets: string | WebStyleSheet | Array<string | WebStyleSheet>): Runtime;
export function renderWebDocument(rt: Runtime, target: Element | string | null): Runtime;
export function webDOMRoot(target: Element | string | null): Element | null;
export function webDOMFrame(target: Element | string | null): WebDocumentFrame | null;
export function findWebNode(rt: Runtime, query: string): WebDocumentNode | null;
export function webNodeQuery(rt: Runtime, selector: string): WebDocumentNode | null;
export function webNodeQueryAll(rt: Runtime, selector: string): WebDocumentNode[];
export function webNodeMatches(rt: Runtime, query: string, selector: string): boolean;
export function webNodeParent(rt: Runtime, query: string): WebDocumentNode | null;
export function webNodeChildren(rt: Runtime, query?: string): WebDocumentNode[];
export function webNodeDescendants(rt: Runtime, query?: string): WebDocumentNode[];
export function webNodeClosest(rt: Runtime, query: string, selector: string): WebDocumentNode | null;
export function webNodeQueryWithin(rt: Runtime, query: string, selector: string): WebDocumentNode | null;
export function webNodeQueryAllWithin(rt: Runtime, query: string, selector: string): WebDocumentNode[];
export function webNodeAtSource(rt: Runtime, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDocumentNode | null;
export function webNodesAtSource(rt: Runtime, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDocumentNode[];
export function webNodeAtSourceRange(rt: Runtime, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDocumentNode | null;
export function webNodesAtSourceRange(rt: Runtime, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDocumentNode[];
export function webSourceMap(rt: Runtime): WebNodeIdentity[];
export function findWebElement(target: Element | string | null, query: string): Element | null;
export function webDOMObject(target: Element | string | null, query: string): WebDOMObject | null;
export function webDOMIdentity(target: Element | string | null, query: string): WebNodeIdentity | null;
export function webDOMEventRefs(target: Element | string | null, query: string): WebNodeEventRefs | null;
export function webDOMRelations(target: Element | string | null, query: string): WebDOMRelations | null;
export function webDOMObjectFromElement(element: Element | null): WebDOMObject | null;
export function webDOMDecorateEvent(eventOrTarget: Event | EventTarget | null): WebDOMObject | null;
export function webDOMObjectFromEvent(eventOrTarget: Event | EventTarget | null): WebDOMObject | null;
export function webDOMIdentityFromEvent(eventOrTarget: Event | EventTarget | null): WebNodeIdentity | null;
export function webDOMElementMatches(element: Element | null, selector: string): boolean;
export function webDOMObjects(target: Element | string | null): WebDOMObject[];
export function webDOMObjectMap(target: Element | string | null): Map<string, WebDOMObject>;
export function webDOMObserve(target: Element | string | null, selector: string,
  handler: (objects: WebDOMObject[], detail: WebDOMObserveDetail) => unknown,
  options?: WebDOMObserveOptions): (() => void) | null;
export function webDOMBind(target: Element | string | null, selector: string,
  handlers: WebDOMBindHandlers | ((object: WebDOMObject, detail: WebDOMObserveDetail) => unknown),
  options?: WebDOMObserveOptions): (() => void) | null;
export function webDOMSync(target: Element | string | null): WebDOMObject[];
export function webDOMSync(target: Element | string | null, query: string): WebDOMObject | null;
export function webDOMSnapshot(target: Element | string | null, query: string): WebDOMSnapshot | null;
export function webDOMSnapshots(target: Element | string | null, selector?: string): WebDOMSnapshot[];
export function webDOMSnapshotFromElement(element: Element | null): WebDOMSnapshot | null;
export function webDOMSnapshotFromEvent(eventOrTarget: Event | EventTarget | null): WebDOMSnapshot | null;
export function webDOMParent(target: Element | string | null, query: string): WebDOMObject | null;
export function webDOMChildren(target: Element | string | null, query?: string): WebDOMObject[];
export function webDOMDescendants(target: Element | string | null, query?: string): WebDOMObject[];
export function webDOMClosest(target: Element | string | null, query: string, selector: string): WebDOMObject | null;
export function webDOMQuery(target: Element | string | null, selector: string): WebDOMObject | null;
export function webDOMQueryAll(target: Element | string | null, selector: string): WebDOMObject[];
export function webDOMQueryWithin(target: Element | string | null, query: string, selector: string): WebDOMObject | null;
export function webDOMQueryAllWithin(target: Element | string | null, query: string, selector: string): WebDOMObject[];
export function webDOMMatches(target: Element | string | null, query: string, selector: string): boolean;
export function webDOMObjectAtSource(target: Element | string | null, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDOMObject | null;
export function webDOMObjectsAtSource(target: Element | string | null, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDOMObject[];
export function webDOMObjectAtSourceRange(target: Element | string | null, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDOMObject | null;
export function webDOMObjectsAtSourceRange(target: Element | string | null, sourcePath: string, sourceLine: number, sourceColumn?: number): WebDOMObject[];
export function webDOMSourceMap(target: Element | string | null): WebDOMObject[];
export function webDOMAddEventListener(target: Element | string | null, query: string, type: string,
  handler: (event: Event, object: WebDOMObject | null) => unknown,
  options?: boolean | AddEventListenerOptions): (() => void) | null;
export function webDOMAddDelegatedEventListener(target: Element | string | null, selector: string, type: string,
  handler: (event: Event, object: WebDOMObject) => unknown,
  options?: boolean | AddEventListenerOptions): (() => void) | null;
export function webDOMAddClass(target: Element | string | null, query: string, className: string): boolean;
export function webDOMRemoveClass(target: Element | string | null, query: string, className: string): boolean;
export function webDOMToggleClass(target: Element | string | null, query: string, className: string, force?: boolean): boolean;
export function webDOMHasClass(target: Element | string | null, query: string, className: string): boolean;
export function webDOMSetState(target: Element | string | null, query: string, name: string, value: boolean): boolean;
export function webDOMToggleState(target: Element | string | null, query: string, name: string, force?: boolean): boolean;
export function webDOMGetState(target: Element | string | null, query: string, name: string): boolean | undefined;
export function webDOMSetAttribute(target: Element | string | null, query: string, name: string, value?: unknown): boolean;
export function webDOMRemoveAttribute(target: Element | string | null, query: string, name: string): boolean;
export function webDOMGetAttribute(target: Element | string | null, query: string, name: string): string | undefined;
export function webDOMHasAttribute(target: Element | string | null, query: string, name: string): boolean;
export function webDOMSetProperty(target: Element | string | null, query: string, name: string, value: unknown): boolean;
export function webDOMGetProperty(target: Element | string | null, query: string, name: string): unknown;
export function webDOMSetStyle(target: Element | string | null, query: string, name: string, value?: unknown): boolean;
export function webDOMRemoveStyle(target: Element | string | null, query: string, name: string): boolean;
export function webDOMGetStyle(target: Element | string | null, query: string, name: string): string | undefined;
export function webDOMComputedStyle(target: Element | string | null, query: string, name?: string): unknown;
export function webDOMRect(target: Element | string | null, query: string): { x: number; y: number; width: number; height: number; left: number; top: number; right: number; bottom: number } | null;
export function webDOMGetScroll(target: Element | string | null, query: string): { left: number; top: number; width: number; height: number } | null;
export function webDOMSetScroll(target: Element | string | null, query: string, left: number, top?: number | null): boolean;
export function webDOMScrollIntoView(target: Element | string | null, query: string, options?: boolean | ScrollIntoViewOptions): boolean;
export function webDOMGetText(target: Element | string | null, query: string): string | undefined;
export function webDOMSetText(target: Element | string | null, query: string, text: unknown): boolean;
export function webDOMGetValue(target: Element | string | null, query: string): unknown;
export function webDOMSetValue(target: Element | string | null, query: string, value: unknown): boolean;
export function webDOMClick(target: Element | string | null, query: string): boolean;
export function webDOMFocus(target: Element | string | null, query: string): boolean;
export function webDOMBlur(target: Element | string | null, query: string): boolean;
export function webDOMSubmit(target: Element | string | null, query: string): boolean;
export function webDOMReset(target: Element | string | null, query: string): boolean;
export function webDOMShowModal(target: Element | string | null, query: string): boolean;
export function webDOMClose(target: Element | string | null, query: string, returnValue?: string): boolean;
export function webDOMShowPopover(target: Element | string | null, query: string): boolean;
export function webDOMHidePopover(target: Element | string | null, query: string): boolean;
export function webDOMTogglePopover(target: Element | string | null, query: string, force?: boolean): boolean;
export function webDOMDispatchEvent(target: Element | string | null, query: string, type: string, init?: Record<string, unknown>): boolean;
export function webFormValue(target: Element | string | null, query: string): unknown;
export function webFormValues(target: Element | string | null, query?: string): Record<string, unknown>;
export function mount(rt: Runtime, target: Element | string | null): Runtime;
export function Color(r?: number, g?: number, b?: number, a?: number): ColorValue;
export function NewVector2(x?: number, y?: number): { x: number; y: number };
export function NewRectangle(x?: number, y?: number, width?: number, height?: number): {
  x: number;
  y: number;
  width: number;
  height: number;
};
export function Key(value: unknown): string;
export function Scale(value: number): number;
export function GetScreenWidth(): number;
export function GetScreenHeight(): number;
export function GetViewWidth(): number;
export function GetViewHeight(): number;
export function GetPageSidePadding(): number;
export function GetThemeBackground(): ColorValue;
export function GetThemeSurface(): ColorValue;
export function GetThemeText(): ColorValue;
export function ThemeDefaultLight(): ThemeValue;
export function ThemeDefaultDark(): ThemeValue;
export function SetTheme(theme: ThemeValue): void;
export function SetThemeFamily(family: ThemeFamilyValue): void;
export function GetThemeFamily(): ThemeFamilyValue | null;
export function GetTheme(): ThemeValue;
export function SetThemeMode(mode: number): void;
export function GetThemeMode(): number;
export function GetThemeButton(): ColorValue;
export function GetThemeButtonHover(): ColorValue;
export function GetThemeCircle(): ColorValue;
export function GetThemeIcon(): ColorValue;
export function GetThemeLink(): ColorValue;
export function SystemThemePrefersDark(): boolean;
export function SetPageTitle(title: string): void;
export function SetPageDescription(description: string): void;
export function SetPageCanonicalURL(url: string): void;
export function SetPageThemeColor(color: ColorValue | string): void;
export function GetRoutePath(): string;
export function GetRouteHash(): string;
export function GetRouteVersion(): number;
export function MatchRoute(pattern: string, path?: string): Record<string, string> | null;
export function RouteMatches(pattern: string, path?: string): boolean;
export function SetRouteParams(params?: Record<string, string>): Record<string, string>;
export function GetRouteParams(): Record<string, string>;
export function GetRouteParam(name: string): string;
export function PushRoute(path: string): string;
export function ReplaceRoute(path: string): string;
export function Fade(color: ColorValue, alpha: number): ColorValue;
export function DarkenColor(color: ColorValue, amount: number): ColorValue;
export function LightenColor(color: ColorValue, amount: number): ColorValue;
export function TextFormat(format: string, ...values: unknown[]): string;
export function GetUIClipboardTextValue(): string;
export function UpdateFileDialog(...args: unknown[]): number;
export function IsKeyPressed(key: number): boolean;
export function IsKeyDown(key: number): boolean;
export function IsMouseButtonReleased(button: number): boolean;
export function GetThemeMetrics(): Record<string, unknown>;
export function CanvasHitTest(canvas: unknown, screen: unknown): Record<string, unknown>;

export const Text8: number;
export const Text12: number;
export const Text14: number;
export const Text16: number;
export const Text18: number;
export const Text20: number;
export const Text24: number;
export const Text32: number;
export const Text48: number;
export const WHITE: ColorValue;
export const BLACK: ColorValue;
export const RAYWHITE: ColorValue;
export const BLANK: ColorValue;
export const LIGHTGRAY: ColorValue;
export const GRAY: ColorValue;
export const DARKGRAY: ColorValue;
export const YELLOW: ColorValue;
export const GOLD: ColorValue;
export const ORANGE: ColorValue;
export const PINK: ColorValue;
export const RED: ColorValue;
export const MAROON: ColorValue;
export const GREEN: ColorValue;
export const LIME: ColorValue;
export const DARKGREEN: ColorValue;
export const SKYBLUE: ColorValue;
export const BLUE: ColorValue;
export const DARKBLUE: ColorValue;
export const PURPLE: ColorValue;
export const VIOLET: ColorValue;
export const DARKPURPLE: ColorValue;
export const BEIGE: ColorValue;
export const BROWN: ColorValue;
export const DARKBROWN: ColorValue;
export const MAGENTA: ColorValue;
export const ButtonToneAccent: number;
export const ButtonToneNeutral: number;
export const ButtonToneDanger: number;
export const ButtonToneSuccess: number;
export const ButtonToneWarning: number;
export const ButtonEmphasisFilled: number;
export const ButtonEmphasisSoft: number;
export const ButtonEmphasisOutline: number;
export const ButtonEmphasisGhost: number;
export const ButtonEmphasisLink: number;
export const ButtonStateAuto: number;
export const ControlSizeMedium: number;
export const ControlSizeSmall: number;
export const ControlSizeLarge: number;
export const IconPlacementLeading: number;
export const IconPlacementTrailing: number;
export const StyleBackground: number;
export const StyleForeground: number;
export const StyleBorder: number;
export const StyleFocus: number;
export const StyleRadius: number;
export const StyleBorderWidth: number;
export const StyleOpacity: number;
export const StylePaddingX: number;
export const StylePaddingY: number;
export const StyleGap: number;
export const StyleFontSize: number;
export const StyleIconSize: number;
export const StyleContentOffset: number;
export const StyleBackgroundEnd: number;
export const StyleMaterial: number;
export const StyleTypeface: number;
export const MaterialLightfield: number;
export const MaterialFlat: number;
export const ButtonStateNormal: number;
export const ButtonStateHover: number;
export const ButtonStatePressed: number;
export const ButtonStateFocus: number;
export const ButtonStateDisabled: number;
export const ButtonStateLoading: number;
export const ButtonStateSelected: number;
export const KeyTab: number;
export const KeyBackspace: number;
export const KeyRight: number;
export const KeyLeft: number;
export const KeyC: number;
export const MouseButtonLeft: number;
export const KEY_SPACE: number;
export const KEY_C: number;
export const KEY_TAB: number;
export const KEY_BACKSPACE: number;
export const KEY_RIGHT: number;
export const KEY_LEFT: number;
export const KEY_DOWN: number;
export const KEY_UP: number;
export const MOUSE_BUTTON_LEFT: number;
export const THEME_SKY: number;
export const THEME_COUNT: number;
export const THEME_MODE_SYSTEM: number;
export const THEME_MODE_LIGHT: number;
export const THEME_MODE_DARK: number;
export const THEME_SOURCE_SYSTEM: number;
export const THEME_SOURCE_APP: number;
export function SetFancyEffectsEnabled(enabled: unknown): void;
export function FancyEffectsEnabled(): number;

export function AppBackground(...args: unknown[]): unknown;
export function Background(...args: unknown[]): unknown;
export function Bevel(...args: unknown[]): unknown;
export function Bullet(...args: unknown[]): unknown;
export function Button(...args: unknown[]): unknown;
export function Card(...args: unknown[]): unknown;
export function Canvas(canvas: unknown): Record<string, unknown>;
export function CanvasGrid(...args: unknown[]): unknown;
export function Checkbox(...args: unknown[]): unknown;
export function Collapsible(...args: unknown[]): unknown;
export function ColorPicker(...args: unknown[]): unknown;
export function Column(...args: unknown[]): unknown;
export function Drag(...args: unknown[]): unknown;
export function DragDrop(...args: unknown[]): unknown;
export function Dropdown(...args: unknown[]): unknown;
export function Input(...args: unknown[]): unknown;
export function SegmentedControl(...args: unknown[]): unknown;
export function Icon(...args: unknown[]): unknown;
export function Fieldset(...args: unknown[]): unknown;
export function Line(...args: unknown[]): unknown;
export function Link(...args: unknown[]): unknown;
export function ListBox(...args: unknown[]): unknown;
export function Menu(...args: unknown[]): unknown;
export function Flow(...args: unknown[]): unknown;
export function Grid(...args: unknown[]): unknown;
export function Heading(...args: unknown[]): unknown;
export function Modal(...args: unknown[]): unknown;
export function NavigationBar(...args: unknown[]): unknown;
export function Page(...args: unknown[]): unknown;
export function PanedView(...args: unknown[]): unknown;
export function Paragraph(...args: unknown[]): unknown;
export function ParagraphText(...args: unknown[]): unknown;
export function Image(...args: unknown[]): unknown;
export function Plot(...args: unknown[]): unknown;
export function Progress(...args: unknown[]): unknown;
export function Radio(...args: unknown[]): unknown;
export function Box(...args: unknown[]): unknown;
export function Row(...args: unknown[]): unknown;
export function Screen(...args: unknown[]): unknown;
export function Scroll(...args: unknown[]): unknown;
export function Selectable(...args: unknown[]): unknown;
export function Separator(...args: unknown[]): unknown;
export function SetCurrentTheme(...args: unknown[]): unknown;
export function Section(...args: unknown[]): unknown;
export function Toast(...args: unknown[]): unknown;
export function Slider(...args: unknown[]): unknown;
export function Spinbox(...args: unknown[]): unknown;
export function Stack(...args: unknown[]): unknown;
export function TabBar(...args: unknown[]): unknown;
export function TableView(...args: unknown[]): unknown;
export function Text(...args: unknown[]): unknown;
export function TextArea(...args: unknown[]): unknown;
export function TextField(...args: unknown[]): unknown;
export function TitleBar(...args: unknown[]): unknown;
export function TreeView(...args: unknown[]): unknown;
export function Toggle(...args: unknown[]): unknown;
export function Toolbar(...args: unknown[]): unknown;
