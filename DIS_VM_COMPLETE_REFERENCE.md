# DIS Virtual Machine Complete Technical Reference

**For Implementing Frontend Language Compilers Targeting DIS**

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Architecture Overview](#2-architecture-overview)
3. [Instruction Set Reference](#3-instruction-set-reference)
4. [Addressing Modes](#4-addressing-modes)
5. [Object File Format](#5-object-file-format)
6. [Garbage Collection](#6-garbage-collection)
7. [Execution Model](#7-execution-model)
8. [Data Structures](#8-data-structures)
9. [Constant Definitions](#9-constant-definitions)
10. [Compiler Implementation Guidelines](#10-compiler-implementation-guidelines)
11. [Runtime Integration](#11-runtime-integration)
12. [Debugging and Tools](#12-debugging-and-tools)
13. [Implementation Examples](#13-implementation-examples)
14. [Appendices](#14-appendices)

---

## 1. Introduction

### What is DIS?

DIS (Dis Virtual Machine) is a register-based virtual machine that provides the execution environment for programs running under Inferno and TaijiOS. It models a CISC-like, three-operand, memory-to-memory architecture designed for:

- **Portability**: Code can be interpreted or JIT-compiled to native machine code
- **Safety**: Type-safe execution with garbage collection
- **Concurrency**: Built-in support for threads and communication channels
- **Modularity**: Dynamic module loading with type checking

### Relationship to Inferno/TaijiOS

DIS is the execution engine for the Limbo programming language and serves as the universal binary format for the operating system. All system components and applications compile to DIS bytecode.

### Design Philosophy

1. **Memory-to-Memory Architecture**: Most instructions operate on memory locations rather than registers
2. **Type-Safe Execution**: All operations are type-checked with explicit type suffixes
3. **Hybrid Garbage Collection**: Supports both reference counting and mark-and-sweep
4. **Concurrent by Design**: Channels and alt for CSP-style concurrency

### Comparison to Other VMs

| Feature | DIS | JVM | .NET CLR |
|---------|-----|-----|----------|
| Architecture | Memory-to-memory | Stack-based | Stack-based |
| Type System | Explicit type suffixes | Bytecode verification | Type-safe metadata |
| Concurrency | Channels (CSP) | Shared memory + monitors | Tasks + async |
| GC | Hybrid (RC + mark-sweep) | Generational | Generational |
| Module System | Dynamic linking | JAR files | Assemblies |
| Instruction Size | Variable-length | Fixed | Variable |

---

## 2. Architecture Overview

### Memory Model

DIS memory is organized into several regions:

#### Code Segment (Not Addressable)
- Stores decoded VM instructions or native JIT-compiled code
- PC values are offsets from the beginning of code space
- Instructions are numbered from 0

#### Data Memory (Linear Array)
- 32-bit byte-addressable space
- Words stored in host CPU's native representation
- Multi-byte values must be aligned to their size boundary
- Addressed using 32-bit pointers

#### Registers

**MP (Module Pointer)**
- Points to global storage for a module
- Initialized at module load time
- Shared across all threads using the same module instance (unless SHAREMP flag is set)

**FP (Frame Pointer)**
- Points to current activation record (stack frame)
- Dynamically allocated from stack
- Modified only by call/return instructions

**Other Virtual Machine Registers** (internal, not directly addressable):
- **PC**: Program counter (instruction offset)
- **SP**: Stack pointer
- **TS**: Top of allocated stack
- **EX**: Stack extent register
- **M**: Current module link
- **IC**: Instruction count (for scheduling)

### Operand Sizes and Types

| Suffix | Type | Size | Description |
|--------|------|------|-------------|
| **B** | Byte | 8-bit | Unsigned integer |
| **W** | Word | 32-bit | Signed integer (two's complement) |
| **L** | Big | 64-bit | Signed integer (two's complement) |
| **F** | Float | 64-bit | IEEE 754 floating point |
| **P** | Pointer | 32-bit | Memory address |
| **C** | String | Variable | UTF-8 encoded Unicode string |
| **M** | Memory | Variable | Raw memory block |
| **MP** | Memory+ | Variable | Memory block containing pointers |

**Extended Types** (limited support †):
- **Short Word** (16-bit signed) - conversion only
- **Short Float** (32-bit IEEE) - conversion only

### Stack Organization

```
High Addresses
    |
    |  ... Stack Grows Down ...
    |
    +----------------------------------+ <-- SP (Stack Pointer)
    |  Current Frame                   |
    |  - Local variables               |
    |  - Temporaries                   |
    |  - Saved registers               |
    +----------------------------------+ <-- FP (Frame Pointer)
    |  Caller's Frame                  |
    |  - Return address (lr)           |
    |  - Saved FP                      |
    |  - Module link                   |
    +----------------------------------+
    |
    V
Low Addresses
```

**Stack Extension**:
- Automatically extended from heap when stack is exhausted
- Specified by `stack-extent` field in module header
- Transparent to programs (handled by VM)

---

## 3. Instruction Set Reference

Complete reference for all 182 DIS opcodes. Each instruction includes:
- Opcode number and mnemonic
- Syntax variants
- Operation description
- Side effects
- Implementation notes

### Notation Conventions

- `src`, `src1`, `src2`: Source operands
- `dst`: Destination operand
- `mid`: Middle operand (for 3-operand instructions)
- `H`: Nil pointer
- `PC`: Program counter
- `FP`: Frame pointer
- `MP`: Module pointer

---

### Instruction Categories

1. [No-ops and Control Flow](#no-ops-and-control-flow)
2. [Branches](#branches)
3. [Loads and Calls](#loads-and-calls)
4. [Memory Allocation](#memory-allocation)
5. [Arithmetic](#arithmetic)
6. [Logical Operations](#logical-operations)
7. [Shifts](#shifts)
8. [Conversions](#conversions)
9. [Data Movement](#data-movement)
10. [List Operations](#list-operations)
11. [String Operations](#string-operations)
12. [Communication](#communication)
13. [Array Operations](#array-operations)
14. [Exception Handling](#exception-handling)
15. [Extended Arithmetic](#extended-arithmetic)

---

### No-ops and Control Flow

#### INOP (0) - No Operation
```
Syntax:     nop
Function:   (no operation)
```
Does nothing. Used for alignment or padding.

---

#### ICALL (4) - Call Local Function
```
Syntax:     call src, dst
Function:   link(src) = pc
            frame(src) = fp
            mod(src) = 0
            fp = src
            pc = dst
```
Performs a function call within the same module.

- `src`: Frame created by `new` or `frame` instruction
- `dst`: PC of function entry point

**Side Effects**:
- Saves return address in frame's link field
- Saves current FP in frame's frame field
- Sets module register to 0 (local call)
- Updates FP to point to new frame
- Transfers control to `dst`

**Implementation** (from xec.c:678):
```c
OP(call)
{
    Frame *f = T(s);
    f->lr = R.PC;
    f->fp = R.FP;
    R.FP = (uchar*)f;
    JMP(d);
}
```

---

#### IRET (12) - Return from Function
```
Syntax:     ret
Function:   npc = link(fp)
            mod = mod(fp)
            fp = frame(fp)
            pc = npc
```
Returns control to caller.

**Side Effects**:
- Restores PC from frame's link field
- Restores module register
- Restores FP to caller's frame
- Decrements reference counts for pointers in frame
- May trigger stack contraction

**Implementation** (from xec.c:727):
```c
OP(ret)
{
    Frame *f = (Frame*)R.FP;
    R.FP = f->fp;
    if(R.FP == nil) {
        R.FP = (uchar*)f;
        error("");  // Exit program
    }
    R.SP = (uchar*)f;
    R.PC = f->lr;
    m = f->mr;

    if(f->t == nil)
        unextend(f);
    else if (f->t->np)
        freeptrs(f, f->t);

    if(m != nil) {
        if(R.M->compiled != m->compiled) {
            R.IC = 1;
            R.t = 1;
        }
        destroy(R.M);
        R.M = m;
        R.MP = m->MP;
    }
}
```

---

#### IJMP (13) - Jump Always
```
Syntax:     jmp dst
Function:   pc = dst
```
Unconditional jump to `dst`.

---

#### IFRAME (5) - Allocate Frame for Local Call
```
Syntax:     frame src1, src2
Function:   src2 = fp + src1->size
            initmem(src2, src1)
```
Creates a new stack frame for a local function call.

- `src1`: Type descriptor for the frame
- `src2`: Destination to store frame pointer

**Side Effects**:
- Allocates space on stack
- Initializes pointer fields to nil
- May trigger stack extension

**Implementation** (from xec.c:351):
```c
OP(frame)
{
    Type *t = R.M->type[W(s)];
    uchar *nsp = R.SP + t->size;
    if(nsp >= R.TS) {
        R.s = t;
        extend();
        T(d) = R.s;
        return;
    }
    Frame *f = (Frame*)R.SP;
    R.SP = nsp;
    f->t = t;
    f->mr = nil;
    if (t->np)
        initmem(t, f);
    T(d) = f;
}
```

---

#### ISPAWN (6) - Spawn New Thread
```
Syntax:     spawn src, dst
Function:   fork()
            if(child)
                dst(src)
```
Creates a new thread executing the specified function.

- `src`: Frame for the new thread
- `dst`: PC of function to execute

**Side Effects**:
- Creates new program structure
- Copies frame for child thread
- Parent continues execution
- Child starts at `dst` with frame `src`

---

#### IRUNT (7) - Run-time Hook
```
Syntax:     runt
Function:   (no operation)
```
Reserved for run-time system use. Does nothing in standard implementation.

---

#### IEXIT (21) - Terminate Thread
```
Syntax:     exit
Function:   exit()
```
Terminates the executing thread. All resources in the stack are deallocated.

---

#### IGOTO (3) - Computed Goto
```
Syntax:     goto src, dst
Function:   pc = dst[src]
```
Performs an indirect jump through a table.

- `src`: Index into table (word)
- `dst`: Address of PC table

**Implementation** (from xec.c:667):
```c
OP(igoto)
{
    WORD *t = (WORD*)((WORD)R.d + (W(s) * IBY2WD));
    if(R.M->compiled) {
        R.PC = (Inst*)t[0];
        return;
    }
    R.PC = R.M->prog + t[0];
}
```

---

#### ICASE (14) - Case Statement (Word)
```
Syntax:     case src, dst
Function:   pc = 0..i: dst[i].pc where
            dst[i].lo >= src && src < dst[i].hi
```
Binary search through range table.

- `src`: Value to test (word)
- `dst`: Table of {lo, hi, pc} triples

**Table Format**:
```
word: nentries
for i = 0 to nentries-1:
    word: low_value
    word: high_value (exclusive)
    word: pc
word: default_pc
```

**Implementation** (from xec.c:547):
```c
OP(icase)
{
    WORD v = W(s);
    WORD *t = (WORD*)((WORD)R.d + IBY2WD);
    WORD n = t[-1];
    WORD d = t[n*3];

    while(n > 0) {
        WORD n2 = n >> 1;
        WORD *l = t + n2*3;
        if(v < l[0]) {
            n = n2;
            continue;
        }
        if(v >= l[1]) {
            t = l+3;
            n -= n2 + 1;
            continue;
        }
        d = l[2];
        break;
    }
    R.PC = R.M->prog + d;
}
```

---

#### ICASEC (144) - Case Statement (String)
```
Syntax:     casec src, dst
Function:   pc = matching string range
```
Similar to `case` but compares strings instead of integers.

---

#### ICASEL (165) - Case Statement (Long)
```
Syntax:     casel src, dst
Function:   pc = matching 64-bit range
```
Similar to `case` but for 64-bit integers.

**Table Format**:
```
word: nentries
for i = 0 to nentries-1:
    big: low_value
    big: high_value (exclusive)
    word: pc
word: default_pc
```

---

### Branches

All branch instructions have the form: `b<condition><type> src1, src2, dst`

If `src1 <condition> src2`, then `pc = dst`.

#### Equality Tests

##### IBEQB (93), IBEQW (99), IBEQF (105), IBEQL (140), IBEQC (111)
```
Syntax:     beq<t> src1, src2, dst
Function:   if src1 == src2 then pc = dst
```

Type variants:
- **B**: Byte (8-bit)
- **W**: Word (32-bit)
- **F**: Float (64-bit)
- **L**: Long (64-bit)
- **C**: String (lexicographic compare)

**String Compare Implementation**:
```c
OP(beqc) { if(stringcmp(S(s), S(m)) == 0) JMP(d); }
```

---

#### Inequality Tests

##### IBNEB (94), IBNEW (100), IBNEF (106), IBNEL (139), IBNEC (112)
```
Syntax:     bne<t> src1, src2, dst
Function:   if src1 != src2 then pc = dst
```

---

#### Less Than Tests

##### IBLTB (95), IBLTW (101), IBLTF (107), IBLTL (136), IBLTC (113)
```
Syntax:     blt<t> src1, src2, dst
Function:   if src1 < src2 then pc = dst
```
For integer types: signed comparison
For float: IEEE 754 comparison
 For string: lexicographic comparison

---

#### Less Than or Equal Tests

##### IBLEB (96), IBLEW (102), IBLEF (108), IBLEL (137), IBLEC (114)
```
Syntax:     ble<t> src1, src2, dst
Function:   if src1 <= src2 then pc = dst
```

---

#### Greater Than Tests

##### IBGTB (97), IBGTW (103), IBGTF (109), IBGTL (138), IBGTC (115)
```
Syntax:     bgt<t> src1, src2, dst
Function:   if src1 > src2 then pc = dst
```

---

#### Greater Than or Equal Tests

##### IBGEB (98), IBGEW (104), IBGEF (110), IBGEL (141), IBGEC (116)
```
Syntax:     bge<t> src1, src2, dst
Function:   if src1 >= src2 then pc = dst
```

---

### Loads and Calls

#### ILOAD (8) - Load Module
```
Syntax:     load src1, src2, dst
Function:   dst = load src2 src1
```
Dynamically loads a module and performs type checking.

- `src1`: Pathname to .dis file (string)
- `src2`: Linkage descriptor table (address in MP)
- `dst`: Destination for Modlink pointer

**Linkage Descriptor Format**:
```c
struct {
    int nentries;           // Number of entries
    struct {
        int sig;            // MD5 signature hash
        char name[];        // UTF-8 encoded function name
    } entry[];
};
```

**Returns**: `H` (nil) on failure

**Side Effects**:
- May trigger module loading from disk
- May trigger JIT compilation
- Performs type signature checking

**Implementation** (from xec.c:769):
```c
OP(iload)
{
    char *n = string2c(S(s));
    Import *ldt;
    Module *m = R.M->m;

    if(m->rt & HASLDT)
        ldt = m->ldt[W(m)];
    else {
        ldt = nil;
        error("obsolete dis");
    }

    if(strcmp(n, "$self") == 0) {
        m->ref++;
        ml = linkmod(m, ldt, 0);
        if(ml != H) {
            ml->MP = R.M->MP;
            Heap *h = D2H(ml->MP);
            h->ref++;
            Setmark(h);
        }
    } else {
        m = readmod(n, lookmod(n), 1);
        ml = linkmod(m, ldt, 1);
    }

    Modlink **mp = R.d;
    Modlink *t = *mp;
    *mp = ml;
    destroy(t);
}
```

---

#### IMCALL (9) - Inter-Module Call
```
Syntax:     mcall src1, src2, src3
Function:   link(src1) = pc
            frame(src1) = fp
            mod(src1) = current_moduleptr
            current_moduleptr = src3->moduleptr
            fp = src1
            pc = src3->links[src2]->pc
```
Calls a function in another module.

- `src1`: Frame created by `mframe`
- `src2`: Link index in module's link table
- `src3`: Modlink pointer from `load` instruction

**Implementation** (from xec.c:808):
```c
OP(mcall)
{
    Heap *h;
    Frame *f;
    Linkpc *l;
    Modlink *ml = *(Modlink**)R.d;

    if(ml == H)
        errorf("mcall: %s", exModule);

    f = T(s);
    f->lr = R.PC;
    f->fp = R.FP;
    f->mr = R.M;

    R.FP = (uchar*)f;
    R.M = ml;
    h = D2H(ml);
    h->ref++;

    int o = W(m);
    if(o >= 0)
        l = &ml->links[o].u;
    else
        l = &ml->m->ext[-o-1].u;

    if(ml->prog == nil) {
        l->runt(f);
        // ... cleanup and return
    }

    R.MP = R.M->MP;
    R.PC = l->pc;
    R.t = 1;

    if(f->mr->compiled != R.M->compiled)
        R.IC = 1;
}
```

---

#### IMSPAWN (10) - Module Spawn
```
Syntax:     mspawn src1, src2, src3
Function:   fork()
            if(child) {
                link(src1) = 0
                frame(src1) = 0
                mod(src1) = src3->moduleptr
                current_moduleptr = src3->moduleptr
                fp = src1
                pc = src3->links[src2]->pc
            }
```
Creates a new thread executing a function in another module.

---

#### IMFRAME (11) - Allocate Inter-Module Frame
```
Syntax:     mframe src1, src2, dst
Function:   dst = fp + src1->links[src2]->t->size
            initmem(dst, src1->links[src2])
```
Creates a frame for calling into another module.

- `src1`: Modlink pointer
- `src2`: Link index
- `dst`: Destination for frame pointer

**Implementation** (from xec.c:378):
```c
OP(mframe)
{
    Type *t;
    Frame *f;
    uchar *nsp;
    Modlink *ml = *(Modlink**)R.s;

    if(ml == H)
        errorf("mframe: %s", exModule);

    int o = W(m);
    if(o >= 0) {
        if(o >= ml->nlinks)
            error("invalid mframe");
        t = ml->links[o].frame;
    } else
        t = ml->m->ext[-o-1].frame;

    nsp = R.SP + t->size;
    if(nsp >= R.TS) {
        R.s = t;
        extend();
        T(d) = R.s;
        return;
    }

    f = (Frame*)R.SP;
    R.SP = nsp;
    f->t = t;
    f->mr = nil;

    if (t->np)
        initmem(t, f);
    T(d) = f;
}
```

---

### Memory Allocation

#### INEW (22) - Allocate Object
```
Syntax:     new src, dst
Function:   dst = malloc(src->size)
            initmem(dst, src->map)
```
Allocates memory from heap. Non-pointer fields have undefined values.

- `src`: Type descriptor index
- `dst`: Destination for pointer

**Implementation** (from xec.c:314):
```c
OP(new)
{
    Heap *h = heap(R.M->type[W(s)]);
    WORD **wp = R.d;
    WORD *t = *wp;
    *wp = H2D(WORD*, h);
    destroy(t);
}
```

---

#### INEWZ (162) - Allocate Zeroed Object
```
Syntax:     newz src, dst
Function:   dst = malloc(src->size)
            initmem(dst, src->map)
            memset(dst, 0, src->size)
```
Same as `new` but all non-pointer fields are zeroed.

---

#### INEWA (23) - Allocate Array
```
Syntax:     newa src1, src2, dst
Function:   dst = malloc(src2->size * src1)
            for(i = 0; i < src1; i++)
                initmem(dst + i*src2->size, src2->map)
```
Allocates an array of elements.

- `src1`: Number of elements (word)
- `src2`: Element type descriptor index
- `dst`: Destination for array pointer

**Implementation** (from xec.c:427):
```c
OP(newa)
{
    int sz = W(s);
    Type *t = R.M->type[W(m)];
    acheck(t->size, sz);

    Heap *h = nheap(sizeof(Array) + (t->size*sz));
    h->t = &Tarray;
    Tarray.ref++;

    Array *a = H2D(Array*, h);
    a->t = t;
    a->len = sz;
    a->root = H;
    a->data = (uchar*)a + sizeof(Array);
    initarray(t, a);

    Array **ap = R.d;
    Array *at = *ap;
    *ap = a;
    destroy(at);
}
```

---

#### INEWAZ (163) - Allocate Zeroed Array
```
Syntax:     newaz src1, src2, dst
```
Same as `newa` but elements are initialized to zero.

---

#### INEWCB (24) - Allocate Channel (Byte)
```
Syntax:     newcb dst
Function:   dst = new Channel of byte
```

#### INEWCW (25) - Allocate Channel (Word)
```
Syntax:     newcw dst
Function:   dst = new Channel of word
```

#### INEWCF (26) - Allocate Channel (Float)
```
Syntax:     newcf dst
Function:   dst = new Channel of float
```

#### INEWCP (27) - Allocate Channel (Pointer)
```
Syntax:     newcp dst
Function:   dst = new Channel of pointer
```

#### INEWCL (149) - Allocate Channel (Long)
```
Syntax:     newcl dst
Function:   dst = new Channel of long
```

#### INEWCM (28) - Allocate Channel (Memory)
```
Syntax:     newcm src, dst
Function:   dst = new Channel of memory (src bytes)
```
Creates a buffered channel for moving raw memory blocks.

- `src`: Buffer size in bytes (optional)

---

#### INEWCMP (29) - Allocate Channel (Memory with Pointers)
```
Syntax:     newcmp src, dst
Function:   dst = new Channel of typed memory
```
Creates a channel for moving structured data containing pointers.

- `src`: Type descriptor index

**Implementation**:
```c
Channel*
cnewc(Type *t, void (*mover)(void), int len)
{
    Heap *h = heap(&Tchannel);
    Channel *c = H2D(Channel*, h);
    c->send = malloc(sizeof(Progq));
    c->recv = malloc(sizeof(Progq));
    c->send->prog = c->recv->prog = nil;
    c->send->next = c->recv->next = nil;
    c->mover = mover;
    c->buf = H;
    if(len > 0)
        c->buf = H2D(Array*, heaparray(t, len));
    c->front = 0;
    c->size = 0;
    return c;
}
```

---

#### IMNEWZ (154) - Allocate Object from Another Module
```
Syntax:     mnewz src1, src2, dst
Function:   dst = malloc(src1->types[src2]->size)
            initmem(dst, src1->types[src2]->map)
            memset(dst, 0, size)
```
Allocates an object using a type descriptor from a loaded module.

- `src1`: Modlink pointer
- `src2`: Type descriptor index in that module
- `dst`: Destination for pointer

---

### Arithmetic

All arithmetic instructions are three-operand: `op<t> src1, src2, dst`

#### Addition

##### IADDB (63), IADDW (64), IADDF (65), IADDL (124)
```
Syntax:     add<t> src1, src2, dst
Function:   dst = src1 + src2
```
For `addb`, result is truncated to 8 bits.

**Implementation**:
```c
OP(addb) { B(d) = B(m) + B(s); }
OP(addw) { W(d) = W(m) + W(s); }
OP(addl) { V(d) = V(m) + V(s); }
OP(addf) { F(d) = F(m) + F(s); }
```

---

#### Subtraction

##### ISUBB (66), ISUBW (67), ISUBF (68), ISUBL (125)
```
Syntax:     sub<t> src1, src2, dst
Function:   dst = src2 - src1
```
Note operand order: `src2 - src1` (not `src1 - src2`)

---

#### Multiplication

##### IMULB (69), IMULW (70), IMULF (71), IMULL (128)
```
Syntax:     mul<t> src1, src2, dst
Function:   dst = src1 * src2
```

---

#### Division

##### IDIVB (72), IDIVW (73), IDIVF (74), IDIVL (126)
```
Syntax:     div<t> src1, src2, dst
Function:   dst = src2 / src1
```
Note operand order: `src2 / src1`
Division by zero causes thread termination.

---

#### Modulus

##### IMODB (76), IMODW (75), IMODL (127)
```
Syntax:     mod<t> src1, src2, dst
Function:   dst = src2 % src1
```
Note operand order: `src2 % src1`
Preserves the invariant: `(a/b)*b + a%b == a`

---

#### Negation

##### INEGF (123) - Negate Float
```
Syntax:     negf src, dst
Function:   dst = -src
```

---

### Logical Operations

Bitwise operations on integer types.

#### AND

##### IANDB (77), IANDW (78), IANDL (130)
```
Syntax:     and<t> src1, src2, dst
Function:   dst = src1 & src2
```

---

#### OR

##### IORB (79), IORW (80), IORL (131)
```
Syntax:     or<t> src1, src2, dst
Function:   dst = src1 | src2
```

---

#### XOR

##### IXORB (81), IXORW (82), IXORL (132)
```
Syntax:     xor<t> src1, src2, dst
Function:   dst = src1 ^ src2
```

---

### Shifts

All shifts have the form: `shift<t> src1, src2, dst`

Where `src1` is shift count and `src2` is value to shift.

#### Shift Left (Arithmetic)

##### ISHLB (83), ISHLW (84), ISHLL (133)
```
Syntax:     shl<t> src1, src2, dst
Function:   dst = src2 << src1
```
Shift counts < 0 or >= type width have undefined results.

---

#### Shift Right (Arithmetic)

##### ISHRB (85), ISHRW (86), ISHRL (134)
```
Syntax:     shr<t> src1, src2, dst
Function:   dst = src2 >> src1  (arithmetic/signed shift)
```

---

#### Shift Right (Logical)

##### ILSRW (159), ILSRL (160)
```
Syntax:     lsr<t> src1, src2, dst
Function:   dst = (unsigned)src2 >> src1
```
Logical right shift fills with zeros (unsigned).

**Implementation**:
```c
OP(lsrw) { W(d) = UW(m) >> W(s); }
OP(lsrl) { V(d) = UV(m) >> W(s); }
```

---

### Conversions

Type conversion instructions. Most conversions are two-operand: `cvt<from><to> src, dst`

#### Numeric Conversions

##### ICVTBW (43) - Byte to Word
```
Syntax:     cvtbw src, dst
Function:   dst = src & 0xff  (zero extend)
```

##### ICVTWB (44) - Word to Byte
```
Syntax:     cvtwb src, dst
Function:   dst = (byte)src  (truncate)
```

##### ICVTWF (50) - Word to Float
```
Syntax:     cvtwf src, dst
Function:   dst = (float)src
```

##### ICVTFW (45) - Float to Word
```
Syntax:     cvtfw src, dst
Function:   dst = (int)src  (round to nearest)
```
Rounds toward nearest integer.

##### ICVTWL (146) - Word to Long
```
Syntax:     cvtwl src, dst
Function:   dst = (big)src
```

##### ICVT LW (147) - Long to Word
```
Syntax:     cvtlw src, dst
Function:   dst = (int)src  (truncate)
```

##### ICVTLF (141) - Long to Float
```
Syntax:     cvtlf src, dst
Function:   dst = (float)src
```

##### ICVTFL (142) - Float to Long
```
Syntax:     cvtfl src, dst
Function:   dst = (big)src  (round to nearest)
```

**Implementation**:
```c
OP(cvtfl)
{
    REAL f = F(s);
    V(d) = f < 0 ? f - .5 : f + .5;  // Round
}
```

---

#### String Conversions

##### ICVTWC (52) - Word to String
```
Syntax:     cvtwc src, dst
Function:   dst = string(src)  (decimal representation)
```

##### ICVTCW (59) - String to Word
```
Syntax:     cvtcw src, dst
Function:   dst = (int)src  (parse decimal)
```
Initial whitespace is ignored. Conversion stops at first non-digit.

**Implementation**:
```c
OP(cvtcl)
{
    String *s = S(s);
    if(s == H)
        V(d) = 0;
    else
        V(d) = strtoll(string2c(s), nil, 10);
}
```

##### ICVTLC (145) - Long to String
```
Syntax:     cvtlc src, dst
Function:   dst = string(src)  (decimal representation)
```

##### ICVTCL (148) - String to Long
```
Syntax:     cvtcl src, dst
Function:   dst = (big)src  (parse decimal)
```

##### ICVTFC (46) - Float to String
```
Syntax:     cvtfc src, dst
Function:   dst = string(src)  (floating point representation)
```

##### ICVTCF (47) - String to Float
```
Syntax:     cvtcf src, dst
Function:   dst = (float)src  (parse floating point)
```

---

#### Array/String Conversions

##### ICVTAC (57) - Byte Array to String
```
Syntax:     cvtac src, dst
Function:   dst = string(src)  (convert UTF-8 bytes to string)
```

##### ICVTCA (58) - String to Byte Array
```
Syntax:     cvtca src, dst
Function:   dst = array(src)  (convert string to byte array)
```

---

#### Extended Type Conversions †

##### ICVTFR (156) - Float to Short Float
```
Syntax:     cvtfr src, dst
Function:   dst = (short float)src  (64-bit to 32-bit)
```

##### ICVTRF (155) - Short Float to Float
```
Syntax:     cvtrf src, dst
Function:   dst = (float)src  (32-bit to 64-bit)
```

##### ICVTWS (157) - Word to Short Word
```
Syntax:     cvtws src, dst
Function:   dst = (short)src  (32-bit to 16-bit)
```

##### ICVTSW (158) - Short Word to Word
```
Syntax:     cvtsw src, dst
Function:   dst = (int)src  (16-bit to 32-bit)
```

---

### Data Movement

#### IMOVP (29) - Move Pointer
```
Syntax:     movp src, dst
Function:   destroy(dst)
            dst = src
            incref(src)
```
Moves a pointer with reference count management.

**Side Effects**:
- Decrements reference count of old value at `dst`
- Increments reference count of `src`
- Copies pointer to `dst`

**Implementation** (from xec.c:288):
```c
OP(movp)
{
    Heap *h;
    WORD *dv = P(d), *sv = P(s);

    if(sv != H) {
        h = D2H(sv);
        h->ref++;
        Setmark(h);
    }

    P(d) = sv;
    destroy(dv);
}
```

---

#### IMOVM (30) - Move Memory
```
Syntax:     movm src1, src2, dst
Function:   memmove(&dst, &src1, src2)
```
Copies raw memory block.

- `src1`: Source address
- `src2`: Byte count
- `dst`: Destination address

**Implementation**:
```c
OP(movm) { memmove(R.d, R.s, W(m)); }
```

---

#### IMOVMP (31) - Move Memory with Pointers
```
Syntax:     movmp src1, src2, dst
Function:   decmem(&dst, src2)
            memmove(&dst, &src1, src2->size)
            incmem(&src, src2)
```
Copies structured data containing pointers with proper reference counting.

- `src1`: Source address
- `src2`: Type descriptor
- `dst`: Destination address

**Implementation** (from xec.c:303):
```c
OP(movmp)
{
    Type *t = R.M->type[W(m)];

    incmem(R.s, t);
    if (t->np)
        freeptrs(R.d, t);
    memmove(R.d, R.s, t->size);
}
```

---

#### Scalar Moves

##### IMOVB (32), IMOVW (33), IMOVF (34), IMOVL (119)
```
Syntax:     mov<t> src, dst
Function:   dst = src
```

**Implementation**:
```c
OP(movb) { B(d) = B(s); }
OP(movw) { W(d) = W(s); }
OP(movf) { F(d) = F(s); }
OP(movl) { V(d) = V(s); }
```

---

#### IMOVPC (152) - Move Program Counter
```
Syntax:     movpc src, dst
Function:   dst = &PC(src)
```
Computes actual machine address of a PC value for computed branches.

**Implementation**:
```c
OP(movpc) { T(d) = &R.M->prog[W(s)]; }
```

---

#### ILEA (42) - Load Effective Address
```
Syntax:     lea src, dst
Function:   dst = &src
```
Computes the address of an operand.

**Implementation**:
```c
OP(lea) { W(d) = (WORD)R.s; }
```

---

#### ITAIL (43) - Tail of List
```
Syntax:     tail src, dst
Function:   dst = src->next
```
Returns a list with the head element removed.

---

### List Operations

Lists are functional-style linked pairs: [head, tail].

#### IHEADB (37), IHEADW (38), IHEADP (39), IHEADF (40), IHEADM (41), IHEADMP (42), IHEADL (147)
```
Syntax:     head<t> src, dst
Function:   dst = hd src
```
Copies the first element from a list.

Type variants:
- **B**: Head is byte
- **W**: Head is word
- **P**: Head is pointer
- **F**: Head is float
- **L**: Head is long
- **M**: Head is memory block
- **MP**: Head is memory with pointers

---

#### ICONSB (32), ICONSW (33), ICONSP (34), ICONSF (35), ICONSM (36), ICONSMP (37), ICONSL (148)
```
Syntax:     cons<t> src, dst
Function:   p = new(src, dst)
            dst = p
```
Adds a new element to the head of a list.

- `src`: Value to prepend
- `dst`: List to prepend to (modified in place)

---

#### ILENL (92) - Length of List
```
Syntax:     lenl src, dst
Function:   dst = 0
            for(l = src; l; l = tl l)
                dst++
```
Counts elements in a list (O(n) operation).

**Implementation** (from xec.c:881):
```c
OP(lenl)
{
    WORD l = 0;
    List *a = L(s);
    while(a != H) {
        l++;
        a = a->tail;
    }
    W(d) = l;
}
```

---

### String Operations

Strings are UTF-8 encoded Unicode.

#### IADDC (53) - Concatenate Strings
```
Syntax:     addc src1, src2, dst
Function:   dst = src1 + src2
```
Concatenates two strings. If both are nil, returns empty string (not nil).

---

#### IINSC (54) - Insert Character
```
Syntax:     insc src1, src2, dst
Function:   src1[src2] = dst
```
Inserts a Unicode character into a string.

- `src1`: String (or nil)
- `src2`: Index (0 to len+1)
- `dst`: Character value (21-bit Unicode)

If index equals string length, character is appended.

---

#### IINDC (55) - Index String
```
Syntax:     indc src1, src2, dst
Function:   dst = src1[src2]
```
Retrieves a Unicode character from a string.

- `src1`: String
- `src2`: Index (origin-0)

---

#### ILENC (56) - Length of String
```
Syntax:     lenc src, dst
Function:   dst = utflen(src)
```
Returns number of Unicode characters (not bytes).

**Implementation**:
```c
OP(lenc) { W(d) = stringlen(S(s)); }
```

---

### Array Operations

#### IINDX (46) - Index Array (Generic)
```
Syntax:     indx src1, dst, src2
Function:   dst = &src1[src2]
```
Computes effective address of array element.

- `src1`: Array pointer
- `src2`: Index
- `dst`: Destination for address

**Implementation** (from xec.c:218):
```c
OP(indx)
{
    Array *a = A(s);
    uintptr i = W(d);

    if(a == H || i >= a->len)
        error(exBounds);

    W(m) = (WORD)(a->data + i*a->t->size);
}
```

---

#### IINDW (120), IINDF (121), IINDB (122), IINDL (151)
```
Syntax:     ind<t> src1, dst, src2
Function:   dst = &src1[src2]
```
Type-specific array indexing (more efficient than generic `indx`).

---

#### ILENA (91) - Length of Array
```
Syntax:     lena src, dst
Function:   dst = nelem(src)
```

**Implementation** (from xec.c:868):
```c
OP(lena)
{
    Array *a = A(s);
    WORD l = (a != H) ? a->len : 0;
    W(d) = l;
}
```

---

#### ISLICEA (117) - Slice Array
```
Syntax:     slicea src1, src2, dst
Function:   dst = dst[src1:src2]
```
Creates a reference array pointing to elements [src1:src2-1] of dst.

**Note**: Original array remains allocated until both slices are freed.

---

#### ISLICELA (118) - Assign to Array Slice
```
Syntax:     slicela src1, src2, dst
Function:   dst[src2:] = src1
```
Assigns array `src1` to slice `dst[src2:]`.

Requires: `src2 + len(src1) <= len(dst)`

---

#### ISLICEC (119) - Slice String
```
Syntax:     slicec src1, src2, dst
Function:   dst = dst[src1:src2]
```
Creates a new string with characters from [src1:src2-1].

**Note**: Unlike `slicea`, this creates a copy of the string data.

---

### Communication

#### ISEND (30) - Send to Channel
```
Syntax:     send src, dst
Function:   dst <-= src
```
Sends a value on a channel. Blocks until a receiver is ready.

- `src`: Value to send
- `dst`: Channel

---

#### IRECV (31) - Receive from Channel
```
Syntax:     recv src, dst
Function:   dst = <-src
```
Receives a value from a channel. Blocks until a sender is ready.

- `src`: Channel
- `dst`: Destination for received value

---

#### IALT (1) - Alternate Between Channels
```
Syntax:     alt src, dst
```
Non-deterministic communication on multiple channels.

**Src points to Alt structure**:
```c
struct Alt {
    int nsend;           // Number of sender entries
    int nrecv;           // Number of receiver entries
    struct {
        Channel *c;      // Channel
        void *val;       // Address of value
    } entry[];
};
```

**Operation**:
1. Test each channel for readiness
2. If none ready, block until one is ready
3. Randomly select from ready set
4. Perform communication
5. Store selected index in `dst`

---

#### INBALT (2) - Non-Blocking Alt
```
Syntax:     nbalt src, dst
```
Same as `alt`, but returns immediately if no channel is ready.

Jumps to PC in last table element if no channels ready.

---

#### ITCMP (153) - Type Compare
```
Syntax:     tcmp src, dst
Function:   if(typeof(src) != typeof(dst))
                error("typecheck")
```
Runtime type check. Ensures `src` can be assigned to `dst`.

Success if:
- Both pointers from same type descriptor
- `src` is nil (H)

---

### Exception Handling

#### IRAISE (164) - Raise Exception
```
Syntax:     raise src
Function:   raise src
```
Raises an exception. Searches for handler in call stack.

---

#### IECLR (161) - Exception Clear
```
Syntax:     eclr
Function:   (implementation specific)
```
Clears exception state. Used in exception handlers.

---

### Extended Arithmetic

Instructions for extended precision arithmetic.

#### IMULX (166), IDIVX (167), ICVTXX (168)
**Placeholder instructions** - not fully implemented in base system.

---

#### IMULX0 (169), IDIVX0 (170), ICVTXX0 (171)
**Placeholder instructions** for extended arithmetic variants.

---

#### IMULX1 (172), IDIVX1 (173), ICVTXX1 (174)
**Placeholder instructions** for extended arithmetic variants.

---

#### ICVTFX (175), ICVTXF (176)
```
Syntax:     cvtfx src, dst
Syntax:     cvtxf src, dst
```
Float to/from extended precision conversions.

---

#### IEXPW (177), IEXPL (178), IEXPF (179)
```
Syntax:     exp<t> src1, src2, dst
Function:   dst = src2 ^ src1
```
Exponentiation (power) operation.

**Implementation** (from xec.c:144):
```c
OP(iexpw)
{
    WORD x = W(m);
    WORD n = W(s);
    int inv = (n < 0);
    if(inv) n = -n;

    WORD r = 1;
    for(;;) {
        if(n&1) r *= x;
        if((n >>= 1) == 0) break;
        x *= x;
    }

    if(inv) r = 1/r;
    W(d) = r;
}
```

---

#### ISELF (180) - Self Reference
```
Syntax:     self
Function:   (implementation specific)
```
Returns reference to current object/module. Used for method dispatch.

---

## 4. Addressing Modes

DIS uses a flexible three-operand addressing scheme. Each instruction can address up to three operands: middle, source, and destination.

### Address Mode Encoding

The address mode byte encodes the addressing mode for all three operands:

```
bit  7  6  5  4  3  2  1  0
     m1 m0 s2 s1 s0 d2 d1 d0
```

- `m1-m0`: Middle operand mode (2 bits)
- `s2-s0`: Source operand mode (3 bits)
- `d2-d0`: Destination operand mode (3 bits)

### Middle Operand Modes

| Encoding | Mode | Description | Syntax |
|----------|------|-------------|--------|
| 00 | AXNON | No middle operand | - |
| 01 | AXIMM | Small immediate | `$n` |
| 10 | AXINF | Small offset from FP | `n(fp)` |
| 11 | AXINM | Small offset from MP | `n(mp)` |

The middle operand does **not** support double-indirect addressing.

### Source/Destination Operand Modes

| Encoding | Mode | Description | Syntax |
|----------|------|-------------|--------|
| 000 | AMP | Offset indirect from MP | `n(mp)` |
| 001 | AFP | Offset indirect from FP | `n(fp)` |
| 010 | AIMM | 30-bit immediate | `$n` |
| 011 | AXXX | No operand | - |
| 100 | AIND\|AMP | Double indirect from MP | `n(m(mp))` |
| 101 | AIND\|AFP | Double indirect from FP | `n(m(fp))` |
| 110 | Reserved | - | - |
| 111 | Reserved | - | - |

### Operand Encoding

Operands are encoded using a variable-length scheme:

**Format**: Selected by two most significant bits
- `00xxxxxx`: Signed 7-bit value, 1 byte
- `10xxxxxx xxxxxxxx`: Signed 14-bit value, 2 bytes
- `11xxxxxx xxxxxxxx xxxxxxxx xxxxxxxx`: Signed 30-bit value, 4 bytes

**Decoding** (from load.c:14):
```c
static s32 operand(uchar **p)
{
    uchar *cp = *p;
    int c = cp[0];

    switch(c & 0xC0) {
    case 0x00:
        *p = cp+1;
        return c;
    case 0x40:
        *p = cp+1;
        return c | ~0x7F;
    case 0x80:
        *p = cp+2;
        if(c & 0x20)
            c |= ~0x3F;
        else
            c &= 0x3F;
        return (c<<8) | cp[1];
    case 0xC0:
        *p = cp+4;
        if(c & 0x20)
            c |= ~0x3F;
        else
            c &= 0x3F;
        return (c<<24) | (cp[1]<<16) | (cp[2]<<8) | cp[3];
    }
    return 0;
}
```

### Addressing Mode Examples

#### Immediate Mode
```
    addw    $5, $10, $15        # dst = 5 + 10
```

#### Indirect from FP (Local Variable)
```
    movw    $42, 4(fp)          # store 42 in local at offset 4
```

#### Indirect from MP (Global Variable)
```
    movw    8(mp), 12(mp)       # copy global[2] to global[3]
```

#### Double Indirect
```
    movw    4(8(mp)), 16(fp)    # load through pointer in global[2]
```

### Effective Address Calculation

For each addressing mode, the effective address is calculated as:

**Offset Indirect (AMP/AFP)**:
```
addr = (base == MP) ? R.MP + offset : R.FP + offset
```

**Immediate (AIMM)**:
```
value = sign_extended(operand)
```

**Double Indirect (AIND|AMP, AIND|AFP)**:
```
ptr_addr = base + offset1
ptr = *(WORD**)ptr_addr
addr = ptr + offset2
```

---

## 5. Object File Format

A DIS object file (.dis) contains a single module with executable code, data, and metadata.

### File Structure

```
+------------------+
| Header           |
+------------------+
| Code Section     |
+------------------+
| Type Section     |
+------------------+
| Data Section     |
+------------------+
| Module Name      |
+------------------+
| Link Section     |
+------------------+
| Import Section   | (optional)
+------------------+
| Handler Section  | (optional)
+------------------+
```

### Data Types in Encoding

| Type | Name | Size | Description |
|------|------|------|-------------|
| B | Byte | 1 byte | Unsigned 8-bit |
| W | Word | 4 bytes | Signed 32-bit (MSB first) |
| F | Real | 8 bytes | IEEE 754 double (canonical) |
| OP | Operand | 1-4 bytes | Variable-length signed integer |

All multi-byte values are big-endian (most significant byte first).

### Header Section

```
Header {
    OP: magic_number;      // Module identifier
    Signature;             // Optional digital signature
    OP: runtime_flag;      // Execution options
    OP: stack_extent;      // Stack growth increment
    OP: code_size;         // Number of instructions
    OP: data_size;         // Size of global data (bytes)
    OP: type_size;         // Number of type descriptors
    OP: link_size;         // Number of exported functions
    OP: entry_pc;          // Entry point instruction index
    OP: entry_type;        // Entry point type descriptor index
}
```

#### Magic Numbers

| Value | Symbolic | Description |
|-------|----------|-------------|
| 819248 | XMAGIC | Unsigned module |
| 923426 | SMAGIC | Signed module (with signature) |

**Verification** (from isa.h):
```c
#define XMAGIC   819248
#define SMAGIC   923426
```

#### Runtime Flags

| Bit | Symbol | Description |
|-----|--------|-------------|
| 0 | MUSTCOMPILE | Must JIT-compile (error if unavailable) |
| 1 | DONTCOMPILE | Do not compile (for debugging) |
| 2 | SHAREMP | Share module data across instances |
| 3 | DYNMOD | Dynamic C module |
| 4 | HASLDT0 | Old-style import section (obsolete) |
| 5 | HASEXCEPT | Has exception handler section |
| 6 | HASLDT | Has import section |

#### Signature (SMAGIC only)

```
Signature {
    OP: length;              // Signature length
    byte[length]: data;      // Signature bytes
}
```

Identifies signing authority, algorithm, and signed data.

### Code Section

Contains `code_size` instructions.

#### Instruction Encoding

```
Instruction {
    B: opcode;              // Operation code
    B: address_mode;        // Middle/src/dst mode encoding
    Middle_data;            // Optional middle operand
    Source_data;            // Optional source operand(s)
    Dest_data;              // Optional dest operand(s)
}
```

**Presence**:
- `Middle_data`: Present if middle mode != AXNON
- `Source_data`: Present if src mode != AXXX
- `Dest_data`: Present if dst mode != AXXX

**Double Indirect**: Source_data and Dest_data each encode two operands when using double-indirect mode.

### Type Section

Contains `type_size` type descriptors for garbage collection.

```
Type_descriptor {
    OP: desc_number;        // Descriptor ID (referenced by instructions)
    OP: size;               // Memory size in bytes
    OP: map_size;           // Pointer map size in bytes
    byte[map_size]: map;    // Pointer bitmap
}
```

#### Pointer Map

The `map` is a bitmap where each bit corresponds to a 32-bit word in memory:

- Bit 0 (MSB of first byte): Word at offset 0
- Bit 1: Word at offset 4
- etc.

A set bit (1) indicates that word contains a pointer.

**Example** (16-byte structure with pointer at offset 8):
```
size = 16
map_size = 2
map = {0x00, 0x80}  // Binary: 00000000 10000000
                     // Only bit 7 (offset 8*4 = 32) has pointer
```

**Note**: Unspecified map bytes are assumed zero.

### Data Section

Encodes initialization data for the module's global data (MP).

#### Data Items

Each item has:
```
data_item {
    B: code;                // Type and count encoding
    OP: offset;             // Byte offset from current base
    ... data values ...     // Depends on code
}
```

#### Code Byte Encoding

```
code[7:4]: Data type
code[3:0]: Count (0-15), or 0 for extended count
```

**Data Types** (high nibble):

| Value | Name | Description |
|-------|------|-------------|
| 0 | DEFZ | End of data section |
| 1 | DEFB | 8-bit bytes |
| 2 | DEFW | 32-bit words |
| 3 | DEFS | UTF-8 string |
| 4 | DEFF | 64-bit IEEE float |
| 5 | DEFA | Array |
| 6 | DIND | Set array index |
| 7 | DAPOP | Restore array index |
| 8 | DEFL | 64-bit big integer |

#### Extended Count

If count nibble is 0, actual count follows as an operand:

```
if(code[3:0] == 0) {
    count = operand();  // Read count
} else {
    count = code[3:0];
}
```

#### Data Type Encodings

**Bytes (DEFB)**:
```
for(i = 0; i < count; i++) {
    byte: value;
}
```

**Words (DEFW)**:
```
for(i = 0; i < count; i++) {
    W: value;  // 4 bytes, big-endian
}
```

**Bigs (DEFL)**:
```
for(i = 0; i < count; i++) {
    hi: W;     // High 32 bits
    lo: W;     // Low 32 bits
}
```

**Floats (DEFF)**:
```
for(i = 0; i < count; i++) {
    W: hi;     // IEEE 754 canonical
    W: lo;
}
```

**Strings (DEFS)**:
```
byte[count]: utf8_data;
```
Converted to String structure with reference count.

**Arrays (DEFA)**:
```
W: type_index;    // Type descriptor for elements
W: length;        // Number of elements
```
Creates Array structure, elements initialized to zero.

**Set Index (DIND)**:
```
W: element_index;
```
Must follow an Array item. Changes load base to `&array[element_index]`.

**Restore Index (DAPOP)**:
```
(no operands)
```
Restores previous load base from stack.

#### Base Address Stack

The loader maintains a stack of base addresses:

1. Initial base = MP (module data start)
2. Each DIND pushes current base and sets new base
3. Each DAPOP pops to restore previous base

**Example** (Initialize array elements):
```
DEFA  offset=0, type=5, length=10    # Create array at global[0]
DIND  offset=4, index=0              # Set base to &array[0]
DEFW  offset=0, values=[1,2,3]       # Init elements 0,1,2
DIND  offset=4, index=3              # Set base to &array[3]
DEFW  offset=0, values=[4,5]         # Init elements 3,4
DAPOP  offset=0                      # Restore to array base
DAPOP  offset=0                      # Restore to MP
```

### Module Name

Immediately follows data section:

```
byte[]: utf8_string_terminated_by_zero;
```

### Link Section

Lists exported functions (entry points for other modules).

```
Link_item {
    OP: pc;                 // Instruction index of entry point
    OP: desc;               // Type descriptor index (-1 if none)
    W: sig;                 // MD5 hash of type signature
    byte[]: name;           // UTF-8 name, null-terminated
}
```

**Member Functions**: Qualified with ADT name:
```
" Iobuf.gets"
```

**Count**: `link_size` items (from header).

**Special Entry**: The module's data pointer is exported as:
```
pc = -1
desc = -1
sig = 0
name = ".mp"
```

### Import Section (Optional)

Present if `runtime_flag & HASLDT`.

```
Import_Section {
    OP: num_modules;        // Number of modules imported
    ... for each module ...
    Module_imports {
        OP: num_funcs;       // Functions from this module
        ... for each function ...
        Function_import {
            W: sig;          // Type signature hash
            byte[]: name;    // Function name (null-terminated)
        }
    }
}
```

Used for type-checking imports at load time.

### Handler Section (Optional)

Present if `runtime_flag & HASEXCEPT`.

```
Handler_Section {
    OP: num_handlers;        // Number of exception handlers
    ... for each handler ...
    Handler {
        OP: frame_offset;    // Offset of exception structure in frame
        OP: pc_start;        // Start PC of handler range
        OP: pc_end;          // End PC (exclusive)
        OP: type_desc;       // Type to destroy (-1 if none)
        OP: num_labels;      // Number of exception labels
        W: flags;            // (high 16 bits = declared count)
        ... for each label ...
        Exception_label {
            byte[]: name;    // Exception name (null-terminated)
            OP: handler_pc;   // PC to jump to
        }
        OP: wildcard_pc;     // PC for wildcard (*) or -1
    }
}
```

### Complete File Example

```
# Header
OP(819248)           # XMAGIC
OP(0)                # runtime_flag
OP(8192)             # stack_extent
OP(5)                # code_size (5 instructions)
OP(12)               # data_size (12 bytes)
OP(2)                # type_size (2 descriptors)
OP(1)                # link_size (1 export)
OP(0)                # entry_pc
OP(1)                # entry_type

# Code Section
B(0x4B)              # opcode: movw
B(0x02)              # addr: AIMM, AXXX, AXNON
OP(42)               # src: immediate 42
B(0x56)              # opcode: jmp
B(0x42)              # addr: AIMM, AXXX, AXNON
OP(0)                # dst: pc = 0
B(0x8C)              # opcode: ret
B(0x00)              # addr: all none
B(0x0C)              # opcode: exit
B(0x00)              # addr: all none
B(0x00)              # opcode: nop
B(0x00)              # addr: all none

# Type Section
OP(0)                # desc 0
OP(12)               # size = 12 bytes
OP(2)                # map_size = 2 bytes
B(0xC0)              # map = 11000000 (pointer at word 0)
B(0x00)
OP(1)                # desc 1
OP(0)                # size = 0
OP(0)                # map_size = 0

# Data Section
B(0x21)              # DEFW | count=1
OP(0)                # offset = 0
W(0x0000000A)        # value = 10
B(0x23)              # DEFW | count=3
OP(4)                # offset = 4
W(0x00000001)        # values[0] = 1
W(0x00000002)        # values[1] = 2
W(0x00000003)        # values[2] = 3
B(0x00)              # End of data section

# Module Name
"Mymodule\0"

# Link Section
OP(1)                # pc = instruction 1
OP(0)                # desc = none
W(0x12345678)        # sig
"main\0"             # name
```

---

## 6. Garbage Collection

DIS uses a hybrid garbage collection system combining reference counting with mark-and-sweep.

### Reference Counting

**Principle**: Each heap object has a reference count. When count reaches zero, object is freed.

**Instructions with Reference Counting**:
- `movp`: Explicit pointer move with refcount updates
- `movmp`: Structured data with pointers
- `new`, `newz`, `newa`, etc.: Initialize refcount to 1

**Implementation** (from xec.c:288):
```c
OP(movp)
{
    Heap *h;
    WORD *dv = P(d), *sv = P(s);

    if(sv != H) {  // Increment source
        h = D2H(sv);
        h->ref++;
        Setmark(h);
    }

    P(d) = sv;
    destroy(dv);  // Decrement destination
}
```

**Limitations**:
- Cannot collect cyclic structures
- Requires explicit `destroy` calls on scope exit

### Mark-and-Sweep

**Principle**: Tracing collector that marks reachable objects and frees unreachable ones.

**When It Runs**:
1. Memory allocation fails
2. Explicitly triggered (implementation-specific)
3. Periodic background collection

**Algorithm**:
1. **Mark Phase**: Starting from roots (MP, FP, registers), recursively mark reachable objects
2. **Sweep Phase**: Free all unmarked objects

**Marking** (from interp.h:366):
```c
#define Setmark(h) \
    if((h)->color != mutator) { \
        (h)->color = propagator; \
        nprop = 1; \
    }
```

**Roots**:
- Module data (MP)
- Stack frames (FP chain)
- Virtual machine registers
- Running program state

### Type Descriptors and GC

Type descriptors tell the GC where pointers are located in objects.

**Type Structure** (from interp.h:201):
```c
struct Type {
    int ref;                 // Reference count
    void (*free)(Heap*, int);
    void (*mark)(Type*, void*);
    int size;                // Object size
    int np;                  // Number of map bytes
    void *destroy;
    void *initialize;
    uchar map[STRUCTALIGN];  // Pointer bitmap
};
```

**Pointer Map**:
- Each byte covers 8 words (32 bytes)
- Most significant bit = lowest address
- Set bit = word contains pointer

**Example** (Structure with 3 pointers):
```
struct {
    void *a;    // offset 0
    int x;      // offset 4
    void *b;    // offset 8
    void *c;    // offset 12
};
```
Type descriptor:
```
size = 16
np = 2
map = {0xE0, 0x00}
// Byte 0: 11100000 (pointers at words 0, 8, 12)
// Byte 1: 00000000
```

### GC-Safe Instructions

Some instructions automatically handle GC:

**`new`, `newz`, `newa`**: Allocate and initialize
**`movmp`**: Update refcounts during copy
**`frame`, `mframe`**: Initialize frame pointers
**`ret`**: Decrement frame refs on return

### Compiler Writer's Guide to GC

**Rule 1**: Always use type descriptors
- Every heap allocation must have a type descriptor
- Pointer maps must be accurate

**Rule 2**: Use appropriate move instructions
- `movp` for pointers
- `movmp` for structures with pointers
- `movm` for raw data

**Rule 3**: Initialize pointers
- Use `initmem` after allocation
- Uninitialized pointers = crashes

**Rule 4**: Don't bypass reference counting
- Never manually manipulate refcounts
- Let VM handle it

---

## 7. Execution Model

### Interpreter Loop

The DIS interpreter uses a dispatch loop to execute instructions.

**Main Loop** (simplified):
```c
void xec(Prog *p)
{
    REG *r = &p->R;

    for(;;) {
        Inst *i = r->PC;

        // Decode instruction
        r->s = resolve_source(i);
        r->d = resolve_dest(i);
        r->m = resolve_middle(i);

        // Dispatch
        optab[i->op]();  // Call handler function

        // Check for scheduling
        if(--r->IC == 0) {
            schedule();
            r->IC = PQUANTA;
        }
    }
}
```

**Instruction Table** (from isa.h:387):
```c
extern void (*optab[256])(void);
```

### Function Call Mechanism

**Calling Sequence**:
```
1. caller: frame src, new_frame    # Allocate frame
2. caller: ... initialize args ...
3. caller: call new_frame, func    # Call
4. callee: ... function body ...
5. callee: ret                     # Return
6. caller: ... use result ...
```

**Frame Layout**:
```
+------------------+
| Type* t          |  # Type descriptor
| ModuleLink* mr   |  # Saved module (for inter-module calls)
| Inst* lr         |  # Return address (link register)
| uchar* fp        |  # Saved frame pointer
+------------------+
| Arguments        |
| Locals           |
| Temporaries       |
+------------------+
```

**Implementation** (call from xec.c:678):
```c
OP(call)
{
    Frame *f = T(s);  // Get new frame
    f->lr = R.PC;     // Save return PC
    f->fp = R.FP;     // Save old FP
    R.FP = (uchar*)f; // Set new FP
    JMP(d);           // Jump to function
}
```

**Return Sequence** (ret from xec.c:727):
```c
OP(ret)
{
    Frame *f = (Frame*)R.FP;
    R.FP = f->fp;     // Restore caller's FP
    R.SP = (uchar*)f; //收缩 stack
    R.PC = f->lr;     // Return to caller

    if(f->t == nil)
        unextend(f);  // Free stack extension
    else if (f->t->np)
        freeptrs(f, f->t);  // Decrement ptrs

    if(f->mr != nil) {
        // Restore module for inter-module call
        R.M = f->mr;
        R.MP = f->mr->MP;
    }
}
```

### Module Loading and Linking

**Load Process**:
1. Read .dis file
2. Verify magic number and signature
3. Parse header
4. Load instructions into memory
5. Load type descriptors
6. Initialize data section (create origmp)
7. Parse export table
8. Optionally JIT-compile

**Linking**:
1. When `load` instruction executed:
   - Check if module already loaded
   - If not, read and parse module
   - For each import, verify signature match
   - Build Modlink structure with PC pointers

**Dynamic Linking** (from load.c:769):
```c
OP(iload)
{
    char *name = string2c(S(s));
    Import *ldt = R.M->ldt[W(m)];

    if(strcmp(name, "$self") == 0) {
        // Special case: self-reference
        ml = linkmod(R.M->m, ldt, 0);
    } else {
        // Load external module
        Module *m = readmod(name, lookmod(name), 1);
        ml = linkmod(m, ldt, 1);
    }

    // Store modlink pointer
    *(Modlink**)R.d = ml;
}
```

### Stack Management

**Initial Stack**:
```
+------------------+ <-- SP
| Frame 0          |
+------------------+
| ... grow down ...|
+------------------+ <-- TS (Top of Stack)
```

**After Call**:
```
+------------------+ <-- SP (new)
| Frame 1 (callee) |
+------------------+
| Frame 0 (caller)| <-- FP
+------------------+
| ...              |
+------------------+ <-- TS
```

**Stack Extension**:
When stack exhausted (`SP + frame_size >= TS`):
1. Allocate new stack chunk from heap
2. Link to old stack
3. Copy frame if necessary
4. Update SP, FP, TS registers

**Contraction**:
On return to stack extent boundary:
1. Free stack chunk
2. Restore previous stack

### Thread Scheduling

DIS is a multi-threaded VM with cooperative scheduling.

**Scheduler States** (from interp.h:12):
```c
enum ProgState {
    Palt,       // Blocked in alt
    Psend,      // Waiting to send
    Precv,      // Waiting to recv
    Pdebug,     // Debugged
    Pready,     // Ready to run
    Prelease,   // Interpreter released
    Pexiting,   // Exiting
    Pbroken,    // Crashed
};
```

**Scheduling**:
- Each thread has time quantum (`PQUANTA = 2048` instructions)
- When quantum exhausted, switch to next ready thread
- Threads block on channel operations
- Alt can block on multiple channels

---

## 8. Data Structures

### Core Runtime Structures

#### Frame - Stack Frame
```c
struct Frame {
    Inst*    lr;   // Link register (return PC)
    uchar*   fp;   // Saved frame pointer
    Modlink* mr;   // Saved module link
    Type*    t;    // Type descriptor
};
```
**Location**: `/include/interp.h:81`

---

#### Array - Dynamic Array
```c
struct Array {
    WORD    len;   // Number of elements
    Type*   t;     // Element type
    Array*  root;  // Root array (for slices)
    uchar*  data;  // Element data
};
```
**Location**: `/include/interp.h:104`

**Memory Layout**:
```
+------------------+
| Array header     |
+------------------+
| Element 0        |
| Element 1        |
| ...              |
| Element n-1      |
+------------------+
```

---

#### List - Linked List
```c
struct List {
    List*   tail;  // Next list node
    Type*   t;     // Element type
    WORD    data[1]; // First element (flexible array)
};
```
**Location**: `/include/interp.h:112`

**Structure**: Functional cons cells
```
[head, tail] -> [head, tail] -> nil
```

---

#### Channel - Communication Channel
```c
struct Channel {
    Array*  buf;   // Buffer (for buffered channels)
    Progq*  send;  // Queue of waiting senders
    Progq*  recv;  // Queue of waiting receivers
    void*   aux;   // Auxiliary data (device server)
    void (*mover)(void);  // Data movement function
    union {
        WORD   w;   // Buffer size (for newcm)
        Type*  t;   // Type (for newcmp)
    } mid;
    int     front; // Front of buffer queue
    int     size;  // Current buffer size
};
```
**Location**: `/include/interp.h:119`

---

#### String - Unicode String
```c
struct String {
    int     len;   // String length (bytes or runes)
    int     max;   // Allocated size
    char*   tmp;   // Temporary data (for operations)
    union {
        char   ascii[STRUCTALIGN];
        Rune   runes[1];
    } data;
};
```
**Location**: `/include/interp.h:140`

**Representation**:
- UTF-8 encoded bytes
- Length in bytes OR runes
- Reference counted

---

#### Type - Type Descriptor
```c
struct Type {
    int     ref;   // Reference count
    void (*free)(Heap*, int);  // Destructor
    void (*mark)(Type*, void*); // Marker for GC
    int     size;  // Size in bytes
    int     np;    // Size of pointer map
    void*   destroy;
    void*   initialize;
    uchar   map[STRUCTALIGN];  // Pointer bitmap
};
```
**Location**: `/include/interp.h:201`

---

#### Module - Loaded Module
```c
struct Module {
    int         ref;        // Use count
    int         compiled;   // JIT-compiled flag
    ulong       ss;         // Stack size
    ulong       rt;         // Runtime flags
    ulong       mtime;      // Modification time
    Qid         qid;        // File server QID
    ushort      dtype;      // File server type
    uint        dev;        // File server device
    int         nprog;      // Number of instructions
    Inst*       prog;       // Instruction array
    uchar*      origmp;     // Original module data
    int         ntype;      // Number of types
    Type**      type;       // Type descriptor array
    Inst*       entry;      // Entry point PC
    Type*       entryt;     // Entry point type
    char*       name;       // Module name
    char*       path;       // File path
    Module*     link;       // Next module in chain
    Link*       ext;        // Exported functions
    Import**    ldt;        // Import tables
    Handler*    htab;       // Exception handlers
    uintptr*    pctab;      // PC table (when compiled)
    void*       dlm;        // Dynamic C module handle
};
```
**Location**: `/include/interp.h:270`

---

#### Modlink - Module Instance
```c
struct Modlink {
    uchar*  MP;     // Module data (per-instance)
    Module* m;      // Module descriptor
    int     compiled; // Compiled flag
    Inst*   prog;   // Instructions (or native code)
    Type**  type;   // Type descriptors
    uchar*  data;   // For C dynamic modules
    int     nlinks; // Number of links
    Modl    links[1]; // Link entries
};
```
**Location**: `/include/interp.h:312`

---

#### Heap - Heap Object Header
```c
struct Heap {
    int     color;  // GC color (mutator/propagator)
    ulong   ref;    // Reference count
    Type*   t;      // Type descriptor
    ulong   hprof;  // Profiling data
};
```
**Location**: `/include/interp.h:325`

**Memory Layout**:
```
+------------------+
| Heap header      |
+------------------+
| Object data      |
| ...              |
+------------------+
```

**Macros** (from interp.h:362):
```c
#define H2D(t, x)  ((t)(((uchar*)(x))+sizeof(Heap)))  // Heap to Data
#define D2H(x)     ((Heap*)(((uchar*)(x))-sizeof(Heap))) // Data to Heap
```

---

#### Inst - Instruction
```c
struct Inst {
    uchar   op;     // Opcode
    uchar   add;    // Address mode
    ushort  reg;    // Reserved
    Adr     s;      // Source operand
    Adr     d;      // Destination operand
};
```
**Location**: `/include/interp.h:179`

**Adr Union**:
```c
union Adr {
    WORD    imm;    // Immediate value
    WORD    ind;    // Indirect offset
    Inst*   ins;    // Instruction pointer
    struct {
        ushort  f;  // First indirection
        ushort  s;  // Second indirection
    } i;
};
```

---

#### REG - Virtual Machine Registers
```c
struct REG {
    Inst*   PC;     // Program counter
    uchar*  MP;     // Module pointer
    uchar*  FP;     // Frame pointer
    uchar*  SP;     // Stack pointer
    uchar*  TS;     // Top of stack
    uchar*  EX;     // Stack extent
    Modlink* M;     // Current module
    int     IC;     // Instruction count
    Inst*   xpc;    // Saved PC
    void*   s;      // Source operand
    void*   d;      // Destination operand
    void*   m;      // Middle operand
    WORD    t;      // Middle temp
    WORD    st;     // Source temp
    WORD    dt;     // Destination temp
};
```
**Location**: `/include/interp.h:213`

---

#### Prog - Program Thread
```c
struct Prog {
    REG         R;      // Register set
    Prog*       link;   // Run queue link
    Channel*    chan;   // Channel pointer
    void*       ptr;    // Channel data pointer
    enum ProgState state; // Scheduler state
    char*       kill;   // Kill reason
    char*       killstr; // Kill string buffer
    int         pid;    // Unique process ID
    int         quanta; // Time slice
    ulong       ticks;  // Time used
    int         flags;  // Error recovery flags
    Prog*       prev;   // Previous in queue
    Prog*       next;   // Next in queue
    Prog*       pidlink; // PID hash chain
    Progs*      group;  // Process group
    Prog*       grpprev; // Previous in group
    Prog*       grpnext; // Next in group
    void*       exval;  // Current exception
    char*       exstr;  // Exception string
    void (*addrun)(Prog*); // Add to run queue
    void (*xec)(Prog*);   // Interpreter function
    void*       osenv;  // OS-specific data
};
```
**Location**: `/include/interp.h:243`

---

## 9. Constant Definitions

### Magic Numbers

```c
#define XMAGIC   819248   // Unsigned module
#define SMAGIC   923426   // Signed module
```
**Location**: `/include/isa.h:188`

### Address Mode Masks

```c
// Source/Destination modes
#define AMP     0x00      // Offset from MP
#define AFP     0x01      // Offset from FP
#define AIMM    0x02      // Immediate
#define AXXX    0x03      // None
#define AIND    0x04      // Double indirect
#define AMASK   0x07      // Mode mask

// Derived values
#define AOFF    0x08      // Offset flag
#define AVAL    0x10      // Value flag

// Source/Destination packing
#define SRC(x)  ((x)<<3)
#define DST(x)  ((x)<<0)
#define USRC(x) (((x)>>3)&AMASK)
#define UDST(x) ((x)&AMASK)
```
**Location**: `/include/isa.h:191`

### Middle Operand Modes

```c
#define ARM     0xC0      // Middle operand mask
#define AXNON   0x00      // No middle operand
#define AXIMM   0x40      // Immediate
#define AXINF   0x80      // Offset from FP
#define AXINM   0xC0      // Offset from MP
```
**Location**: `/include/isa.h:200`

### Data Encoding Types

```c
#define DEFZ    0         // Zero / end
#define DEFB    1         // Byte
#define DEFW    2         // Word
#define DEFS    3         // String
#define DEFF    4         // Float
#define DEFA    5         // Array
#define DIND    6         // Set array index
#define DAPOP   7         // Restore index
#define DEFL    8         // Big integer
#define DEFSS   9         // String share (reserved)
```
**Location**: `/include/isa.h:206`

### Data Encoding Macros

```c
#define DTYPE(x)    (x>>4)           // Extract type
#define DBYTE(x, l) ((x<<4)|l)       // Compose byte
#define DMAX        (1<<4)           // Max count
#define DLEN(x)     (x & (DMAX-1))   // Extract length
```
**Location**: `/include/isa.h:239`

### Runtime Flags

```c
#define MUSTCOMPILE  (1<<0)          // Must JIT compile
#define DONTCOMPILE  (1<<1)          // Do not compile
#define SHAREMP      (1<<2)          // Share module data
#define DYNMOD       (1<<3)          // Dynamic C module
#define HASLDT0      (1<<4)          // Old imports (obsolete)
#define HASEXCEPT    (1<<5)          // Has exception handlers
#define HASLDT       (1<<6)          // Has import section
```
**Location**: `/include/isa.h:230`

### Register Indices

```c
#define REGLINK     0                // Link register
#define REGFRAME    1                // Frame pointer
#define REGMOD      2                // Module register
#define REGTYP      3                // Type register
#define REGRET      4                // Return register
#define NREG        5                // Number of registers
```
**Location**: `/include/isa.h:219`

### Size Constants

```c
#define IBY2WD      sizeof(intptr)   // Bytes per word (4)
#define IBY2FT      8                // Bytes per float
#define IBY2LG      8                // Bytes per long
```
**Location**: `/include/isa.h:226`

### Other Constants

```c
#define DADEPTH     4                // Array address stack depth
```
**Location**: `/include/isa.h:217`

---

## 10. Compiler Implementation Guidelines

This section provides practical guidance for implementing a frontend compiler that targets DIS.

### Instruction Selection

#### Choosing Addressing Modes

**Rule**: Use the most efficient mode that expresses your operation.

**Guidelines**:

1. **Local Variables** → Use FP offsets
   ```
   addw    4(fp), 8(fp), 12(fp)   # local = local1 + local2
   ```

2. **Global Variables** → Use MP offsets
   ```
   movw    $42, 16(mp)            # global = 42
   ```

3. **Constants** → Use immediate mode
   ```
   addw    $10, x(fp), x(fp)      # x += 10
   ```

4. **Array Access** → Use indirect modes
   ```
   indx    arr(fp), $0, temp       # Get address
   movw    temp, value(fp)         # Dereference
   ```

5. **Double Indirect** → For pointers to data
   ```
   movw    4(8(mp)), dst(fp)      # Load through pointer
   ```

#### Type-Specific Variants

Always use the type-specific instruction variant matching your data:

```
Integer arithmetic:    addw, subw, mulw, divw
Float arithmetic:      addf, subf, mulf, divf
Long arithmetic:       addl, subl, mull, divl
Byte operations:       addb, subb, movb
```

**Wrong**:
```
addw    byte_var(fp), $5, dst   # Don't use word op on bytes!
```

**Correct**:
```
addb    byte_var(fp), $5, dst
```

### Code Generation

#### Function Prologue

**Standard Pattern**:
```
function:
    frame   type_desc, new_frame    # Allocate frame
    movw    $0, new_frame+0         # Initialize locals
    movp    H, new_frame+4          # Clear pointers
    ...                             # Function body
    ret                             # Return
```

#### Function Epilogue

**Simple Return**:
```
    ret
```

**Return with Value**:
```
    movw    retval(fp), ret_reg(fp) # Copy to return slot
    ret
```

#### Global Variable Access

**Read Global**:
```
    movw    global_offset(mp), local(fp)
```

**Write Global**:
```
    movw    local(fp), global_offset(mp)
```

#### Control Flow

**If-Then-Else**:
```
    beqw    condition(fp), $0, then_label
    # Else branch
    ... else code ...
    jmp     end_label
then_label:
    # Then branch
    ... then code ...
end_label:
```

**Loop**:
```
loop_top:
    ... loop body ...
    beqw    counter(fp), $0, loop_exit
    addw    $-1, counter(fp), counter(fp)
    jmp     loop_top
loop_exit:
```

**Switch Statement**:
```
    case    value(fp), jump_table
jump_table:
    word    nentries
    word    lo1, hi1, pc1
    word    lo2, hi2, pc2
    ...
    word    default_pc
```

### Type System Mapping

#### Mapping Language Types to DIS

| Language Type | DIS Type | Instructions |
|---------------|----------|--------------|
| int8/byte | B | addb, subb, movb |
| int16 | W† | cvtws, cvtsw |
| int32/int | W | addw, subw, movw |
| int64/long | L | addl, subl, movl |
| float32 | F† | cvtfr, cvtrf |
| float64/double | F | addf, subf, movf |
| pointer | P | movp, lea |
| string | C | addc, lenc, slicec |
| array | array | newa, lena, indx |
| list | list | cons, head, tail |

† Limited support (conversions only)

#### Type Descriptor Generation

For each composite type in your language, generate a type descriptor:

**C Struct Example**:
```c
struct Node {
    int value;
    struct Node *next;
    char *name;
};
```

**DIS Type Descriptor**:
```
size = 12 bytes
map = {0x16, 0x00}
// Binary: 00010110 00000000
// Bit 1: offset 4 (next pointer)
// Bit 2: offset 8 (name pointer)
```

**Generation Algorithm**:
```
for each word_offset in type:
    if field is pointer:
        set bit (word_offset / 8) at position (word_offset % 8)
```

#### Signature Calculation

Function signatures are hashed for type checking across modules.

**Algorithm** (simplified):
```
signature = MD5(function_name + param_types + return_type)
```

### Module Creation

#### Required Sections

Every .dis file must have:
1. Header (with magic, sizes, entry point)
2. Code section (at least 1 instruction, even if nop)
3. Type section (descriptor 0 = module data type)
4. Data section (terminated by 0 byte)
5. Module name (null-terminated string)
6. Link section (exported functions, if any)

#### Entry Point Specification

**Header Fields**:
```
entry_pc:   Instruction index of entry function
entry_type: Type descriptor of entry frame
```

**If No Entry** (e.g., library module):
```
entry_pc = 0
entry_type = 0
```

**Module Data Type** (Descriptor 0):
```
Must describe entire MP region
Size must match header's data_size
```

#### Export/Import Handling

**Exporting Functions**:
```
Link_item {
    pc:     function's instruction index
    desc:   function's frame type (-1 if none)
    sig:    MD5 of signature
    name:   "function_name\0"
}
```

**Importing Functions**:
```
In source:
    load    "$module", linkage_table, mod_link

Linkage table in data section:
    word    nentries
    struct {
        word    sig;
        bytes   "function_name\0";
    } entry[nentries];
```

**Calling Import**:
```
    mframe  mod_link, link_index, frame
    mcall   frame, link_index, mod_link
```

### Optimization Opportunities

#### Constant Folding

**Before**:
```
    movw    $10, temp(fp)
    addw    $20, temp(fp), temp(fp)
```

**After**:
```
    movw    $30, temp(fp)
```

#### Common Subexpression Elimination

**Before**:
```
    indx    arr(fp), $i, addr1(fp)
    movw    addr1(fp), x(fp)
    indx    arr(fp), $i, addr2(fp)
    movw    addr2(fp), y(fp)
```

**After**:
```
    indx    arr(fp), $i, addr(fp)
    movw    addr(fp), x(fp)
    movw    addr(fp), y(fp)
```

#### Dead Code Elimination

Remove unreachable code after:
- Exit/return instructions
- Unconditional jumps
- Conditional branches where condition is constant

#### Register Allocation

DIS doesn't have general-purpose registers, but you can minimize temporary accesses:

**Less Efficient**:
```
    movw    x(fp), temp(fp)
    addw    temp(fp), $5, temp(fp)
    movw    temp(fp), y(fp)
```

**More Efficient** (use middle operand):
```
    addw    $5, x(fp), y(fp)
```

---

## 11. Runtime Integration

### Built-in Functions

DIS modules can call functions from built-in system modules.

#### System Modules

Common modules in TaijiOS/Inferno:
- **Sys**: Basic system calls
- **Draw**: Graphics operations
- **Keyring**: Cryptography
- **Prefab**: UI widgets
- **Math**: Extended math functions

#### Calling Built-ins

**Load Module**:
```
    load    "/dis/lib/math.dis", linkage, math_mod
```

**Call Function**:
```
    mframe  math_mod, MATH_sin_index, frame
    mcall   frame, MATH_sin_index, math_mod
```

**Linkage Declaration**:
```
linkage:
    word    1                          # 1 function
    word    MD5("sin(real):real")     # Signature
    byte    "sin", 0                  # Name
```

### Exception Handling

#### Raising Exceptions

```
    raise   exception_string
```

**In Data Section**:
```
DEFS    offset=0
"EOF\0"
```

**In Code**:
```
    movp    $EOF_msg(fp), src(fp)
    raise   src(fp)
```

#### Declaring Handlers

**Handler Table Format**:
```
handler {
    offset:     frame_offset_of_exception_struct
    pc_start:   first_pc_covered
    pc_end:     first_pc_not_covered
    type_desc:  type_to_destroy (-1 if none)
    num_labels: (declared << 16) | nlab
    ... labels ...
    {
        string: exception_name
        pc:     handler_pc
    }[nlab]
    wildcard_pc: default_handler_pc_or_-1
}
```

**Handler Example**:
```
    .text
func_with_handler:
    frame   type_desc, frame_ptr
    ... function body ...
    ret

    .data
exception_handlers:
    word    1                        # 1 handler

    # Handler 0
    word    8                        # exception at fp+8
    word    0                        # pc_start
    word    100                      # pc_end
    word    -1                       # no type to destroy
    word    (1 << 16) | 2           # 1 declared, 2 total

    # Label 0
    byte    "Eof", 0
    word    eof_handler

    # Label 1
    byte    "*", 0
    word    generic_handler

eof_handler:
    ... handle EOF ...
    ret
generic_handler:
    ... handle others ...
    ret
```

### Concurrency

#### Spawning Threads

**Simple Spawn** (same module):
```
    frame   frame_type, new_frame
    ... initialize args ...
    spawn   new_frame, func_pc
```

**Module Spawn** (other module):
```
    mframe  mod_link, link_idx, frame
    ... initialize args ...
    mspawn  frame, link_idx, mod_link
```

#### Channel Communication

**Create Channel**:
```
    newcw   chan_ptr(fp)
```

**Send**:
```
    movw    value(fp), temp(fp)
    send    temp(fp), chan_ptr(fp)
```

**Receive**:
```
    recv    chan_ptr(fp), recv_ptr(fp)
```

**Non-Deterministic Choice** (Alt):
```
    # Build alt structure
    movw    $1, alt_struct(fp)        # nsend = 1
    movw    $1, alt_struct+4(fp)      # nrecv = 1
    movp    chan1(fp), alt_struct+8(fp)  # send chan
    movp    value1(fp), alt_struct+12(fp) # send value
    movp    chan2(fp), alt_struct+16(fp)  # recv chan
    movp    recv_buf(fp), alt_struct+20(fp) # recv addr

    # Perform alt
    movw    $alt_struct(fp), src(fp)
    alt     src(fp), selected(fp)

    # Check result
    beqw    selected(fp), $0, send_case
    # recv case - data already in recv_buf
    jmp     done
send_case:
    # send case - value already sent
done:
```

**Non-Blocking Alt**:
```
    nbalt   src(fp), selected(fp)
    beqw    selected(fp), $-1, no_channel_ready
    ... handle communication ...
no_channel_ready:
    ... no channels ready ...
```

---

## 12. Debugging and Tools

### Disassembly Format

The `dis` library module provides disassembly capabilities.

**Function**: `inst2s(ref Inst): string`

**Example Output**:
```
movw    $42, 4(fp)
addw    8(fp), 12(fp), 16(fp)
call    frame_ptr(fp), entry_point
ret
```

### Symbol Tables

Symbol tables (.sbl files) contain debugging information not in the .dis file.

**Format** (from `man 6 sbl`):
- Line number information
- Local variable names
- Function signatures
- Type information

### Available Tools

**dis** - Disassemble .dis files
```
dis module.dis
```

**disdep** - Show module dependencies
```
disdep module.dis
```

**limbo** - The Limbo compiler
```
limbo -o module.dis module.b
```

**asm** - DIS assembler (if available)
```
asm -o module.dis module.s
```

### Debug Information Format

**Line Number Table** (optional):
```
struct LineEntry {
    word pc;          // Instruction index
    word line;        // Source line number
    word file;        // File index
};
```

**Local Variable Table** (optional):
```
struct LocalVar {
    word offset;      // Frame offset
    word start_pc;    // Live range start
    word end_pc;      // Live range end
    string name;      // Variable name
};
```

---

## 13. Implementation Examples

### Example 1: Simple Function

**Source** (C-like):
```c
int add(int a, int b) {
    return a + b;
}
```

**DIS Code**:
```
# Type descriptor for frame
# size = 12 bytes (3 words)
# map = {0x00} (no pointers)
desc    $0, 12, "\x00"

add:
    # Frame layout: [return, a, b]
    # Arguments already in frame
    addw    4(fp), 8(fp), 12(fp)   # ret = a + b
    ret
```

**Generated Instruction Stream**:
```
B(0x40)        # addw
B(0x95)        # AFP|AMP|AXNON
OP(4)          # src1: offset 4 from FP
B(0x2A)        # AFP|AIMM|AXNON
OP(8)          # src2: offset 8 from FP
B(0x01)        # AMP|AIMM|AXNON
OP(12)         # dst: offset 12 from FP

B(0x8C)        # ret
B(0x00)        # all none
```

---

### Example 2: Loop

**Source** (C-like):
```c
int sum_array(int *arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}
```

**DIS Code**:
```
# Type descriptor: [ret, arr, n, sum, i]
# size = 20 bytes
# map = {0x02} (arr is pointer at offset 4)
desc    $0, 20, "\x02"

sum_array:
    frame   $0, new_frame       # Allocate frame
    movw    $0, 12(new_frame)   # sum = 0
    movw    $0, 16(new_frame)   # i = 0

loop_top:
    movw    16(new_frame), temp
    cmpw    temp, 8(new_frame)  # compare i and n
    bge     temp, 8(new_frame), loop_end

    # sum += arr[i]
    movp    4(new_frame), arr_ptr
    indx    arr_ptr, 16(new_frame), elem_addr
    movw    elem_addr, elem_val
    addw    elem_val, 12(new_frame), 12(new_frame)

    # i++
    addw    $1, 16(new_frame), 16(new_frame)
    jmp     loop_top

loop_end:
    movw    12(new_frame), ret(new_frame)  # return sum
    ret
```

---

### Example 3: Recursive Function

**Source** (C-like):
```c
int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}
```

**DIS Code**:
```
# Frame: [ret, n]
# size = 8 bytes
# map = {0x00}
desc    $0, 8, "\x00"

factorial:
    frame   $0, new_frame
    movw    4(old_frame), 4(new_frame)  # Copy argument

    # if (n <= 1) return 1
    movw    4(new_frame), temp
    blew    temp, $1, return_one

    # Compute factorial(n-1)
    subw    $1, 4(new_frame), arg_temp
    frame   $0, recurse_frame
    movw    arg_temp, 4(recurse_frame)
    call    recurse_frame, factorial
    movw    ret(recurse_frame), rec_result

    # return n * result
    mulw    4(new_frame), rec_result, ret(new_frame)
    ret

return_one:
    movw    $1, ret(new_frame)
    ret
```

---

### Example 4: Module with Exports

**Module: mathutils.dis**

**Header**:
```
# XMAGIC, no special flags
OP(819248)      # magic
OP(0)           # runtime_flag
OP(8192)        # stack_extent
OP(10)          # code_size
OP(0)           # data_size
OP(1)           # type_size
OP(2)           # link_size (2 exports)
OP(0)           # entry_pc
OP(0)           # entry_type
```

**Type Section**:
```
# Frame type for both functions
# size = 8 bytes (return + arg)
# map = {0x00}
OP(0)
OP(8)
OP(1)
B(0x00)
```

**Code Section**:
```
# square function
square:
    frame   $0, new_frame
    movw    4(new_frame), temp
    mulw    temp, temp, ret(new_frame)
    ret

# cube function
cube:
    frame   $0, new_frame
    movw    4(new_frame), temp
    mulw    temp, temp, temp2
    mulw    temp2, temp, ret(new_frame)
    ret
```

**Module Name**:
```
"mathutils\0"
```

**Link Section**:
```
# Export 1: square
OP(0)               # pc = instruction 0
OP(0)               # type descriptor 0
W(0xABCD1234)       # signature MD5
"square\0"

# Export 2: cube
OP(5)               # pc = instruction 5
OP(0)               # type descriptor 0
W(0x56789ABC)       # signature MD5
"cube\0"
```

**Using from Another Module**:
```
    # In data section:
linkage:
    word    2               # 2 imports
    word    MD5("square(w):w")
    byte    "square", 0
    word    MD5("cube(w):w")
    byte    "cube", 0

    # In code section:
    load    "mathutils.dis", linkage, math_mod

    # Call square
    mframe  math_mod, 0, frame
    movw    $5, 4(frame)
    mcall   frame, 0, math_mod
    # Result in ret(frame)
```

---

## 14. Appendices

### Appendix A: Complete Opcode Quick Reference

#### Alphabetical Listing

| Opcode | Name | Description |
|--------|------|-------------|
| IADD* | Add | Addition |
| IALT | Alt | Alternate communication |
| IAND* | And | Bitwise AND |
| IBEQ* | Beq | Branch if equal |
| IBGE* | Bge | Branch if greater or equal |
| IBGT* | Bgt | Branch if greater |
| IBLE* | Ble | Branch if less or equal |
| IBLT* | Blt | Branch if less |
| IBNE* | Bne | Branch if not equal |
| ICALL | Call | Call local function |
| ICASE | Case | Case (word) |
| ICASEC | Casec | Case (string) |
| ICASEL | Casel | Case (long) |
* continues for all 182 opcodes... |

#### Numerical Listing

```
0x00:  INOP       0x40:  IHEADW     0x80:  IBEQB
0x01:  IALT       0x41:  IMULW      0x81:  IBLEB
0x02:  INBALT     0x42:  IMULF      0x82:  IBGTB
...
0xB4:  IMULX1     0xB5:  IDIVX1     0xB6:  ICVTXX1
0xB7:  ICVTFX     0xB8:  ICVTXF     0xB9:  IEXPW
0xBA:  IEXPL      0xBB:  IEXPF      0xBC:  ISELF
```

#### Type Suffix Matrix

| Op \ Type | B | W | F | L | C | P | M | MP |
|----------|---|---|---|---|---|---|---|----|
| Add      | X | X | X | X | - | - | - | - |
| Sub      | X | X | X | X | - | - | - | - |
| Mul      | X | X | X | X | - | - | - | - |
| Div      | X | X | X | X | - | - | - | - |
| Mod      | X | X | - | X | - | - | - | - |
| Mov      | X | X | X | X | - | X | X | X |
| ...      |   |   |   |   |   |   |   |   |

---

### Appendix B: Address Mode Encoding

#### Bit Patterns

**Middle Operand** (2 bits):
```
00: AXNON (none)
01: AXIMM ($imm)
10: AXINF n(fp)
11: AXINM n(mp)
```

**Source/Destination** (3 bits):
```
000: AMP n(mp)
001: AFP n(fp)
010: AIMM $imm
011: AXXX none
100: AIND|AMP n(m(mp))
101: AIND|AFP n(m(fp))
110: reserved
111: reserved
```

#### Decoding Algorithms

**Decode Middle**:
```c
mid_mode = (addr_byte >> 6) & 0x03;
switch(mid_mode) {
    case 0x00: /* none */ break;
    case 0x40: /* immediate */ read_operand(); break;
    case 0x80: /* offset from FP */ read_operand(); break;
    case 0xC0: /* offset from MP */ read_operand(); break;
}
```

**Decode Source**:
```c
src_mode = (addr_byte >> 3) & 0x07;
switch(src_mode) {
    case 0x00: /* offset from MP */ read_operand(); break;
    case 0x01: /* offset from FP */ read_operand(); break;
    case 0x02: /* immediate */ read_operand(); break;
    case 0x03: /* none */ break;
    case 0x04: /* double ind MP */ read_operand(); read_operand(); break;
    case 0x05: /* double ind FP */ read_operand(); read_operand(); break;
}
```

**Decode Destination**:
```c
dst_mode = addr_byte & 0x07;
/* Same as source decode */
```

---

### Appendix C: Type Descriptor Examples

#### Scalar Types

**Byte**:
```
size = 1
map_size = 0
```

**Word**:
```
size = 4
map_size = 0
```

**Pointer**:
```
size = 4
map_size = 1
map = {0x80}  # Binary: 10000000
```

#### Structure Types

**Linked List Node**:
```c
struct Node {
    int value;
    struct Node *next;
};
```
```
size = 8
map_size = 1
map = {0x40}  # Binary: 01000000 (bit 1 = offset 4)
```

**Binary Tree Node**:
```c
struct TreeNode {
    int key;
    void *value;
    struct TreeNode *left;
    struct TreeNode *right;
};
```
```
size = 16
map_size = 2
map = {0x6C, 0x00}
# Byte 0: 01101100 (pointers at offsets 4, 8, 12)
# Byte 1: 00000000
```

#### Array Types

**Array of Words** (element type):
```
element_size = 4
map_size = 0  # No pointers in words
```

**Array of Strings** (element type):
```
element_size = 4
map_size = 1
map = {0xFF}  # All words are pointers
```

#### Function Frame

**Simple Frame** (return + 2 args):
```
size = 12
map_size = 1
map = {0x00}  # No pointers (assuming scalar args)
```

**Frame with Pointers**:
```c
// return value, pointer arg, int arg, local pointer
size = 16
map_size = 2
map = {0x14, 0x00}
# Byte 0: 00010100 (pointers at offsets 4, 12)
```

---

### Appendix D: Object File Examples

#### Minimal Module

```
# Header
OP(819248)    # XMAGIC
OP(0)         # flags
OP(8192)      # stack_extent
OP(1)         # 1 instruction
OP(0)         # no data
OP(1)         # 1 type
OP(0)         # no exports
OP(0)         # entry PC
OP(0)         # entry type

# Code
B(0x00)       # nop
B(0x00)       # all none

# Type (descriptor 0 = module data)
OP(0)         # id 0
OP(0)         # size 0 (no data)
OP(0)         # no map

# Data
B(0x00)       # End

# Module name
"minimal\0"

# Link section (empty - 0 items)
```

#### Hello World

**Header**: (similar to above, with appropriate sizes)

**Code**:
```
B(0x30)       # newa
B(0x42)       # AIMM, AXXX, AXNON
OP(13)        # Array length
B(0x41)       # AMP, AIMM, AXNON
OP(0)         # Type 0 (array of byte)
B(0x01)       # AMP, AXXX, AXNON
OP(4)         # Store at offset 4 in MP

B(0x10)       # new
B(0x02)       # AIMM, AXXX, AXNON
OP(1)         # Type 1 (string)
B(0x01)       # AMP, AXXX, AXNON
OP(8)         # Store at offset 8 in MP

B(0x8C)       # ret
B(0x00)       # all none
```

**Types**:
```
# Type 0: Array of byte
OP(0)
OP(1)
OP(0)

# Type 1: String (forSys->print)
OP(1)
OP(12)
OP(1)
B(0x80)       # Has pointer
```

**Data**:
```
B(0x21)       # DEFW count=1
OP(0)         # offset 0
W(0x48656C6C) # "Hell"
W(0x6F20776F) # "o wo"
W(0x726C6421) # "rld!"
```

---

### Appendix E: Instruction Encoding Examples

#### Example 1: `addw $5, 10(fp), 20(fp)`

**Address Mode Byte**:
- Middle: AXIMM (01)
- Source: AFP (001)
- Dest: AFP (001)

```
addr = 0b01_001_001 = 0x49
```

**Encoding**:
```
B(0x40)       # addw opcode
B(0x49)       # address mode
OP(5)         # middle: $5
OP(10)        # source: offset 10
OP(20)        # dest: offset 20
```

#### Example 2: `movw 4(mp), 8(fp)`

**Address Mode Byte**:
- Middle: AXNON (00)
- Source: AMP (000)
- Dest: AFP (001)

```
addr = 0b00_000_001 = 0x01
```

**Encoding**:
```
B(0x21)       # movw opcode
B(0x01)       # address mode
OP(4)         # source: offset 4 from MP
OP(8)         # dest: offset 8 from FP
```

#### Example 3: `call frame_ptr(fp), entry_point`

**Address Mode Byte**:
- Middle: AXNON (00)
- Source: AFP (001)
- Dest: AIMM (010)

```
addr = 0b00_001_010 = 0x0A
```

**Encoding**:
```
B(0x04)       # call opcode
B(0x0A)       # address mode
OP(frame_offset)  # source: frame from FP
OP(entry_pc)      # dest: entry point PC
```

---

### Appendix F: Glossary

**ADT**: Abstract Data Type

**Alt**: Non-deterministic choice between channels (CSP)

**Compiler**: Translator from source language to DIS bytecode

**Descriptor**: Type metadata for garbage collection

**Frame**: Activation record on stack

**Heap**: Dynamically allocated memory

**JIT**: Just-In-Time compilation to native code

**Link**: Connection between modules (import/export)

**Module**: Unit of compilation and loading

**MP**: Module Pointer (global data)

**Operand**: Value operated on by instruction

**PC**: Program Counter

**Type**: Data layout and pointer map description

**VM**: Virtual Machine

---

### Appendix G: References

1. **"The Dis Virtual Machine Specification"** - Primary technical document
   Location: `/doc/dis.ms`, `/doc/dis.pdf`

2. **Inferno Programmer's Manual** (Volume 1 & 2)
   - Volume 1: Manual pages and user documentation
   - Volume 2: Architecture papers

3. **"The Design of the Inferno Virtual Machine"** - Winterbottom and Pike
   Discusses design rationale and architecture decisions

4. **Source Code**:
   - `/include/isa.h` - Instruction set definitions
   - `/include/interp.h` - Runtime structures
   - `/libinterp/xec.c` - Instruction interpreter
   - `/libinterp/load.c` - Module loader
   - `/limbo/dis.c` - Code generator
   - `/module/dis.m` - Limbo API

5. **Man Pages**:
   - `man 2 dis` - Dis module API
   - `man 6 dis` - Object file format
   - `man 6 sbl` - Symbol table format

---

### Appendix H: Implementation Checklist

For frontend compiler writers:

**Phase 1: Code Generation**
- [ ] Implement instruction selection
- [ ] Generate proper addressing modes
- [ ] Create type descriptors
- [ ] Emit valid instruction encodings

**Phase 2: Module Structure**
- [ ] Generate valid header
- [ ] Create code section
- [ ] Create type section
- [ ] Create data section
- [ ] Create link section (exports)
- [ ] Add module name

**Phase 3: Memory Management**
- [ ] Use correct move instructions
- [ ] Implement proper refcounting
- [ ] Generate pointer maps
- [ ] Handle initialization

**Phase 4: Control Flow**
- [ ] Implement function calls
- [ ] Implement returns
- [ ] Generate branches
- [ ] Handle loops

**Phase 5: Interfacing**
- [ ] Module loading
- [ ] Import handling
- [ ] Export declarations
- [ ] Signature calculation

**Phase 6: Advanced Features**
- [ ] Exception handling
- [ ] Concurrency (spawn, channels)
- [ ] Array operations
- [ ] String operations

---

## Conclusion

This document provides a comprehensive reference for the DIS virtual machine. All information necessary to implement a frontend compiler targeting DIS has been included:

- **Complete instruction set** (all 182 opcodes)
- **Object file format** with encoding examples
- **Memory and execution models**
- **Data structures** from the implementation
- **Compiler guidelines** with examples
- **Runtime integration** details

For further information, consult the source code in:
- `/include/isa.h` - Constants and opcodes
- `/include/interp.h` - Data structures
- `/libinterp/xec.c` - Instruction implementation
- `/libinterp/load.c` - Module loading
- `/doc/dis.ms` - Formal specification

---

**Document Version**: 1.0
**Date**: 2025-01-27
**For**: TaijiOS / Inferno DIS Virtual Machine
**Purpose**: Frontend language compiler implementation
