// Law inventory validator (phase 1 of the law plan).
// Validates laws/inventory.json against the canonical widget surface,
// the KIR construct enums and the runtime module set on disk.
// Usage: node tools/check-law-inventory.mjs [--report]
import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';

export const ROOT = path.resolve(import.meta.dirname, '..');

const STATUSES = ['proposed', 'specified', 'model-proved', 'implementation-connected', 'integration-verified', 'deferred'];
const DOMAINS = ['compiler', 'kir', 'widget', 'krb', 'policy'];
const TARGETS = ['c', 'cpp', 'go', 'js'];
const EVIDENCE_LEVELS = ['missing', 'planned', 'static-check', 'property-test', 'exhaustive-test', 'test-suite', 'bend-proof', 'integration-test'];
const MECHANISM_RE = /^(static-check|property-test|exhaustive-test|test-suite|bend-proof|integration-test|planned:[a-z0-9-]+)$/;
const ID_RE = /^[a-z0-9][a-z0-9_-]*(\.[a-z0-9_-]+)+$/;
const CLAIMED = ['model-proved', 'implementation-connected', 'integration-verified'];

export function parseCanonicalSurface(root = ROOT) {
    const text = fs.readFileSync(path.join(root, 'docs/CANONICAL_WIDGET_SURFACE.md'), 'utf8');
    const rows = [];
    for (const line of text.split('\n')) {
        const m = /^\| `runtime\/([a-z0-9_]+)\.kry` \| (.+?) \| `([^`]+)` \|$/.exec(line);
        if (m) rows.push({ module: m[1], role: m[2].trim(), decision: m[3].trim() });
    }
    return rows;
}

export function parseKirKinds(root = ROOT) {
    const text = fs.readFileSync(path.join(root, 'cmd/kir/kir.h'), 'utf8');
    const kinds = enumName => {
        const body = new RegExp(`typedef enum ${enumName} \\{([\\s\\S]*?)\\}`).exec(text)[1];
        return [...body.matchAll(/\bKIR_[A-Z_]+\b/g)].map(m => m[0])
            .filter(n => !n.includes('UNKNOWN'))
            .map(n => n.replace(/^KIR_(?:STMT|EXPR)_/, '').toLowerCase());
    };
    return { stmts: kinds('KirStmtKind'), exprs: kinds('KirExprKind') };
}

export function runtimeModules(root = ROOT) {
    return fs.readdirSync(path.join(root, 'runtime'))
        .filter(f => f.endsWith('.kry')).map(f => f.replace(/\.kry$/, '')).sort();
}

export function loadInventory(root = ROOT) {
    return JSON.parse(fs.readFileSync(path.join(root, 'laws/inventory.json'), 'utf8'));
}

export function validateInventory(inventory, opts = {}) {
    const root = opts.root ?? ROOT;
    const checkPaths = opts.checkPaths !== false;
    const surface = opts.surface ?? parseCanonicalSurface(root);
    const kir = opts.kir ?? parseKirKinds(root);
    const runtime = opts.runtimeModules ?? runtimeModules(root);
    const problems = [];
    const fail = msg => problems.push(msg);

    if (inventory?.schema !== 1) fail('inventory.schema must be 1');
    if (!Array.isArray(inventory?.laws) || inventory.laws.length === 0) fail('inventory.laws must be a non-empty array');
    if (problems.length) return { ok: false, problems };

    const rows = inventory.laws;
    const ids = rows.map(r => r?.id);
    if (JSON.stringify(ids) !== JSON.stringify([...ids].sort())) fail('rows must be sorted by id for deterministic diffs');
    const seen = new Set();
    for (const id of ids) {
        if (seen.has(id)) fail(`duplicate law id: ${id}`);
        seen.add(id);
    }

    const byId = new Map(rows.map(r => [r.id, r]));
    for (const row of rows) {
        const where = `law ${row.id}`;
        if (typeof row.id !== 'string' || !ID_RE.test(row.id)) fail(`${where}: id must be a dotted lowercase name`);
        if (!DOMAINS.includes(row.domain)) fail(`${where}: unknown domain ${row.domain}`);
        if (typeof row.summary !== 'string' || !row.summary.trim()) fail(`${where}: summary required`);
        if (!Array.isArray(row.sources) || row.sources.length === 0) fail(`${where}: sources required`);
        else for (const src of row.sources) {
            if (typeof src !== 'string') fail(`${where}: source ${src} must be a path string`);
            else if (checkPaths && !fs.existsSync(path.join(root, src))) fail(`${where}: stale source path ${src}`);
        }
        if (!Number.isInteger(row.phase) || row.phase < 1 || row.phase > 10) fail(`${where}: phase must be an integer 1..10`);
        if (!Array.isArray(row.targets) || row.targets.length === 0) fail(`${where}: targets required`);
        else {
            for (const t of row.targets) if (!TARGETS.includes(t)) fail(`${where}: unknown target ${t}`);
            if (row.targets.includes('js') && row.status !== 'deferred') fail(`${where}: js target is paused and only allowed on deferred rows`);
        }
        if (row.scope !== undefined) {
            if (!Array.isArray(row.scope) || row.scope.length === 0) fail(`${where}: scope must be a non-empty target list`);
            else {
                for (const t of row.scope) {
                    if (!TARGETS.includes(t)) fail(`${where}: unknown scope target ${t}`);
                    if (t === 'js' && row.status !== 'deferred') fail(`${where}: js can only appear in the scope of deferred rows`);
                }
                for (const t of row.targets) if (!row.scope.includes(t)) fail(`${where}: scope must be a superset of targets`);
            }
        }
        if (!STATUSES.includes(row.status)) fail(`${where}: unknown status ${row.status}`);
        if (typeof row.mechanism !== 'string' || !MECHANISM_RE.test(row.mechanism)) fail(`${where}: invalid mechanism ${row.mechanism}`);
        if (row.status === 'deferred') {
            if (typeof row.deferred_reason !== 'string' || !row.deferred_reason.trim()) fail(`${where}: deferred rows need deferred_reason`);
            if (row.evidence) fail(`${where}: deferred rows must not claim evidence`);
        }
        if (row.status === 'model-proved') {
            for (const key of ['contract', 'proof']) {
                if (typeof row[key] !== 'string') fail(`${where}: model-proved rows need a ${key} path`);
                else if (checkPaths && !fs.existsSync(path.join(root, row[key]))) fail(`${where}: missing ${key} path ${row[key]}`);
            }
        }
        if (CLAIMED.includes(row.status)) {
            if (typeof row.evidence !== 'object' || row.evidence === null) fail(`${where}: ${row.status} rows need evidence per target`);
            else {
                for (const [target, level] of Object.entries(row.evidence)) {
                    if (!TARGETS.includes(target)) fail(`${where}: evidence for unknown target ${target}`);
                    if (!EVIDENCE_LEVELS.includes(level)) fail(`${where}: unknown evidence level ${level}`);
                    if (level === 'missing' || level === 'planned') fail(`${where}: ${row.status} claimed for target ${target} but evidence is ${level}`);
                }
                for (const t of row.targets) if (!(t in row.evidence)) fail(`${where}: no evidence recorded for declared target ${t}`);
            }
            if (typeof row.evidence_revision !== 'string' || !row.evidence_revision.trim()) fail(`${where}: ${row.status} rows need evidence_revision`);
        }
        if (row.evidence && !CLAIMED.includes(row.status) && row.status !== 'deferred') {
            for (const level of Object.values(row.evidence)) if (!EVIDENCE_LEVELS.includes(level)) fail(`${where}: unknown evidence level ${level}`);
        }
        if (Array.isArray(row.laws)) for (const link of row.laws) if (!byId.has(link)) fail(`${where}: links unknown law ${link}`);
    }
    return { ok: problems.length === 0, problems, byId, surface, kir, runtime };
}

export function checkDenominators(result) {
    const { problems, byId, surface, kir, runtime } = result;
    const fail = msg => problems.push(msg);
    const live = surface.filter(s => s.decision !== 'Removed');
    const removed = new Set(surface.filter(s => s.decision === 'Removed').map(s => s.module));
    const moduleToRow = new Map();
    for (const row of byId.values()) {
        if (row.domain !== 'widget' || !Array.isArray(row.sources)) continue;
        for (const src of row.sources) {
            const m = /^runtime\/([a-z0-9_]+)\.kry$/.exec(src);
            if (!m) continue;
            if (removed.has(m[1])) fail(`${row.id}: references removed module ${m[1]}`);
            if (moduleToRow.has(m[1])) fail(`module ${m[1]} is claimed by both ${moduleToRow.get(m[1])} and ${row.id}`);
            moduleToRow.set(m[1], row.id);
        }
    }
    for (const s of live) if (!moduleToRow.has(s.module)) fail(`missing inventory row: canonical module runtime/${s.module}.kry has no owning law row`);
    for (const [module, rowId] of moduleToRow) {
        if (!live.some(s => s.module === module)) fail(`${rowId}: claims runtime/${module}.kry which is not in the canonical surface`);
    }
    const surfaceSet = new Set(surface.map(s => s.module));
    for (const m of runtime) if (!surfaceSet.has(m)) fail(`runtime/${m}.kry exists on disk but is missing from the canonical surface`);
    for (const m of surfaceSet) if (!runtime.includes(m) && !removed.has(m)) fail(`canonical surface lists runtime/${m}.kry but no such file exists`);
    for (const kind of kir.stmts) if (!byId.has(`kir.stmt.${kind}`)) fail(`missing inventory row: kir.stmt.${kind}`);
    for (const kind of kir.exprs) if (!byId.has(`kir.expr.${kind}`)) fail(`missing inventory row: kir.expr.${kind}`);
    const kirIds = new Set([...kir.stmts.map(k => `kir.stmt.${k}`), ...kir.exprs.map(k => `kir.expr.${k}`)]);
    for (const row of byId.values()) {
        if (/^kir\.(stmt|expr)\./.test(row.id) && !kirIds.has(row.id)) fail(`${row.id}: stale KIR construct row (not present in cmd/kir/kir.h)`);
    }
    result.ok = problems.length === 0;
    return result;
}

export function renderReport(inventory) {
    const rows = inventory.laws;
    const count = (key) => {
        const out = {};
        for (const row of rows) out[row[key]] = (out[row[key]] || 0) + 1;
        return Object.fromEntries(Object.entries(out).sort(([a], [b]) => a < b ? -1 : 1));
    };
    const lines = [
        `# Law inventory report`,
        ``,
        `- Rows: ${rows.length}`,
        `- By domain: ${JSON.stringify(count('domain'))}`,
        `- By status: ${JSON.stringify(count('status'))}`,
        `- By phase: ${JSON.stringify(count('phase'))}`,
        ``,
        `| Law | Domain | Phase | Status | Mechanism | Targets |`,
        `|---|---|---|---|---|---|`,
    ];
    for (const row of rows) {
        lines.push(`| ${row.id} | ${row.domain} | ${row.phase} | ${row.status} | ${row.mechanism} | ${row.targets.join(', ')} |`);
    }
    return lines.join('\n');
}

function main() {
    const report = process.argv.includes('--report');
    const inventory = loadInventory();
    const result = checkDenominators(validateInventory(inventory));
    if (report) console.log(renderReport(inventory));
    if (!result.ok) {
        for (const problem of result.problems) console.error(`law-inventory: ${problem}`);
        console.error(`law-inventory: ${result.problems.length} problem(s)`);
        process.exit(1);
    }
    const byStatus = {};
    for (const row of inventory.laws) byStatus[row.status] = (byStatus[row.status] || 0) + 1;
    console.log(`law-inventory: ${inventory.laws.length} rows valid ${JSON.stringify(byStatus)}`);
}

if (process.argv[1] && import.meta.url === new URL(`file://${process.argv[1]}`).href) main();
