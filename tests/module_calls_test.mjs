import assert from "node:assert/strict";
import { pathToFileURL } from "node:url";

const caller = await import(pathToFileURL(process.argv[2]).href);
const counter = await import(pathToFileURL(process.argv[3]).href);
assert.equal(caller.ModuleCalls_EnumCheck(null), 15);
assert.equal(caller.ModuleCalls_Check(null, undefined, undefined, 1, 2, 3), 29);
assert.equal(caller.ModuleCalls_Check(null, undefined, undefined, 1, 2, 3), 33);
assert.equal(caller.moduleState.count, 100);
assert.equal(counter.moduleState.count, 14);
console.log("JavaScript module paths, context names, and provider state passed");
