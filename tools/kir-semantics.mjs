// Checked KIR semantic encoder and evaluator (law plan phase 3 first patch).
//
// Parses the k2kir inspection dump, encodes a *restricted* deterministic
// subset of typed KIR function bodies and evaluates them with exact i32
// semantics. Every construct outside the reviewed subset is rejected with a
// source span before evaluation; nothing is guessed. The vertical case for
// this patch: integer literals, identifiers, binary + - * / %, unary minus,
// scalar i32 declarations, assignments and return.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

export const INT32_MIN = -(2n ** 31n);
export const INT32_MAX = 2n ** 31n - 1n;

export function wrapInt32(value) {
    const modulo = 2n ** 32n;
    let wrapped = ((value % modulo) + modulo) % modulo;
    if (wrapped > INT32_MAX) wrapped -= modulo;
    return wrapped;
}

export class Unsupported extends Error {
    constructor(message, span) {
        super(`kir-semantics: unsupported: ${message}${span ? ` (${span})` : ''}`);
        this.span = span;
    }
}

const STMT_LINE = /^ {4}stmt (\S+) widget (.*?) args (.*?) text (.*?) span (\S+)/;
const FN_LINE = /^ {2}function (\S+) args (.*?) return (\S+) exported (\d+) span (\S+)/;
const EXPR_LINE = /^ +expr (\S+) text (.*?) name (.*?) op (.*?) span (\S+)(?: type (\S+))?$/;

function parseArgs(text) {
    if (!text.trim()) return [];
    return text.split(',').map(part => {
        const colon = part.indexOf(':');
        if (colon < 0) throw new Unsupported(`untyped parameter ${part.trim()}`);
        return { name: part.slice(0, colon).trim(), type: part.slice(colon + 1).trim() };
    });
}

export function parseKirDump(text) {
    if (!text.startsWith('kir 1\n')) throw new Error('kir-semantics: unknown dump format');
    const modules = [];
    let module = null;
    let fn = null;
    // Indentation stack for rebuilding the expression tree.
    let stack = [];
    for (const line of text.split('\n')) {
        if (line.startsWith('module ')) {
            module = { name: line.split(' ')[1], functions: [] };
            modules.push(module);
            fn = null;
        } else if (FN_LINE.test(line)) {
            const m = FN_LINE.exec(line);
            fn = {
                name: m[1], args: parseArgs(m[2]), returnType: m[3],
                exported: m[4] === '1', span: m[5], stmts: [],
            };
            module.functions.push(fn);
        } else if (STMT_LINE.test(line)) {
            const m = STMT_LINE.exec(line);
            fn.stmts.push({ kind: m[1], text: m[4], span: m[5], expr: null });
            stack = [];
        } else if (EXPR_LINE.test(line) && fn) {
            const m = EXPR_LINE.exec(line);
            const indent = (line.length - line.trimStart().length) / 2;
            const node = {
                kind: m[1], text: m[2], name: m[3], op: m[4],
                span: m[5], type: m[7] ?? '', children: [],
            };
            while (stack.length && stack[stack.length - 1].indent >= indent) stack.pop();
            if (stack.length === 0) {
                const stmt = fn.stmts[fn.stmts.length - 1];
                if (stmt.expr === null) stmt.expr = node;
                else if (stmt.lhs === undefined) stmt.lhs = node; // assignment destination
                else throw new Unsupported('three root expressions for a statement', node.span);
            } else {
                stack[stack.length - 1].node.children.push(node);
            }
            stack.push({ indent, node });
        }
    }
    return { modules };
}

const BINARY_OPS = new Set(['+', '-', '*', '/', '%']);
const SCALAR_TYPES = new Set(['i32', 'int', 'i8', 'i16', 'u8', 'u16', 'u32']);

export function findFunction(program, name) {
    for (const module of program.modules) {
        for (const fn of module.functions) {
            if (fn.name === name) return fn;
        }
    }
    throw new Unsupported(`function ${name} not found in dump`);
}

