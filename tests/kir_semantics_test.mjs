// Phase 3 vertical case: the checked KIR semantic evaluator agrees with the
// production k2c lowering over exact i32 semantics, rejects constructs
// outside the reviewed subset, and detects operand-order mutations.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';
import {
    dumpProgram, findFunction, encodeChecked, evaluate, Unsupported, wrapInt32,
} from '../tools/kir-semantics.mjs';

const root = path.resolve(import.meta.dirname, '..');
const generated = path.resolve(root,
    process.env.KRYON_LAW_GENERATED_DIR || 'build/linux-x86_64/generated/src');
const binDir = path.resolve(generated, '..', '..');
const fixture = path.join(root, 'tests/fixtures/kir_semantics.kry');

function temporary(run) {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'kryon-kir-semantics-'));
    return Promise.resolve().then(() => run(directory)).finally(() => {
        fs.rmSync(directory, { recursive: true, force: true });
    });
}

function run(command, args, options = {}) {
    const result = spawnSync(command, args, { encoding: 'utf8', timeout: 60000, ...options });
    assert.ifError(result.error);
    assert.equal(result.status, 0, `${command}: ${result.stdout}\n${result.stderr}`);
    return result;
}

const cases = [
    [0, 0], [1, 2], [7, 3], [-7, 3], [-7, 2], [100, -7],
    [2147483647, 1], [-2147483648, 1], [-2147483648, 2], [12345, -999],
];

async function groundTruth(directory) {
    // Production truth: lower the fixture with k2c and execute it.
    const out = path.join(directory, 'out');
    run(path.join(binDir, 'bin', 'k2c'),
        ['--strict', '--no-main', '--root', root, '-o', out, fixture]);
    const driver = path.join(directory, 'driver.c');
    const calls = ['Add', 'Sub', 'Div', 'Temp', 'Max', 'Clamp', 'Branch'].flatMap(name =>
        cases.filter(([a, b]) => name !== 'Div' || b !== 0)
            .map(([a, b]) => `    printf("%d\\n", KirSem${name}(${a}, ${b}));`)).join('\n');
    fs.writeFileSync(driver, `#include <stdio.h>
#include "tests/fixtures/kir_semantics.h"
int main(void) {
${calls}
    return 0;
}
`);
    const executable = path.join(directory, 'truth');
    run(process.env.CC || 'cc', ['-std=c99', '-O0', '-Wall', '-Werror',
        '-I' + out, '-I' + path.join(root, 'include'), driver,
        path.join(out, 'tests/fixtures/kir_semantics.c'), '-lm', '-o', executable]);
    return run(executable, []).stdout.trim().split('\n').map(Number);
}

test('checked evaluator agrees with the k2c lowering over exact i32 semantics', async () => {
    await temporary(async directory => {
        const program = dumpProgram(binDir, root,
            fixture, path.join(directory, 'kir'));
        const truth = await groundTruth(directory);
        const encoders = {
            Add: encodeChecked(findFunction(program, 'KirSemAdd')),
            Sub: encodeChecked(findFunction(program, 'KirSemSub')),
            Div: encodeChecked(findFunction(program, 'KirSemDiv')),
            Temp: encodeChecked(findFunction(program, 'KirSemTemp')),
            Max: encodeChecked(findFunction(program, 'KirSemMax')),
            Clamp: encodeChecked(findFunction(program, 'KirSemClamp')),
            Branch: encodeChecked(findFunction(program, 'KirSemBranch')),
        };
        let index = 0;
        for (const name of ['Add', 'Sub', 'Div', 'Temp', 'Max', 'Clamp', 'Branch']) {
            for (const [a, b] of cases) {
                if (name === 'Div' && b === 0) {
                    // Division by zero is a trap on both sides; the evaluator
                    // rejects it rather than guessing a value.
                    assert.throws(() => evaluate(encoders[name], [a, b]), Unsupported);
                    continue;
                }
                const expected = truth[index++];
                const actual = evaluate(encoders[name], [a, b]);
                assert.equal(actual, BigInt(expected),
                    `${name}(${a}, ${b}): evaluator ${actual} vs native ${expected}`);
            }
        }
    });
});

test('constructs outside the reviewed subset are rejected with a span', async () => {
    await temporary(async directory => {
        const program = dumpProgram(binDir, root,
            fixture, path.join(directory, 'kir'));
        assert.throws(() => encodeChecked(findFunction(program, 'KirSemLoop')),
            error => error instanceof Unsupported
                && /statement kind while/.test(error.message)
                && /kir_semantics\.kry:\d+/.test(error.span));
    });
});

test('an operand-order mutation is detected against the native truth', async () => {
    await temporary(async directory => {
        const program = dumpProgram(binDir, root,
            fixture, path.join(directory, 'kir'));
        const truth = await groundTruth(directory);
        // Swap the non-commutative subtraction operands in the encoded tree.
        const encoded = encodeChecked(findFunction(program, 'KirSemSub'));
        const swapped = structuredClone(encoded);
        const ret = swapped.body.find(s => s.stmt === 'return');
        assert.equal(ret.expr.op, 'bin');
        [ret.expr.left, ret.expr.right] = [ret.expr.right, ret.expr.left];
        const diverged = cases.some(([a, b], i) =>
            evaluate(swapped, [a, b]) !== BigInt(truth.slice(0, cases.length)[i]));
        assert.ok(diverged, 'the operand swap must change at least one result');
    });
});

test('i32 wrapping is exact at the boundaries', () => {
    assert.equal(wrapInt32(2147483647n), 2147483647n);
    assert.equal(wrapInt32(2147483648n), -2147483648n);
    assert.equal(wrapInt32(-2147483649n), 2147483647n);
    assert.equal(wrapInt32(-2147483648n), -2147483648n);
});
