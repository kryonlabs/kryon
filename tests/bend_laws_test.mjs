import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'node:test';
import { checkLaws } from '../tools/bend-laws.mjs';

const implementation = `import Base
type State is Data:
  Ready{}
  Waiting{}
def delay(state: State) -> U32:
  match state:
    case Ready{}:
      0
    case Waiting{}:
      5
`;
const laws = `import Base
import ./main.bend as Policy
law ready:
  {Policy.delay(Policy.Ready{}) == 0 : U32}
law waiting:
  {Policy.delay(Policy.Waiting{}) == 5 : U32}
`;
const proof = `import Base
import ./LAWS.bend as Laws
def Laws.ready():
  {==}
def Laws.waiting():
  {==}
`;

async function fixture(run, changes = {}) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), 'bend-laws-'));
    try {
        for (const [name, text] of Object.entries({
            'main.bend': implementation, 'LAWS.bend': laws, 'PROOF.bend': proof, ...changes,
        })) {
            fs.writeFileSync(path.join(root, name), text);
        }
        await run(path.join(root, 'PROOF.bend'));
    } finally {
        fs.rmSync(root, { recursive: true, force: true });
    }
}

test('proves laws and evaluates the checked implementation for every state', async () => {
    await fixture(async file => {
        const checked = await checkLaws(file);
        assert.deepEqual(checked.laws, ['ready', 'waiting']);
        assert.deepEqual(checked.table('main.delay'), {
            domains: [['main.Ready', 'main.Waiting']],
            rows: [
                { arguments: ['main.Ready'], value: 0 },
                { arguments: ['main.Waiting'], value: 5 },
            ],
        });
    });
});

test('rejects an implementation that breaks a proved contract', async () => {
    await fixture(file => assert.rejects(checkLaws(file)), {
        'main.bend': implementation.replace('      5', '      6'),
    });
});

test('rejects unproved laws', async () => {
    await fixture(file => assert.rejects(checkLaws(file), /unproved declarations/), {
        'PROOF.bend': proof.replace('def Laws.waiting():\n  {==}\n', ''),
    });
});

test('rejects proof holes', async () => {
    await fixture(file => assert.rejects(checkLaws(file), /\?missing|proof holes/), {
        'PROOF.bend': proof.replace('{==}', '?missing'),
    });
});

test('rejects proofs that omit the law package', async () => {
    await fixture(file => assert.rejects(checkLaws(file), /must import/), {
        'PROOF.bend': 'import Base\ndef main() -> U32:\n  0\n',
    });
});

test('rejects disabled termination checking', async () => {
    await fixture(file => assert.rejects(checkLaws(file), /@unsafe/), {
        'main.bend': implementation.replace('def delay', '@unsafe def delay'),
    });
});

test('rejects remote and foreign imports before loading', async () => {
    for (const target of ['0xabcdef/main.bend as Remote', '"./effect.c"']) {
        await fixture(file => assert.rejects(checkLaws(file), /only Base and/), {
            'main.bend': `import ${target}\n${implementation}`,
        });
    }
});

test('rejects imports outside the proof package', async () => {
    await fixture(async file => {
        fs.symlinkSync(import.meta.filename, path.join(path.dirname(file), 'escape.bend'));
        await assert.rejects(checkLaws(file), /import leaves proof package/);
    }, {
        'main.bend': 'import ./escape.bend as Escape\n',
    });
});
