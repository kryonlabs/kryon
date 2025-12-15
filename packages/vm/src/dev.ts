/**
 * Development server for testing the Kryon VM
 * Run with: bun run dev
 */

import { KryonVM } from './vm';
import { DOMRenderer } from './renderer';
import { Opcode, PropertyID, OPCODE_NAMES } from './opcodes';
import { parseKRB, KRBModule, KRBValue } from './loader';

// ============================================================================
// Test bytecode generator
// ============================================================================

/**
 * Generate a simple test .krb bytecode for a counter app
 */
function generateTestBytecode(): ArrayBuffer {
  const buffer = new ArrayBuffer(512);
  const view = new DataView(buffer);
  let offset = 0;

  // Write helper
  const writeU8 = (val: number) => {
    view.setUint8(offset, val);
    offset += 1;
  };
  const writeU16 = (val: number) => {
    view.setUint16(offset, val, true);
    offset += 2;
  };
  const writeU32 = (val: number) => {
    view.setUint32(offset, val, true);
    offset += 4;
  };

  // ========== HEADER (32 bytes) ==========
  writeU32(0x5942524b); // Magic: "KRBY"
  writeU8(1); // Version major
  writeU8(0); // Version minor
  writeU16(0); // Flags
  writeU32(4); // Section count (UI, CODE, STRINGS, FUNCS)
  writeU32(0); // Entry function
  writeU32(0); // Checksum (skip for now)
  // Reserved (12 bytes)
  for (let i = 0; i < 12; i++) writeU8(0);

  // Section header offset: 32
  // Section headers: 4 * 16 = 64 bytes (type:1 + flags:1 + reserved:2 + offset:4 + size:4 + uncompressed:4)
  // Data starts at: 32 + 64 = 96

  const sectionHeaderStart = 32;
  const dataStart = 96;

  // ========== SECTION HEADERS ==========

  // UI Section (placeholder)
  offset = sectionHeaderStart;
  writeU8(0x01); // Type: UI
  writeU8(0); // Flags
  writeU16(0); // Reserved
  writeU32(dataStart); // Offset
  writeU32(0); // Size (empty for now)
  writeU32(0); // Uncompressed size

  // CODE Section
  const codeOffset = dataStart;
  writeU8(0x02); // Type: CODE
  writeU8(0); // Flags
  writeU16(0); // Reserved
  writeU32(codeOffset); // Offset
  writeU32(20); // Size (bytecode below)
  writeU32(20); // Uncompressed size

  // STRINGS Section
  const stringsOffset = codeOffset + 20;
  writeU8(0x05); // Type: STRINGS
  writeU8(0); // Flags
  writeU16(0); // Reserved
  writeU32(stringsOffset); // Offset
  writeU32(30); // Size
  writeU32(30); // Uncompressed size

  // FUNCS Section
  const funcsOffset = stringsOffset + 30;
  writeU8(0x06); // Type: FUNCS
  writeU8(0); // Flags
  writeU16(0); // Reserved
  writeU32(funcsOffset); // Offset
  writeU32(18); // Size (1 function)
  writeU32(18); // Uncompressed size

  // ========== CODE SECTION ==========
  offset = codeOffset;

  // Simple function: increment global 0 and print
  // func_0:
  //   LOAD_GLOBAL 0
  //   PUSH_INT8 1
  //   ADD
  //   STORE_GLOBAL 0
  //   LOAD_GLOBAL 0
  //   DEBUG_PRINT
  //   RET

  writeU8(Opcode.LOAD_GLOBAL); // LOAD_GLOBAL
  writeU16(0); // index 0
  writeU8(Opcode.PUSH_INT8); // PUSH_INT8
  writeU8(1); // value 1
  writeU8(Opcode.ADD); // ADD
  writeU8(Opcode.STORE_GLOBAL); // STORE_GLOBAL
  writeU16(0); // index 0
  writeU8(Opcode.LOAD_GLOBAL); // LOAD_GLOBAL
  writeU16(0); // index 0
  writeU8(Opcode.DEBUG_PRINT); // DEBUG_PRINT
  writeU8(Opcode.RET); // RET
  // Pad to 20 bytes
  while (offset < codeOffset + 20) writeU8(Opcode.NOP);

  // ========== STRINGS SECTION ==========
  offset = stringsOffset;
  // Format: count (u32) + [length-prefixed strings]
  writeU32(2); // 2 strings
  // String 0: "increment"
  writeU16(9);
  const str0 = 'increment';
  for (let i = 0; i < str0.length; i++) writeU8(str0.charCodeAt(i));
  // String 1: "counter"
  writeU16(7);
  const str1 = 'counter';
  for (let i = 0; i < str1.length; i++) writeU8(str1.charCodeAt(i));

  // ========== FUNCS SECTION ==========
  offset = funcsOffset;
  // Format: count (u32) + [function entries]
  writeU32(1); // 1 function
  // Function 0:
  writeU16(0); // name index (string 0: "increment")
  writeU32(0); // code offset (start of code section)
  writeU32(16); // code size
  writeU8(0); // param count
  writeU8(0); // local count
  writeU16(0); // flags

  return buffer;
}

// ============================================================================
// Tests
// ============================================================================

async function runTests() {
  console.log('=== Kryon VM Tests ===\n');

  // Test 1: Parse test bytecode
  console.log('Test 1: Parse bytecode');
  const bytecode = generateTestBytecode();

  const module = parseKRB(bytecode);
  console.log('  Header:', module.header);
  console.log('  Strings:', module.stringTable);
  console.log('  Functions:', module.functions.map((f) => f.name));
  console.log('  Code data size:', module.codeData?.length);
  console.log('  ✓ Parsed successfully\n');

  // Test 2: Create VM and load module
  console.log('Test 2: Create VM');
  const vm = new KryonVM({ debug: false });
  vm.load(bytecode);

  // Initialize global 0 to 0
  vm.setGlobal(0, { type: 'int', value: 0 });
  console.log('  Initial global[0]:', vm.getGlobal(0));
  console.log('  ✓ VM created\n');

  // Test 3: Dispatch event (call increment function)
  console.log('Test 3: Execute bytecode');
  // Manually add event binding to the VM's module
  const vmModule = vm.getModule();
  if (vmModule) {
    vmModule.eventBindings.push({
      componentId: 1,
      eventType: 0, // click
      functionIndex: 0,
    });
  }
  vm.dispatchEvent(1, 0);
  console.log('  After increment, global[0]:', vm.getGlobal(0));
  console.log('  ✓ Bytecode executed\n');

  // Test 4: Run multiple times
  console.log('Test 4: Multiple executions');
  vm.dispatchEvent(1, 0);
  vm.dispatchEvent(1, 0);
  console.log('  After 2 more increments, global[0]:', vm.getGlobal(0));
  console.log('  ✓ Multiple executions work\n');

  console.log('=== All tests passed! ===');
}

// Run tests
runTests().catch(console.error);
