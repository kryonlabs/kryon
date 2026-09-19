import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import {
    loadInventory, validateInventory, checkDenominators, renderReport, ROOT,
} from '../tools/check-law-inventory.mjs';

const surface = [
    { module: 'widget_a', role: 'A widget', decision: '.kry canonical' },
    { module: 'widget_a_props', role: 'Props', decision: '.kry canonical' },
    { module: 'gone', role: 'Retired', decision: 'Removed' },
];
const kir = { stmts: ['if', 'return'], exprs: ['int', 'call'] };
const runtime = ['gone', 'widget_a', 'widget_a_props'];
const opts = { surface, kir, runtimeModules: runtime, checkPaths: false };

function baseInventory() {
    return {
        schema: 1,
        laws: [
            { id: 'kir.expr.call', domain: 'kir', summary: 'call', sources: ['cmd/kir/kir.h'], phase: 3, targets: ['c', 'cpp', 'go'], status: 'proposed', mechanism: 'planned:bend-semantics' },
            { id: 'kir.expr.int', domain: 'kir', summary: 'int', sources: ['cmd/kir/kir.h'], phase: 3, targets: ['c', 'cpp', 'go'], status: 'proposed', mechanism: 'planned:bend-semantics' },
            { id: 'kir.stmt.if', domain: 'kir', summary: 'if', sources: ['cmd/kir/kir.h'], phase: 3, targets: ['c', 'cpp', 'go'], status: 'proposed', mechanism: 'planned:bend-semantics' },
            { id: 'kir.stmt.return', domain: 'kir', summary: 'return', sources: ['cmd/kir/kir.h'], phase: 3, targets: ['c', 'cpp', 'go'], status: 'proposed', mechanism: 'planned:bend-semantics' },
            { id: 'widget.widget_a', domain: 'widget', summary: 'A widget family', sources: ['runtime/widget_a.kry', 'runtime/widget_a_props.kry'], phase: 6, targets: ['c', 'cpp', 'go'], status: 'proposed', mechanism: 'planned:policy-proof' },
        ],
    };
}

function problemsOf(inventory, extra = {}) {
    return checkDenominators(validateInventory(inventory, { ...opts, ...extra })).problems;
}

test('the committed inventory validates against the real denominators', () => {
    const inventory = loadInventory();
    const result = checkDenominators(validateInventory(inventory));
    assert.deepEqual(result.problems, []);
    assert.ok(result.ok);
});

test('the report renders deterministically with the row count', () => {
    const first = renderReport(loadInventory());
    const second = renderReport(loadInventory());
    assert.equal(first, second);
    assert.match(first, /- Rows: \d+/);
});

test('duplicate and unsorted law ids are rejected', () => {
    const inventory = baseInventory();
    inventory.laws.push(structuredClone(inventory.laws[4]));
    assert.ok(problemsOf(inventory).some(p => p.includes('duplicate law id: widget.widget_a')));
    const shuffled = baseInventory();
    shuffled.laws.reverse();
    assert.ok(problemsOf(shuffled).some(p => p.includes('sorted by id')));
});

test('a canonical module without an owning row is rejected by module name', () => {
    const inventory = baseInventory();
    inventory.laws = inventory.laws.filter(r => r.id !== 'widget.widget_a');
    assert.ok(problemsOf(inventory).some(p => p.includes('runtime/widget_a.kry has no owning law row')));
});

test('widget rows claiming modules outside the surface are stale', () => {
    const inventory = baseInventory();
    inventory.laws[4].sources = ['runtime/other.kry'];
    assert.ok(problemsOf(inventory).some(p => p.includes('claims runtime/other.kry which is not in the canonical surface')));
    assert.ok(problemsOf(inventory).some(p => p.includes('runtime/widget_a.kry has no owning law row')));
});

test('removed modules are neither required nor referenceable', () => {
    const inventory = baseInventory();
    assert.deepEqual(problemsOf(inventory), []);
    inventory.laws[4].sources.push('runtime/gone.kry');
    assert.ok(problemsOf(inventory).some(p => p.includes('references removed module gone')));
});

test('surface and runtime drift is rejected in both directions', () => {
    assert.ok(problemsOf(baseInventory(), { runtimeModules: [...runtime, 'extra'] })
        .some(p => p.includes('runtime/extra.kry exists on disk but is missing from the canonical surface')));
    assert.ok(problemsOf(baseInventory(), { surface: surface.filter(s => s.module !== 'gone') })
        .some(p => p.includes('runtime/gone.kry exists on disk but is missing from the canonical surface')));
    assert.ok(problemsOf(baseInventory(), { runtimeModules: runtime.filter(m => m !== 'widget_a_props') })
        .some(p => p.includes('canonical surface lists runtime/widget_a_props.kry but no such file exists')));
});

test('missing and stale KIR construct rows are rejected', () => {
    const inventory = baseInventory();
    inventory.laws = inventory.laws.filter(r => r.id !== 'kir.stmt.if');
    assert.ok(problemsOf(inventory).some(p => p.includes('missing inventory row: kir.stmt.if')));
    const stale = baseInventory();
    stale.laws.push({ id: 'kir.stmt.gosub', domain: 'kir', summary: 'stale', sources: ['cmd/kir/kir.h'], phase: 3, targets: ['c'], status: 'proposed', mechanism: 'planned:bend-semantics' });
    const problems = problemsOf(stale);
    assert.ok(problems.some(p => p.includes('stale KIR construct row')));
    assert.ok(problems.some(p => p.includes('sorted by id')));
});

