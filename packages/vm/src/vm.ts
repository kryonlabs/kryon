/**
 * Kryon Virtual Machine
 * Stack-based bytecode interpreter for executing .krb files
 */

import { Opcode, PropertyID, OPCODE_NAMES } from './opcodes';
import { KRBModule, KRBValue, KRBValueType, KRBFunction, parseKRB, loadKRB } from './loader';

// ============================================================================
// Types
// ============================================================================

export interface CallFrame {
  functionIndex: number;
  returnAddress: number;
  stackBase: number;
  locals: KRBValue[];
}

export interface VMOptions {
  maxStackSize?: number;
  maxCallDepth?: number;
  debug?: boolean;
}

export type ComponentRef = number; // Component ID

export interface RendererInterface {
  // Get component reference by ID
  getComponent(id: number): ComponentRef | null;

  // Set property on component
  setProperty(comp: ComponentRef, propId: PropertyID, value: KRBValue): void;

  // Get property from component
  getProperty(comp: ComponentRef, propId: PropertyID): KRBValue;

  // Set text content
  setText(comp: ComponentRef, text: string): void;

  // Set visibility
  setVisible(comp: ComponentRef, visible: boolean): void;

  // Add/remove children
  addChild(parent: ComponentRef, child: ComponentRef): void;
  removeChild(parent: ComponentRef, child: ComponentRef): void;

  // Request redraw
  redraw(): void;
}

// ============================================================================
// VM Implementation
// ============================================================================

export class KryonVM {
  private module: KRBModule | null = null;
  private codeView: DataView | null = null;

  // Execution state
  private ip: number = 0;
  private stack: KRBValue[] = [];
  private callStack: CallFrame[] = [];
  private halted: boolean = false;

  // Configuration
  private maxStackSize: number;
  private maxCallDepth: number;
  private debug: boolean;

  // Renderer interface
  private renderer: RendererInterface | null = null;

  // Error state
  private error: string | null = null;

  constructor(options: VMOptions = {}) {
    this.maxStackSize = options.maxStackSize ?? 1024;
    this.maxCallDepth = options.maxCallDepth ?? 256;
    this.debug = options.debug ?? false;
  }

  // ============================================================================
  // Public API
  // ============================================================================

  /**
   * Load a .krb module from ArrayBuffer
   */
  load(buffer: ArrayBuffer): void {
    this.module = parseKRB(buffer);
    this.reset();
  }

  /**
   * Load a .krb module from URL
   */
  async loadFromUrl(url: string): Promise<void> {
    this.module = await loadKRB(url);
    this.reset();
  }

  /**
   * Set the renderer interface
   */
  setRenderer(renderer: RendererInterface): void {
    this.renderer = renderer;
  }

  /**
   * Reset VM state
   */
  reset(): void {
    this.ip = 0;
    this.stack = [];
    this.callStack = [];
    this.halted = false;
    this.error = null;

    if (this.module?.codeData) {
      this.codeView = new DataView(this.module.codeData.buffer);
    }
  }

  /**
   * Dispatch an event to the appropriate handler
   */
  dispatchEvent(componentId: number, eventType: number): void {
    if (!this.module) return;

    // Find matching event binding
    const binding = this.module.eventBindings.find(
      (b) => b.componentId === componentId && b.eventType === eventType
    );

    if (binding) {
      // Reset halted state for new event dispatch
      this.halted = false;
      this.error = null;
      this.callFunction(binding.functionIndex);
      this.run();
    }
  }

  /**
   * Execute a single bytecode instruction
   * Returns false if halted or error
   */
  step(): boolean {
    if (this.halted || this.error || !this.codeView || !this.module) {
      return false;
    }

    const opcode = this.readUint8() as Opcode;

    if (this.debug) {
      console.log(`[${this.ip - 1}] ${OPCODE_NAMES[opcode] || `0x${opcode.toString(16)}`}`);
    }

    try {
      this.executeOpcode(opcode);
    } catch (e) {
      this.error = e instanceof Error ? e.message : String(e);
      return false;
    }

    return !this.halted;
  }

  /**
   * Run until halted or error
   */
  run(): void {
    while (this.step()) {
      // Continue executing
    }
  }

  /**
   * Get a global variable value
   */
  getGlobal(index: number): KRBValue | undefined {
    return this.module?.globals.get(index);
  }

  /**
   * Set a global variable value
   */
  setGlobal(index: number, value: KRBValue): void {
    this.module?.globals.set(index, value);
  }

  /**
   * Get error message if any
   */
  getError(): string | null {
    return this.error;
  }

