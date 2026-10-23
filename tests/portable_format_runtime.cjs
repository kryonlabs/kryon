const assert = require('node:assert/strict');
const createRuntime = require('../docs/site/assets/playground-runtime.js');

async function main() {
  const errors = [];
  const runtime = await createRuntime({printErr: text => errors.push(text)});
  runtime.FS.writeFile('/format.zi', 'Answer :: () -> s32 { return 42; }');
  const status = runtime.ccall('BuildBundle', 'number', Array(6).fill('string'),
    ['/', '/std', '/format.zi', 'format', 'Answer', '/format.zx']);
  assert.equal(status, 0, errors.join('\n'));
  assert.deepEqual([...runtime.FS.readFile('/format.zx').slice(0, 4)], [90, 88, 0, 0]);
  console.log('Website compiler produces the ZX portable format');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
