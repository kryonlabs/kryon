# DOM Plan Index

Status: future planning contract for the Kry web-native DOM path. The old
JavaScript/web target is paused; this plan must not be read as a current release
gate until web work resumes under the roadmap.

This folder breaks the remaining DOM work into ten implementation documents.
The target is a web authoring surface where `.kry` owns document structure,
KSS owns styling, and generated JavaScript owns logic and runtime glue.

## Documents

1. `00-index.md` defines the plan map and completion rules.
2. `01-dom-node-identity.md` defines the stable identity every DOM-producing
   Kry node must expose.
3. `02-compiler-metadata.md` defines how a future web-native compiler should
   emit identity metadata before runtime fallback is needed.
4. `03-source-ranges.md` defines full source-span coverage for editor and
   devtools lookup.
5. `04-native-element-surface.md` defines how Kry widget names map to browser
   elements.
6. `05-attributes-state-and-data.md` defines native attributes, data, ARIA, and
   state synchronization.
7. `06-kss-selector-and-style-contract.md` defines the contract KSS resolves
   against.
8. `07-relationships-and-accessibility.md` defines semantic DOM relationships
   and accessibility snapshots.
9. `08-events-and-dom-commands.md` defines event glue, native commands, and
   mutation APIs.
10. `09-tests-rollout-and-coordination.md` defines verification, parallel-agent
    coordination, and rollout gates.

## Completion Rules

- Every DOM-producing `.kry` node has stable `ref`, `path`, `key`, source, tag,
  state, class, data, ARIA, relationship, and style facts.
- The browser receives native elements and attributes; generated JavaScript
  does not hand-author app structure.
- KSS selectors and style resolution use Kry DOM facts and exported DOM
  annotations.
- Mounted DOM objects expose the same identity and facts as pre-mount Web
  Document nodes.
- Tests prove behavior at compiler, runtime, fake DOM, and browser levels.
