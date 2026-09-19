import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';
import { checkLaws } from '../tools/bend-laws.mjs';

const root = path.resolve(import.meta.dirname, '..');
const packageDir = path.join(root, 'laws/focus');
const generated = path.resolve(root,
    process.env.KRYON_LAW_GENERATED_DIR || 'build/linux-x86_64/generated/src');
const proofNames = ['no_tab_stays', 'tab_moves_forward', 'shift_tab_moves_backward'];

function temporary(run) {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'kryon-focus-laws-'));
    return Promise.resolve().then(() => run(directory)).finally(() => {
        fs.rmSync(directory, { recursive: true, force: true });
    });
}

function run(command, args) {
    const result = spawnSync(command, args, { encoding: 'utf8', timeout: 30000 });
    assert.ifError(result.error);
    assert.equal(result.status, 0, `${command}: ${result.stdout}\n${result.stderr}`);
    return result;
}

function nativeRows(checked) {
    assert.deepEqual(checked.laws, proofNames);
    const table = checked.table('main.tab_direction');
    assert.deepEqual(table.domains, [['False', 'True'], ['False', 'True']]);
    assert.equal(table.rows.length, 4);
    const booleans = new Map([['False', 0], ['True', 1]]);
    const directions = new Map([['main.Stay', 0], ['main.Forward', 1], ['main.Backward', -1]]);
    const seen = new Set();
    return table.rows.map(row => {
        assert.equal(row.arguments.length, 2);
        const args = row.arguments.map(name => {
            assert(booleans.has(name), `unmapped input: ${name}`);
            return booleans.get(name);
        });
        assert.deepEqual(row.value.fields, []);
        assert(directions.has(row.value.constructor), 'unmapped direction');
        const key = args.join(',');
        assert(!seen.has(key), 'duplicate input row');
        seen.add(key);
        return [...args, directions.get(row.value.constructor)];
    });
}

function compile(directory, source, rows, suffix) {
    const driver = path.join(directory, `driver-${suffix}.c`);
    fs.writeFileSync(driver, `#include "runtime/focus.h"
#include <stdio.h>
int main(void) {
    const int cases[][3] = {
${rows.map(row => `        {${row.join(', ')}},`).join('\n')}
    };
    for(unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int actual = FocusTabDirectionFor(cases[i][0] != 0, cases[i][1] != 0);
        if(actual != cases[i][2]) {
            fprintf(stderr, "focus.tab.direction: case %u: got %d, expected %d\\n",
                    i, actual, cases[i][2]);
            return 1;
        }
    }
    return 0;
}
`);
    const executable = path.join(directory, `check-${suffix}`);
    run(process.env.CC || 'cc', ['-std=c99', '-O2', '-DNDEBUG', '-Wall', '-Werror',
        '-I' + path.join(root, 'include'), '-I' + generated, driver, source, '-lm', '-o', executable]);
    return executable;
}

test('three checked focus laws agree with generated C for the complete Boolean domain', async () => {
    const rows = nativeRows(await checkLaws(path.join(packageDir, 'PROOF.bend')));
    await temporary(directory => {
        const executable = compile(directory, path.join(generated, 'runtime/focus.c'), rows, 'actual');
        run(executable, []);
    });
});

test('each missing proof and each wrong reference result is rejected', async () => {
    for (const name of proofNames) {
        await temporary(async directory => {
            fs.cpSync(packageDir, directory, { recursive: true });
            const file = path.join(directory, 'PROOF.bend');
            const proof = fs.readFileSync(file, 'utf8');
            const pattern = new RegExp(`def Laws\\.${name}\\([^)]*\\):\\n  \\{==\\}\\n`);
            assert(pattern.test(proof));
            fs.writeFileSync(file, proof.replace(pattern, ''));
            await assert.rejects(checkLaws(file), /unproved declarations/);
        });
    }
    for (const [before, after] of [
        ['      Stay{}', '      Forward{}'],
        ['          Forward{}', '          Stay{}'],
        ['          Backward{}', '          Forward{}'],
    ]) {
        await temporary(async directory => {
            fs.cpSync(packageDir, directory, { recursive: true });
            const file = path.join(directory, 'main.bend');
            const source = fs.readFileSync(file, 'utf8');
            assert(source.includes(before));
            fs.writeFileSync(file, source.replace(before, after));
            await assert.rejects(checkLaws(path.join(directory, 'PROOF.bend')));
        });
    }
});

test('the native comparison rejects inert and reversed generated implementations', async () => {
    const rows = nativeRows(await checkLaws(path.join(packageDir, 'PROOF.bend')));
    const source = fs.readFileSync(path.join(generated, 'runtime/focus.c'), 'utf8');
    // Mutation only: generated functions have a column-zero closing brace.
    // This is not a source extractor or part of the proof checker.
    const pattern = /FocusTabDirectionFor\([^)]*\)\s*\{[\s\S]*?\n\}/;
    assert.equal([...source.matchAll(new RegExp(pattern, 'g'))].length, 1);
    for (const [name, body] of [
        ['inert', '(void)tab_pressed; (void)shift_down; return 0;'],
        ['reversed', 'return !tab_pressed ? 0 : (shift_down ? 1 : -1);'],
    ]) {
        await temporary(directory => {
            const mutant = path.join(directory, `focus-${name}.c`);
            fs.writeFileSync(mutant, source.replace(pattern,
                `FocusTabDirectionFor(bool tab_pressed, bool shift_down) {\n    ${body}\n}`));
            const executable = compile(directory, mutant, rows, name);
            const result = spawnSync(executable, [], { encoding: 'utf8', timeout: 10000 });
            assert.ifError(result.error);
            assert.equal(result.status, 1);
            assert.match(result.stderr, /focus\.tab\.direction/);
        });
    }
});