  /**
   * Check if VM is halted
   */
  isHalted(): boolean {
    return this.halted;
  }

  /**
   * Get the loaded module
   */
  getModule(): KRBModule | null {
    return this.module;
  }

  // ============================================================================
  // Bytecode reading
  // ============================================================================

  private readUint8(): number {
    const val = this.codeView!.getUint8(this.ip);
    this.ip += 1;
    return val;
  }

  private readInt8(): number {
    const val = this.codeView!.getInt8(this.ip);
    this.ip += 1;
    return val;
  }

  private readUint16(): number {
    const val = this.codeView!.getUint16(this.ip, true);
    this.ip += 2;
    return val;
  }

  private readInt16(): number {
    const val = this.codeView!.getInt16(this.ip, true);
    this.ip += 2;
    return val;
  }

  private readUint32(): number {
    const val = this.codeView!.getUint32(this.ip, true);
    this.ip += 4;
    return val;
  }

  private readInt32(): number {
    const val = this.codeView!.getInt32(this.ip, true);
    this.ip += 4;
    return val;
  }

  private readFloat32(): number {
    const val = this.codeView!.getFloat32(this.ip, true);
    this.ip += 4;
    return val;
  }

  private readFloat64(): number {
    const val = this.codeView!.getFloat64(this.ip, true);
    this.ip += 8;
    return val;
  }

  // ============================================================================
  // Stack operations
  // ============================================================================

  private push(value: KRBValue): void {
    if (this.stack.length >= this.maxStackSize) {
      throw new Error('Stack overflow');
    }
    this.stack.push(value);
  }

  private pop(): KRBValue {
    if (this.stack.length === 0) {
      throw new Error('Stack underflow');
    }
    return this.stack.pop()!;
  }

  private peek(): KRBValue {
    if (this.stack.length === 0) {
      throw new Error('Stack underflow');
    }
    return this.stack[this.stack.length - 1];
  }

  // ============================================================================
  // Value helpers
  // ============================================================================

  private nullVal(): KRBValue {
    return { type: 'null', value: null };
  }

  private boolVal(b: boolean): KRBValue {
    return { type: 'bool', value: b };
  }

  private intVal(i: number): KRBValue {
    return { type: 'int', value: i };
  }

  private floatVal(f: number): KRBValue {
    return { type: 'float', value: f };
  }

  private strVal(s: string): KRBValue {
    return { type: 'string', value: s };
  }

  private arrVal(arr: KRBValue[]): KRBValue {
    return { type: 'array', value: arr };
  }

  private toNumber(v: KRBValue): number {
    switch (v.type) {
      case 'int':
      case 'float':
        return v.value as number;
      case 'bool':
        return v.value ? 1 : 0;
      case 'string':
        return parseFloat(v.value as string) || 0;
      default:
        return 0;
    }
  }

  private toBool(v: KRBValue): boolean {
    switch (v.type) {
      case 'bool':
        return v.value as boolean;
      case 'int':
      case 'float':
        return (v.value as number) !== 0;
      case 'string':
        return (v.value as string).length > 0;
      case 'array':
        return (v.value as KRBValue[]).length > 0;
      case 'null':
        return false;
      default:
        return false;
    }
  }

  private toString(v: KRBValue): string {
    switch (v.type) {
      case 'string':
        return v.value as string;
      case 'int':
      case 'float':
        return String(v.value);
      case 'bool':
        return v.value ? 'true' : 'false';
      case 'null':
        return 'null';
      case 'array':
        return '[array]';
      default:
        return '';
    }
  }

  // ============================================================================
  // Function calls
  // ============================================================================

  private callFunction(functionIndex: number): void {
    if (!this.module) return;

    const func = this.module.functions[functionIndex];
    if (!func) {
      throw new Error(`Invalid function index: ${functionIndex}`);
    }

    if (this.callStack.length >= this.maxCallDepth) {
      throw new Error('Call stack overflow');
    }

    // Create new call frame
    const frame: CallFrame = {
      functionIndex,
      returnAddress: this.ip,
      stackBase: this.stack.length - func.paramCount,
      locals: new Array(func.localCount).fill(null).map(() => this.nullVal()),
    };

    // Pop parameters into locals
    for (let i = func.paramCount - 1; i >= 0; i--) {
      frame.locals[i] = this.pop();
    }

    this.callStack.push(frame);
    this.ip = func.codeOffset;
  }

