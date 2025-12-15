/**
 * KRB Bytecode Loader
 * Parses .krb binary files into KRBModule structures
 */

import { Opcode, SectionType } from './opcodes';

// ============================================================================
// Constants
// ============================================================================

export const KRB_MAGIC = 0x5942524B; // "KRBY" in little-endian
export const KRB_VERSION_MAJOR = 1;
export const KRB_VERSION_MINOR = 0;

// Header sizes (in bytes)
const HEADER_SIZE = 32;
const SECTION_HEADER_SIZE = 16; // type:1 + flags:1 + reserved:2 + offset:4 + size:4 + uncompressed:4

// ============================================================================
// Types
// ============================================================================

export interface KRBHeader {
  magic: number;
  versionMajor: number;
  versionMinor: number;
  flags: number;
  sectionCount: number;
  entryFunction: number;
  checksum: number;
}

export interface KRBSectionHeader {
  type: SectionType;
  flags: number;
  offset: number;
  size: number;
  uncompressedSize: number;
}

export interface KRBFunction {
  name: string;
  codeOffset: number;
  codeSize: number;
  paramCount: number;
  localCount: number;
  flags: number;
}

export interface KRBEventBinding {
  componentId: number;
  eventType: number;
  functionIndex: number;
}

export interface KRBModule {
  header: KRBHeader;

  // Raw section data
  uiData: Uint8Array | null;
  codeData: Uint8Array | null;
  dataData: Uint8Array | null;

  // Parsed tables
  stringTable: string[];
  functions: KRBFunction[];
  eventBindings: KRBEventBinding[];

  // Global state (initialized from DATA section)
  globals: Map<number, KRBValue>;
}

export type KRBValueType = 'null' | 'bool' | 'int' | 'float' | 'string' | 'array';

export interface KRBValue {
  type: KRBValueType;
  value: null | boolean | number | string | KRBValue[];
}

// ============================================================================
// DataView helpers
// ============================================================================

class BinaryReader {
  private view: DataView;
  private pos: number = 0;

  constructor(buffer: ArrayBuffer) {
    this.view = new DataView(buffer);
  }

  get position(): number {
    return this.pos;
  }

  seek(offset: number): void {
    this.pos = offset;
  }

  readUint8(): number {
    const val = this.view.getUint8(this.pos);
    this.pos += 1;
    return val;
  }

  readUint16(): number {
    const val = this.view.getUint16(this.pos, true); // little-endian
    this.pos += 2;
    return val;
  }

  readUint32(): number {
    const val = this.view.getUint32(this.pos, true);
    this.pos += 4;
    return val;
  }

  readInt8(): number {
    const val = this.view.getInt8(this.pos);
    this.pos += 1;
    return val;
  }

  readInt16(): number {
    const val = this.view.getInt16(this.pos, true);
    this.pos += 2;
    return val;
  }

  readInt32(): number {
    const val = this.view.getInt32(this.pos, true);
    this.pos += 4;
    return val;
  }

  readInt64(): bigint {
    const lo = this.view.getUint32(this.pos, true);
    const hi = this.view.getInt32(this.pos + 4, true);
    this.pos += 8;
    return BigInt(hi) << 32n | BigInt(lo);
  }

  readFloat32(): number {
    const val = this.view.getFloat32(this.pos, true);
    this.pos += 4;
    return val;
  }

  readFloat64(): number {
    const val = this.view.getFloat64(this.pos, true);
    this.pos += 8;
    return val;
  }

  readBytes(count: number): Uint8Array {
    const bytes = new Uint8Array(this.view.buffer, this.pos, count);
    this.pos += count;
    return bytes.slice(); // Return copy
  }

  readString(): string {
    // Strings are length-prefixed (uint16 length + bytes)
    const len = this.readUint16();
    const bytes = this.readBytes(len);
    return new TextDecoder().decode(bytes);
  }

  readCString(): string {
    // Read null-terminated string
    const start = this.pos;
    while (this.view.getUint8(this.pos) !== 0) {
      this.pos++;
    }
    const bytes = new Uint8Array(this.view.buffer, start, this.pos - start);
    this.pos++; // Skip null terminator
    return new TextDecoder().decode(bytes);
  }

  skip(count: number): void {
    this.pos += count;
  }
}

// ============================================================================
// Loader
// ============================================================================

