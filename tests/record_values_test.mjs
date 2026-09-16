import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";

// This pure fixture does not use host APIs. Remove the runtime import so the
// same generated policy can be tested independently of browser packaging.
const source = (await readFile(process.argv[2], "utf8"))
  .replace(/^import \* as kryon from .*;$/m, `const deepCopy = (v) => {
    if (v === null || typeof v !== "object") return v;
    if (Array.isArray(v)) return v.map(deepCopy);
    const out = {};
    for (const key of Object.keys(v)) out[key] = deepCopy(v[key]);
    return out;
  };
  const kryon = {
    createRuntime: () => ({}),
    copyValue: deepCopy,
    index: (base, i) => base[i]
  };`);
const generated = await import(`data:text/javascript;base64,${Buffer.from(source).toString("base64")}`);
assert.equal(generated.RecordValues_Check(null), 0);