  private returnFromFunction(hasValue: boolean): void {
    const frame = this.callStack.pop();
    if (!frame) {
      this.halted = true;
      return;
    }

    const returnValue = hasValue ? this.pop() : null;

    // Restore stack to base
    this.stack.length = frame.stackBase;

    // Push return value if any
    if (returnValue) {
      this.push(returnValue);
    }

    this.ip = frame.returnAddress;

    // If we've returned from the top-level call (call stack is now empty), halt
    if (this.callStack.length === 0) {
      this.halted = true;
    }
  }

  private getCurrentFrame(): CallFrame | undefined {
    return this.callStack[this.callStack.length - 1];
  }

  // ============================================================================
  // Opcode execution
  // ============================================================================

  private executeOpcode(opcode: Opcode): void {
    switch (opcode) {
      // ========== Stack Operations ==========
      case Opcode.NOP:
        break;

      case Opcode.PUSH_NULL:
        this.push(this.nullVal());
        break;

      case Opcode.PUSH_TRUE:
        this.push(this.boolVal(true));
        break;

      case Opcode.PUSH_FALSE:
        this.push(this.boolVal(false));
        break;

      case Opcode.PUSH_INT8:
        this.push(this.intVal(this.readInt8()));
        break;

      case Opcode.PUSH_INT16:
        this.push(this.intVal(this.readInt16()));
        break;

      case Opcode.PUSH_INT32:
        this.push(this.intVal(this.readInt32()));
        break;

      case Opcode.PUSH_INT64: {
        // JavaScript doesn't have native int64, read as two 32-bit parts
        const lo = this.readUint32();
        const hi = this.readInt32();
        // Approximate as float for large values
        this.push(this.intVal(hi * 0x100000000 + lo));
        break;
      }

      case Opcode.PUSH_FLOAT:
        this.push(this.floatVal(this.readFloat32()));
        break;

      case Opcode.PUSH_DOUBLE:
        this.push(this.floatVal(this.readFloat64()));
        break;

      case Opcode.PUSH_STR: {
        const index = this.readUint16();
        const str = this.module?.stringTable[index] ?? '';
        this.push(this.strVal(str));
        break;
      }

      case Opcode.POP:
        this.pop();
        break;

      case Opcode.DUP:
        this.push({ ...this.peek() });
        break;

      case Opcode.SWAP: {
        const a = this.pop();
        const b = this.pop();
        this.push(a);
        this.push(b);
        break;
      }

      // ========== Variables ==========
      case Opcode.LOAD_LOCAL: {
        const index = this.readUint8();
        const frame = this.getCurrentFrame();
        if (frame && frame.locals[index]) {
          this.push({ ...frame.locals[index] });
        } else {
          this.push(this.nullVal());
        }
        break;
      }

      case Opcode.STORE_LOCAL: {
        const index = this.readUint8();
        const value = this.pop();
        const frame = this.getCurrentFrame();
        if (frame) {
          frame.locals[index] = value;
        }
        break;
      }

      case Opcode.LOAD_GLOBAL: {
        const index = this.readUint16();
        const value = this.module?.globals.get(index);
        this.push(value ? { ...value } : this.nullVal());
        break;
      }

      case Opcode.STORE_GLOBAL: {
        const index = this.readUint16();
        const value = this.pop();
        this.module?.globals.set(index, value);
        break;
      }

      // ========== Arithmetic ==========
      case Opcode.ADD: {
        const b = this.pop();
        const a = this.pop();
        if (a.type === 'string' || b.type === 'string') {
          this.push(this.strVal(this.toString(a) + this.toString(b)));
        } else {
          this.push(this.floatVal(this.toNumber(a) + this.toNumber(b)));
        }
        break;
      }

      case Opcode.SUB: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.floatVal(this.toNumber(a) - this.toNumber(b)));
        break;
      }

