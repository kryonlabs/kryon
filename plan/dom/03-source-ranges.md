# Source Ranges

Goal: editors and devtools can map a `.kry` cursor or selection to the correct
Kry DOM object.

## Required Behavior

- Every DOM-producing node exposes `sourcePath`, `sourceLine`, `sourceColumn`,
  `sourceEndLine`, and `sourceEndColumn` when the parser can know them.
- Single-point lookup finds the deepest node that contains a cursor.
- Range lookup finds nodes that contain or overlap a selection.
- Mounted DOM elements expose equivalent `data-kry-source-*` annotations.

## Steps

1. Audit parser span coverage for statements, expressions, named blocks, and
   lexical UI scopes.
2. Extend span capture beyond UI blocks to full multiline AST spans for every
   expression-backed DOM node.
3. Normalize spans so missing end positions fall back predictably to start
   positions.
4. Preserve source ranges through component remapping.
5. Expose source refs as `path:line`, `path:line:column`, and
   `path:startLine:startColumn-endLine:endColumn`.
6. Verify mounted DOM lookup mirrors pre-mount Web Document lookup.

## Evidence

- `webNodeAtSource(...)`, `webNodesAtSourceRange(...)`, and
  `webNodeOverlappingSourceRange(...)` return expected pre-mount nodes.
- `webDOMObjectAtSource(...)`, `webDOMObjectsAtSourceRange(...)`, and
  `webDOMObjectsOverlappingSourceRange(...)` return expected mounted objects.
- Browser tests prove the `data-kry-source-range-ref` attributes exist.
