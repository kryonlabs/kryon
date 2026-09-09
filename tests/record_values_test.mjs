import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";

// This pure fixture does not use host APIs. Remove the runtime import so the
// same generated policy can be tested independently of browser packaging.
const source = (await readFile(process.argv[2], "utf8"))
  .replace(/^import \* as kryon from .*;$/m, "const kryon = {};");
const generated = await import(`data:text/javascript;base64,${Buffer.from(source).toString("base64")}`);
assert.equal(generated.RecordValues_Check(null), 0);