      case Opcode.MUL: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.floatVal(this.toNumber(a) * this.toNumber(b)));
        break;
      }

      case Opcode.DIV: {
        const b = this.pop();
        const a = this.pop();
        const divisor = this.toNumber(b);
        if (divisor === 0) {
          throw new Error('Division by zero');
        }
        this.push(this.floatVal(this.toNumber(a) / divisor));
        break;
      }

      case Opcode.MOD: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.floatVal(this.toNumber(a) % this.toNumber(b)));
        break;
      }

      case Opcode.NEG: {
        const a = this.pop();
        this.push(this.floatVal(-this.toNumber(a)));
        break;
      }

      case Opcode.INC: {
        const a = this.pop();
        this.push(this.floatVal(this.toNumber(a) + 1));
        break;
      }

      case Opcode.DEC: {
        const a = this.pop();
        this.push(this.floatVal(this.toNumber(a) - 1));
        break;
      }

      // ========== Comparison ==========
      case Opcode.EQ: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(this.compareEqual(a, b)));
        break;
      }

      case Opcode.NE: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(!this.compareEqual(a, b)));
        break;
      }

      case Opcode.LT: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(this.toNumber(a) < this.toNumber(b)));
        break;
      }

      case Opcode.LE: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(this.toNumber(a) <= this.toNumber(b)));
        break;
      }

      case Opcode.GT: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(this.toNumber(a) > this.toNumber(b)));
        break;
      }

      case Opcode.GE: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(this.toNumber(a) >= this.toNumber(b)));
        break;
      }

      // ========== Logic ==========
      case Opcode.AND: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(this.toBool(a) && this.toBool(b)));
        break;
      }

      case Opcode.OR: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.boolVal(this.toBool(a) || this.toBool(b)));
        break;
      }

      case Opcode.NOT: {
        const a = this.pop();
        this.push(this.boolVal(!this.toBool(a)));
        break;
      }

      // ========== Bitwise ==========
      case Opcode.BAND: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.intVal(this.toNumber(a) & this.toNumber(b)));
        break;
      }

      case Opcode.BOR: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.intVal(this.toNumber(a) | this.toNumber(b)));
        break;
      }

      case Opcode.BXOR: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.intVal(this.toNumber(a) ^ this.toNumber(b)));
        break;
      }

      case Opcode.BNOT: {
        const a = this.pop();
        this.push(this.intVal(~this.toNumber(a)));
        break;
      }

      case Opcode.SHL: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.intVal(this.toNumber(a) << this.toNumber(b)));
        break;
      }

      case Opcode.SHR: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.intVal(this.toNumber(a) >> this.toNumber(b)));
        break;
      }

      // ========== Control Flow ==========
      case Opcode.JMP: {
        const offset = this.readInt16();
        this.ip += offset - 2; // -2 because we already read the offset
        break;
      }

      case Opcode.JMP_IF: {
        const offset = this.readInt16();
        const cond = this.pop();
        if (this.toBool(cond)) {
          this.ip += offset - 2;
        }
        break;
      }

      case Opcode.JMP_IF_NOT: {
        const offset = this.readInt16();
        const cond = this.pop();
        if (!this.toBool(cond)) {
          this.ip += offset - 2;
        }
        break;
      }

      case Opcode.CALL: {
        const funcIndex = this.readUint16();
        this.callFunction(funcIndex);
        break;
      }

      case Opcode.CALL_NATIVE: {
        const nativeIndex = this.readUint16();
        this.callNative(nativeIndex);
        break;
      }

      case Opcode.RET:
        this.returnFromFunction(false);
        break;

      case Opcode.RET_VAL:
        this.returnFromFunction(true);
        break;

      // ========== UI Operations ==========
      case Opcode.GET_COMP: {
        const compId = this.readUint32();
        // Push component reference onto stack
        this.push(this.intVal(compId));
        break;
      }

      case Opcode.SET_PROP: {
        const propId = this.readUint16() as PropertyID;
        const value = this.pop();
        const compId = this.pop();
        if (this.renderer) {
          const comp = this.renderer.getComponent(this.toNumber(compId));
          if (comp !== null) {
            this.renderer.setProperty(comp, propId, value);
          }
        }
        break;
      }

      case Opcode.GET_PROP: {
        const propId = this.readUint16() as PropertyID;
        const compId = this.pop();
        if (this.renderer) {
          const comp = this.renderer.getComponent(this.toNumber(compId));
          if (comp !== null) {
            this.push(this.renderer.getProperty(comp, propId));
          } else {
            this.push(this.nullVal());
          }
        } else {
          this.push(this.nullVal());
        }
        break;
      }

      case Opcode.SET_TEXT: {
        const text = this.pop();
        const compId = this.pop();
        if (this.renderer) {
          const comp = this.renderer.getComponent(this.toNumber(compId));
          if (comp !== null) {
            this.renderer.setText(comp, this.toString(text));
          }
        }
        break;
      }

      case Opcode.SET_VISIBLE: {
        const visible = this.pop();
        const compId = this.pop();
        if (this.renderer) {
          const comp = this.renderer.getComponent(this.toNumber(compId));
          if (comp !== null) {
            this.renderer.setVisible(comp, this.toBool(visible));
          }
        }
        break;
      }

      case Opcode.ADD_CHILD: {
        const childId = this.pop();
        const parentId = this.pop();
        if (this.renderer) {
          const parent = this.renderer.getComponent(this.toNumber(parentId));
          const child = this.renderer.getComponent(this.toNumber(childId));
          if (parent !== null && child !== null) {
            this.renderer.addChild(parent, child);
          }
        }
        break;
      }

      case Opcode.REMOVE_CHILD: {
        const childId = this.pop();
        const parentId = this.pop();
        if (this.renderer) {
          const parent = this.renderer.getComponent(this.toNumber(parentId));
          const child = this.renderer.getComponent(this.toNumber(childId));
          if (parent !== null && child !== null) {
            this.renderer.removeChild(parent, child);
          }
        }
        break;
      }

      case Opcode.REDRAW:
        if (this.renderer) {
          this.renderer.redraw();
        }
        break;

      // ========== String Operations ==========
      case Opcode.STR_CONCAT: {
        const b = this.pop();
        const a = this.pop();
        this.push(this.strVal(this.toString(a) + this.toString(b)));
        break;
      }

      case Opcode.STR_LEN: {
        const s = this.pop();
        this.push(this.intVal(this.toString(s).length));
        break;
      }

      case Opcode.STR_SUBSTR: {
        const len = this.toNumber(this.pop());
        const start = this.toNumber(this.pop());
        const s = this.toString(this.pop());
        this.push(this.strVal(s.substring(start, start + len)));
        break;
      }

      case Opcode.STR_FORMAT: {
        // Format string with arguments
        // Stack: format_string, arg_count, args...
        const argCount = this.toNumber(this.pop());
        const args: KRBValue[] = [];
        for (let i = 0; i < argCount; i++) {
          args.unshift(this.pop());
        }
        const format = this.toString(this.pop());
        // Simple format: replace {} with args
        let result = format;
        for (const arg of args) {
          result = result.replace('{}', this.toString(arg));
        }
        this.push(this.strVal(result));
        break;
      }

      // ========== Array Operations ==========
      case Opcode.ARR_NEW: {
        const size = this.readUint8();
        const arr: KRBValue[] = new Array(size).fill(null).map(() => this.nullVal());
        this.push(this.arrVal(arr));
        break;
      }

      case Opcode.ARR_GET: {
        const index = this.toNumber(this.pop());
        const arr = this.pop();
        if (arr.type === 'array') {
          const a = arr.value as KRBValue[];
          this.push(a[index] ? { ...a[index] } : this.nullVal());
        } else {
          this.push(this.nullVal());
        }
        break;
      }

      case Opcode.ARR_SET: {
        const value = this.pop();
        const index = this.toNumber(this.pop());
        const arr = this.pop();
        if (arr.type === 'array') {
          (arr.value as KRBValue[])[index] = value;
        }
        break;
      }

      case Opcode.ARR_PUSH: {
        const value = this.pop();
        const arr = this.pop();
        if (arr.type === 'array') {
          (arr.value as KRBValue[]).push(value);
        }
        break;
      }

      case Opcode.ARR_POP: {
        const arr = this.pop();
        if (arr.type === 'array') {
          const a = arr.value as KRBValue[];
          this.push(a.pop() ?? this.nullVal());
        } else {
          this.push(this.nullVal());
        }
        break;
      }

      case Opcode.ARR_LEN: {
        const arr = this.pop();
        if (arr.type === 'array') {
          this.push(this.intVal((arr.value as KRBValue[]).length));
        } else {
          this.push(this.intVal(0));
        }
        break;
      }

      // ========== Debug ==========
      case Opcode.DEBUG_PRINT: {
        const val = this.pop();
        console.log('[DEBUG]', this.toString(val));
        break;
      }

      case Opcode.DEBUG_BREAK:
        // Set halted flag to pause execution
        this.halted = true;
        break;

      case Opcode.HALT:
        this.halted = true;
        break;

      default:
        throw new Error(`Unknown opcode: 0x${opcode.toString(16)}`);
    }
  }

  private compareEqual(a: KRBValue, b: KRBValue): boolean {
    if (a.type !== b.type) {
      // Type coercion for numbers
      if ((a.type === 'int' || a.type === 'float') && (b.type === 'int' || b.type === 'float')) {
        return this.toNumber(a) === this.toNumber(b);
      }
      return false;
    }

    switch (a.type) {
      case 'null':
        return true;
      case 'bool':
      case 'int':
      case 'float':
      case 'string':
        return a.value === b.value;
      case 'array':
        // Reference equality for arrays
        return a.value === b.value;
      default:
        return false;
    }
  }

  private callNative(nativeIndex: number): void {
    // Built-in native functions
    // For now, just console.log
    switch (nativeIndex) {
      case 0: // print
        const val = this.pop();
        console.log(this.toString(val));
        break;
      default:
        throw new Error(`Unknown native function: ${nativeIndex}`);
    }
  }
}
