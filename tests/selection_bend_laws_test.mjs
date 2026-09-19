import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { test } from 'node:test';
import { checkLaws } from '../tools/bend-laws.mjs';

const root = path.resolve(import.meta.dirname, '..');
const packageDir = path.join(root, 'laws/selection');
const generated = path.resolve(root,
    process.env.KRYON_LAW_GENERATED_DIR || 'build/linux-x86_64/generated/src');
const proofNames = ['select_item_returns_item', 'clear_selection_returns_none',
    'deselect_matching_minus', 'deselect_matching_zero', 'deselect_matching_one',
    'deselect_distinct_preserves_minus_zero', 'deselect_distinct_preserves_zero_one',
    'deselect_distinct_preserves_one_minus', 'other_actions_preserve'];

const values = new Map([['MinusOne', -1], ['Zero', 0], ['One', 1]]);
const actions = new Map([['Other', 1], ['SelectItem', 16], ['DeselectItem', 32], ['ClearSelection', 128]]);

function temporary(run) {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'kryon-selection-laws-'));
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

function nativeRows(checked) {
    assert.deepEqual(checked.laws, proofNames);
    const table = checked.table('main.single_selection');
    const bare = name => name.replace(/^.*\./, '');
    assert.deepEqual(table.domains[0].map(bare), ['MinusOne', 'Zero', 'One']);
    assert.deepEqual(table.domains[1].map(bare), ['MinusOne', 'Zero', 'One']);
    assert.equal(table.domains[2].length, 4);
    assert.equal(table.rows.length, 36);
    const seen = new Set();
    return table.rows.map(row => {
        assert.equal(row.arguments.length, 3);
        const args = row.arguments.map((name, i) => {
            const map = i < 2 ? values : actions;
            const key = bare(name);
            assert(map.has(key), `unmapped input: ${name}`);
            return map.get(key);
        });
        const result = bare(row.value.constructor);
        assert(values.has(result), 'result must be a closed Value');
        const key = args.join(',');
        assert(!seen.has(key), 'duplicate input row');
        seen.add(key);
        return [...args, values.get(result)];
    });
}

function compileAndCheck(directory, rows, suffix) {
    const driver = path.join(directory, `driver-${suffix}.c`);
    fs.writeFileSync(driver, `#include "runtime/accessibility_policy.h"
#include <stdio.h>
int main(void) {
    const int cases[][4] = {
${rows.map(row => `        {${row.join(', ')}},`).join('\n')}
    };
    for(unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const int *c = cases[i];
        int actual = AccessibilitySingleSelectionFor(c[0], c[1], (AccessibilityAction)c[2]);
        if(actual != c[3]) {
            fprintf(stderr, "accessibility.selection.single: case %u: got %d, expected %d\\n",
                    i, actual, c[3]);
            return 1;
        }
    }
    return 0;
}
`);
    const executable = path.join(directory, `check-${suffix}`);
    run(process.env.CC || 'cc', ['-std=c99', '-O2', '-DNDEBUG', '-Wall', '-Werror',
        '-I' + path.join(root, 'include'), '-I' + generated, driver,
        path.join(generated, 'runtime/accessibility_policy.c'), '-lm', '-o', executable]);
    run(executable, []);
}

test('nine checked selection laws agree with generated C over the bounded subdomain', async () => {
    const rows = nativeRows(await checkLaws(path.join(packageDir, 'PROOF.bend')));
    await temporary(directory => compileAndCheck(directory, rows, 'actual'));
});

test('a wrong reference branch is rejected by the checker', async () => {
    await temporary(async directory => {
        fs.cpSync(packageDir, directory, { recursive: true });
        const file = path.join(directory, 'main.bend');
        const source = fs.readFileSync(file, 'utf8');
        const mutated = source.replace('    case ClearSelection{}:\n      MinusOne{}',
            '    case ClearSelection{}:\n      Zero{}');
        assert.notEqual(mutated, source, 'mutation must apply');
        fs.writeFileSync(file, mutated);
        await assert.rejects(checkLaws(path.join(directory, 'PROOF.bend')));
    });
});