// Encode one function into the reviewed subset. Rejects everything else.
export function encodeChecked(fn) {
    const declName = stmt => {
        const m = /^([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(\S+)\s*=/.exec(stmt.text);
        if (!m) throw new Unsupported(`declaration form not in subset: "${stmt.text}"`, stmt.span);
        if (!SCALAR_TYPES.has(m[2])) throw new Unsupported(`type ${m[2]} not in subset`, stmt.span);
        return { name: m[1], type: m[2] };
    };
    const encodeExpr = node => {
        switch (node.kind) {
            case 'int':
                return { op: 'lit', value: BigInt(node.text.trim()), span: node.span };
            case 'ident':
                return { op: 'var', name: node.name, span: node.span };
            case 'unary':
                if (node.op !== '-') throw new Unsupported(`unary ${node.op} not in subset`, node.span);
                if (node.children.length !== 1) throw new Unsupported('unary arity', node.span);
                return { op: 'neg', value: encodeExpr(node.children[0]), span: node.span };
            case 'binary':
                if (!BINARY_OPS.has(node.op)) throw new Unsupported(`binary ${node.op} not in subset`, node.span);
                if (node.children.length !== 2) throw new Unsupported('binary arity', node.span);
                return {
                    op: 'bin', kind: node.op,
                    left: encodeExpr(node.children[0]), right: encodeExpr(node.children[1]),
                    span: node.span,
                };
            default:
                throw new Unsupported(`expression kind ${node.kind}`, node.span);
        }
    };
    const body = [];
    for (const stmt of fn.stmts) {
        switch (stmt.kind) {
            case 'decl': {
                const declared = declName(stmt);
                if (stmt.expr === null) throw new Unsupported('declaration without initializer', stmt.span);
                body.push({ stmt: 'decl', name: declared.name, type: declared.type, expr: encodeExpr(stmt.expr), span: stmt.span });
                break;
            }
            case 'assign': {
                const m = /^([A-Za-z_][A-Za-z0-9_]*)\s*=/.exec(stmt.text);
                if (!m) throw new Unsupported(`assignment form not in subset: "${stmt.text}"`, stmt.span);
                if (stmt.expr === null) throw new Unsupported('assignment without expression', stmt.span);
                body.push({ stmt: 'assign', name: m[1], expr: encodeExpr(stmt.expr), span: stmt.span });
                break;
            }
            case 'return':
                if (stmt.expr === null) throw new Unsupported('bare return not in subset', stmt.span);
                body.push({ stmt: 'return', expr: encodeExpr(stmt.expr), span: stmt.span });
                break;
            default:
                throw new Unsupported(`statement kind ${stmt.kind}`, stmt.span);
        }
    }
    if (!body.some(s => s.stmt === 'return')) throw new Unsupported('function without return', fn.span);
    return { name: fn.name, args: fn.args, returnType: fn.returnType, body, span: fn.span };
}
// Evaluate an encoded function with exact two's-complement i32 semantics.
// Division and remainder follow C truncation toward zero.
export function evaluate(encoded, argumentValues) {
    if (argumentValues.length !== encoded.args.length) {
        throw new Unsupported(`arity mismatch for ${encoded.name}`);
    }
    const env = new Map();
    encoded.args.forEach((arg, i) => env.set(arg.name, wrapInt32(BigInt(argumentValues[i]))));
    const evalExpr = node => {
        switch (node.op) {
            case 'lit': return node.value;
            case 'var': {
                if (!env.has(node.name)) throw new Unsupported(`unbound identifier ${node.name}`, node.span);
                return env.get(node.name);
            }
            case 'neg': return wrapInt32(-evalExpr(node.value));
            case 'bin': {
                const left = evalExpr(node.left);
                const right = evalExpr(node.right);
                if (node.kind === '+') return wrapInt32(left + right);
                if (node.kind === '-') return wrapInt32(left - right);
                if (node.kind === '*') return wrapInt32(left * right);
                if (right === 0n) throw new Unsupported('division by zero is a trap, not a value', node.span);
                return wrapInt32(left / right); // BigInt division truncates toward zero
            }
            default: throw new Unsupported(`encoded op ${node.op}`, node.span);
        }
    };
    for (const step of encoded.body) {
        if (step.stmt === 'decl' || step.stmt === 'assign') {
            if (step.stmt === 'assign' && !env.has(step.name)) {
                throw new Unsupported(`assignment to undeclared ${step.name}`, step.span);
            }
            env.set(step.name, evalExpr(step.expr));
        } else if (step.stmt === 'return') {
            return evalExpr(step.expr);
        }
    }
    throw new Unsupported(`no return reached in ${encoded.name}`, encoded.span);
}

// Run k2kir over one source file and return the parsed dump.
export function dumpProgram(binDir, root, source, outDir) {
    fs.mkdirSync(outDir, { recursive: true });
    const result = spawnSync(path.join(binDir, 'bin', 'k2kir'),
        ['--root', root, '-o', outDir, source], { encoding: 'utf8', timeout: 60000 });
    if (result.status !== 0) {
        throw new Error(`k2kir failed: ${result.stderr || result.error}`);
    }
    const dump = path.join(outDir, path.relative(root, source).replace(/\.kry$/, '.kir'));
    if (!fs.existsSync(dump)) throw new Error(`k2kir produced no dump at ${dump}`);
    return parseKirDump(fs.readFileSync(dump, 'utf8'));
}