export function parseKRB(buffer: ArrayBuffer, debug = false): KRBModule {
  const reader = new BinaryReader(buffer);

  // Read header
  const header = readHeader(reader);

  if (debug) {
    console.log('[KRB] Header parsed, position:', reader.position);
  }

  // Validate magic
  if (header.magic !== KRB_MAGIC) {
    throw new Error(`Invalid KRB magic: expected 0x${KRB_MAGIC.toString(16)}, got 0x${header.magic.toString(16)}`);
  }

  // Validate version
  if (header.versionMajor > KRB_VERSION_MAJOR) {
    throw new Error(`Unsupported KRB version: ${header.versionMajor}.${header.versionMinor}`);
  }

  // Skip reserved bytes in header
  reader.skip(12);

  if (debug) {
    console.log('[KRB] After skipping reserved, position:', reader.position);
  }

  // Read section headers
  const sectionHeaders: KRBSectionHeader[] = [];
  for (let i = 0; i < header.sectionCount; i++) {
    const sh = readSectionHeader(reader);
    if (debug) {
      console.log(`[KRB] Section ${i}: type=${sh.type}, offset=${sh.offset}, size=${sh.size}`);
    }
    sectionHeaders.push(sh);
  }

  // Initialize module
  const module: KRBModule = {
    header,
    uiData: null,
    codeData: null,
    dataData: null,
    stringTable: [],
    functions: [],
    eventBindings: [],
    globals: new Map(),
  };

  // Read each section
  for (const section of sectionHeaders) {
    reader.seek(section.offset);

    switch (section.type) {
      case SectionType.UI:
        module.uiData = reader.readBytes(section.size);
        break;

      case SectionType.CODE:
        module.codeData = reader.readBytes(section.size);
        break;

      case SectionType.DATA:
        module.dataData = reader.readBytes(section.size);
        break;

      case SectionType.STRINGS:
        module.stringTable = readStringTable(reader, section.size);
        break;

      case SectionType.FUNCS:
        module.functions = readFunctionTable(reader, section.size, module.stringTable);
        break;

      case SectionType.EVENTS:
        module.eventBindings = readEventBindings(reader, section.size);
        break;

      case SectionType.META:
        // Skip metadata for now
        break;
    }
  }

  return module;
}

function readHeader(reader: BinaryReader): KRBHeader {
  return {
    magic: reader.readUint32(),
    versionMajor: reader.readUint8(),
    versionMinor: reader.readUint8(),
    flags: reader.readUint16(),
    sectionCount: reader.readUint32(),
    entryFunction: reader.readUint32(),
    checksum: reader.readUint32(),
    // Skip reserved bytes
  };
}

function readSectionHeader(reader: BinaryReader): KRBSectionHeader {
  return {
    type: reader.readUint8() as SectionType,
    flags: reader.readUint8(),
    // Skip reserved
    offset: (reader.skip(2), reader.readUint32()),
    size: reader.readUint32(),
    uncompressedSize: reader.readUint32(),
  };
}

function readStringTable(reader: BinaryReader, size: number): string[] {
  const strings: string[] = [];
  const endPos = reader.position + size;

  // String table format: count (uint32) + [length-prefixed strings]
  const count = reader.readUint32();

  for (let i = 0; i < count && reader.position < endPos; i++) {
    strings.push(reader.readString());
  }

  return strings;
}

function readFunctionTable(reader: BinaryReader, size: number, stringTable: string[]): KRBFunction[] {
  const functions: KRBFunction[] = [];
  const endPos = reader.position + size;

  // Function table format: count (uint32) + [function entries]
  const count = reader.readUint32();

  for (let i = 0; i < count && reader.position < endPos; i++) {
    const nameIndex = reader.readUint16();
    const codeOffset = reader.readUint32();
    const codeSize = reader.readUint32();
    const paramCount = reader.readUint8();
    const localCount = reader.readUint8();
    const flags = reader.readUint16();

    functions.push({
      name: stringTable[nameIndex] || `func_${i}`,
      codeOffset,
      codeSize,
      paramCount,
      localCount,
      flags,
    });
  }

  return functions;
}

function readEventBindings(reader: BinaryReader, size: number): KRBEventBinding[] {
  const bindings: KRBEventBinding[] = [];
  const endPos = reader.position + size;

  // Event bindings format: count (uint32) + [binding entries]
  const count = reader.readUint32();

  for (let i = 0; i < count && reader.position < endPos; i++) {
    bindings.push({
      componentId: reader.readUint32(),
      eventType: reader.readUint16(),
      functionIndex: reader.readUint16(),
    });
  }

  return bindings;
}

// ============================================================================
// Async loader (for browser fetch)
// ============================================================================

export async function loadKRB(url: string): Promise<KRBModule> {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to load KRB file: ${response.status} ${response.statusText}`);
  }
  const buffer = await response.arrayBuffer();
  return parseKRB(buffer);
}
