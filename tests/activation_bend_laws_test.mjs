import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';
import { checkLaws, checkLawsProcess } from '../tools/bend-laws.mjs';

const root = path.resolve(import.meta.dirname, '..');
const packageDir = path.join(root, 'laws/activation');
const generated = path.resolve(root,
    process.env.KRYON_LAW_GENERATED_DIR || 'build/linux-x86_64/generated/src');
const proofNames = ['no_activation_when_inactive', 'no_activation_when_keyboard_disabled',
    'no_activation_when_disabled', 'no_activation_when_captured', 'enter_activates',
    'space_activates_without_text_input', 'text_input_blocks_space'];

function temporary(run) {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'kryon-activation-laws-'));
    return Promise.resolve().then(() => run(directory)).finally(() => {
        fs.rmSync(directory, { recursive: true, force: true });
    });
}

function run(command, args, options = {}) {
    const result = spawnSync(command, args, { encoding: 'utf8', timeout: 30000, ...options });
    assert.ifError(result.error);
    assert.equal(result.status, 0, `${command}: ${result.stdout}\n${result.stderr}`);
    return result;
}

const booleans = new Map([['False', 0], ['True', 1]]);

function nativeRows(checked) {
    assert.deepEqual(checked.laws, proofNames);
    const table = checked.table('main.activation');
    assert.equal(table.domains.length, 7);
    for (const domain of table.domains) assert.deepEqual(domain, ['False', 'True']);
    assert.equal(table.rows.length, 128);
    const seen = new Set();
    return table.rows.map(row => {
        assert.equal(row.arguments.length, 7);
        const args = row.arguments.map(name => {
            assert(booleans.has(name), `unmapped input: ${name}`);
            return booleans.get(name);
        });
        assert(booleans.has(row.value.constructor), 'result must be a closed Bool');
        const key = args.join(',');
        assert(!seen.has(key), 'duplicate input row');
        seen.add(key);
        return [...args, booleans.get(row.value.constructor)];
    });
}

function compileAndCheck(directory, rows, suffix) {
    const driver = path.join(directory, `driver-${suffix}.c`);
    fs.writeFileSync(driver, `#include "runtime/focus.h"
#include <stdio.h>
int main(void) {
    const int cases[][8] = {
${rows.map(row => `        {${row.join(', ')}},`).join('\n')}
    };
    for(unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const int *c = cases[i];
        int actual = FocusActivationFor(c[0] != 0, c[1] != 0, c[2] != 0,
            c[3] != 0, c[4] != 0, c[5] != 0, c[6] != 0);
        if(actual != c[7]) {
            fprintf(stderr, "focus.activation: case %u: got %d, expected %d\\n", i, actual, c[7]);
            return 1;
        }
    }
    return 0;
}
`);
    const executable = path.join(directory, `check-${suffix}`);
    run(process.env.CC || 'cc', ['-std=c99', '-O2', '-DNDEBUG', '-Wall', '-Werror',
        '-I' + path.join(root, 'include'), '-I' + generated, driver,
        path.join(generated, 'runtime/focus.c'), '-lm', '-o', executable]);
    run(executable, []);
}

test('six checked activation laws agree with generated C over the full Boolean domain', async () => {
    const rows = nativeRows(await checkLaws(path.join(packageDir, 'PROOF.bend')));
    await temporary(directory => compileAndCheck(directory, rows, 'actual'));
});

test('each missing proof and a wrong reference branch are rejected', async () => {
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
    await temporary(async directory => {
        fs.cpSync(packageDir, directory, { recursive: true });
        const file = path.join(directory, 'main.bend');
        const source = fs.readFileSync(file, 'utf8');
        // Flip the eligible-Enter result: True{} under enter_pressed.
        const mutated = source.replace('                    case True{}:\n                      True{}',
            '                    case True{}:\n                      False{}');
        assert.notEqual(mutated, source, 'mutation must apply');
        fs.writeFileSync(file, mutated);
        await assert.rejects(checkLaws(path.join(directory, 'PROOF.bend')));
    });
});

test('the bounded process runner checks, rejects and times out', async () => {
    const bounded = await checkLawsProcess(path.join(packageDir, 'PROOF.bend'));
    assert.deepEqual(bounded.laws, proofNames);
    assert.equal(bounded.version, '2.0.16');
    await temporary(async directory => {
        fs.cpSync(packageDir, directory, { recursive: true });
        const file = path.join(directory, 'PROOF.bend');
        fs.writeFileSync(file, fs.readFileSync(file, 'utf8')
            .replace(/def Laws\.enter_activates\(\):\n  \{==\}\n/, ''));
        await assert.rejects(checkLawsProcess(file), /unproved declarations/);
    });
    await assert.rejects(checkLawsProcess(path.join(packageDir, 'PROOF.bend'), { timeoutMs: 1 }),
        /wall-clock limit/);
});
