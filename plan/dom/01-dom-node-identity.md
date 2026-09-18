# DOM Node Identity

Goal: every `.kry` node that can become browser DOM must have stable identity
before and after mount.

## Required Facts

- `kind`: the Kry widget or DOM alias name.
- `tag`: the native browser element tag selected for that node.
- `path`: stable tree path, including parent nesting.
- `parentPath`: stable parent tree path.
- `name`: authored block name when present.
- `key`: authored key or compiler-derived deterministic key.
- `ref`: canonical lookup ref used by DOM object APIs.
- `webRef`: optional authored web ref from `dom_ref`, `web_ref`, or `kry_ref`.
- `domId` and `domName`: browser `id` and `name` fields when authored.
- source location and range facts.
- classes, state, data attrs, ARIA attrs, native attrs, relationships, events,
  accessibility facts, and style facts.

## Steps

1. Audit every runtime `widget(...)` call site and every compiler-emitted widget
   expression path.
2. Mark which nodes are compiler-owned and which still rely on runtime fallback.
3. For compiler-owned nodes, require metadata with `path`, `key`, `sourcePath`,
   `sourceLine`, and `sourceColumn`.
4. For fallback nodes, keep deterministic runtime identity but document why the
   compiler cannot own it yet.
5. Ensure mounted elements expose `data-kry-*` annotations matching
   `webNodeIdentity(node)`.
6. Ensure `webDOMObjectMap(...)` indexes every alias: `ref`, `path`, `name`,
   `key`, DOM id, DOM name, source ref, and source range ref.

## Evidence

- `tests/k2js_syntax_test_runner.mjs` asserts pre-mount identity.
- Browser DOM tests assert `data-kry-*` annotations and mounted object lookup.
- `webDOMObject(...)`, `webDOMObjects(...)`, and `root.kryObject(...)` resolve
  the same node through all supported aliases.