test('field-level violations are rejected', () => {
    const mutate = fn => { const inv = baseInventory(); fn(inv.laws[4], inv); return problemsOf(inv); };
    assert.ok(mutate(r => { r.status = 'maybe'; }).some(p => p.includes('unknown status maybe')));
    assert.ok(mutate(r => { r.phase = 11; }).some(p => p.includes('phase must be an integer 1..10')));
    assert.ok(mutate(r => { r.targets = ['rust']; }).some(p => p.includes('unknown target rust')));
    assert.ok(mutate(r => { r.mechanism = 'vibes'; }).some(p => p.includes('invalid mechanism vibes')));
    assert.ok(mutate(r => { r.sources = []; }).some(p => p.includes('sources required')));
    assert.ok(mutate(r => { delete r.summary; }).some(p => p.includes('summary required')));
});

test('the paused js target is only allowed on deferred rows', () => {
    const mutate = fn => { const inv = baseInventory(); fn(inv.laws[4], inv); return problemsOf(inv); };
    assert.ok(mutate(r => { r.targets = ['js']; }).some(p => p.includes('js target is paused')));
    assert.ok(mutate(r => { r.scope = ['js']; }).some(p => p.includes('js can only appear in the scope of deferred rows')));
    assert.ok(mutate(r => { r.scope = ['c']; }).some(p => p.includes('scope must be a superset of targets')));
});

test('deferred rows need a reason and must not claim evidence', () => {
    const mutate = fn => { const inv = baseInventory(); fn(inv.laws[4], inv); return problemsOf(inv); };
    assert.ok(mutate(r => { r.status = 'deferred'; }).some(p => p.includes('deferred rows need deferred_reason')));
    assert.ok(mutate(r => {
        r.status = 'deferred'; r.deferred_reason = 'paused'; r.targets = ['js'];
        r.evidence = { js: 'test-suite' };
    }).some(p => p.includes('deferred rows must not claim evidence')));
    assert.deepEqual(mutate(r => {
        r.status = 'deferred'; r.deferred_reason = 'paused'; r.targets = ['js'];
        r.mechanism = 'planned:test-suite'; delete r.laws;
    }), []);
});

test('claimed statuses need solid per-target evidence and a revision', () => {
    const mutate = fn => { const inv = baseInventory(); fn(inv.laws[4], inv); return problemsOf(inv); };
    const claim = r => {
        r.status = 'implementation-connected'; r.mechanism = 'property-test';
        r.evidence = { c: 'property-test' }; r.evidence_revision = 'testrev';
    };
    assert.deepEqual(mutate(r => { claim(r); r.targets = ['c']; }), []);
    assert.ok(mutate(r => { claim(r); }).some(p => p.includes('no evidence recorded for declared target cpp')));
    assert.ok(mutate(r => { claim(r); r.targets = ['c']; delete r.evidence_revision; })
        .some(p => p.includes('need evidence_revision')));
    assert.ok(mutate(r => { claim(r); r.targets = ['c']; r.evidence.c = 'missing'; })
        .some(p => p.includes('evidence is missing')));
    assert.ok(mutate(r => { claim(r); r.targets = ['c']; r.evidence.c = 'wishful'; })
        .some(p => p.includes('unknown evidence level wishful')));
});

test('model-proved rows need existing contract and proof paths', () => {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'law-inventory-'));
    try {
        const contract = path.join(directory, 'LAWS.bend');
        fs.writeFileSync(contract, 'laws');
        const inv = baseInventory();
        inv.laws.push({
            id: 'widget.widget_a.proof', domain: 'policy', summary: 'proof',
            sources: ['runtime/widget_a.kry'], phase: 2, targets: ['c'],
            status: 'model-proved', mechanism: 'bend-proof',
            contract: 'laws/x/LAWS.bend', proof: 'laws/x/PROOF.bend',
            evidence: { c: 'exhaustive-test' }, evidence_revision: 'rev',
        });
        inv.laws.sort((a, b) => a.id < b.id ? -1 : 1);
        const problems = problemsOf(inv, { root: directory, checkPaths: true });
        assert.ok(problems.some(p => p.includes('missing contract path laws/x/LAWS.bend')));
        assert.ok(problems.some(p => p.includes('missing proof path laws/x/PROOF.bend')));
    } finally {
        fs.rmSync(directory, { recursive: true, force: true });
    }
});

test('non-widget rows may reference runtime sources without claiming module ownership', () => {
    const inv = baseInventory();
    inv.laws.push({
        id: 'widget_a.policy.something', domain: 'policy', summary: 'policy connection',
        sources: ['runtime/widget_a.kry'], phase: 6, targets: ['c'],
        status: 'proposed', mechanism: 'planned:policy-proof',
    });
    inv.laws.sort((a, b) => a.id < b.id ? -1 : 1);
    assert.deepEqual(problemsOf(inv), []);
});

test('law links must reference existing rows', () => {
    const inv = baseInventory();
    inv.laws[4].laws = ['widget_a.policy.missing'];
    assert.ok(problemsOf(inv).some(p => p.includes('links unknown law widget_a.policy.missing')));
});

test('ROOT points at the repository that owns this test', () => {
    assert.ok(fs.existsSync(path.join(ROOT, 'laws/inventory.json')));
    assert.ok(fs.existsSync(path.join(ROOT, 'docs/CANONICAL_WIDGET_SURFACE.md')));
});
