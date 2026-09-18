# Relationships And Accessibility

Goal: Kry DOM exposes semantic relationships and accessibility facts without
requiring inspectors to scrape raw browser markup.

## Relationship Families

- labels: explicit `for`, implicit wrapped labels, labelled-by relations;
- forms: control ownership, output `for`, datalist `list`;
- tables: headers, header reverse links, row and column groups;
- landmarks: owner/member relationships for page regions;
- collections: menus, tabs, trees, listboxes, lists, selected and active items;
- disclosure: details and summary;
- fieldsets: legends and disabled scopes;
- media/maps: image map ownership and mapped images;
- ARIA references: controls, owns, described-by, details, error message,
  flow-to, active descendant;
- structure: parent, ancestors, siblings, children, descendants.

## Steps

1. For each native element family, define forward and reverse relation names.
2. Resolve relation targets by Kry ref, path, name, key, DOM id, and DOM name.
3. Expose relation objects and serializable refs before mount.
4. Expose the same relation objects and refs after mount.
5. Include relationship refs inside DOM and accessibility snapshots.
6. Add browser tests for relations that depend on native containment or
   rendered attributes.

## Accessibility Facts

Accessibility snapshots must include role, label, description, value, state,
range facts, ownership refs, and any native role fallback produced by tag
choice.

## Evidence

- `webNodeRelations(...)` and `webNodeRelationRefs(...)` return expected
  pre-mount links.
- `webDOMRelations(...)` and `webDOMRelationRefs(...)` return expected mounted
  links.
- `webAccessibilitySnapshot(...)` and `webDOMAccessibilitySnapshot(...)` expose
  the same semantic packet.
