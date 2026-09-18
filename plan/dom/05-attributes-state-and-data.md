# Attributes, State, And Data

Goal: authored Kry metadata becomes native browser attributes and synchronized
Kry DOM facts.

## Attribute Sources

- first-class Kry metadata fields;
- direct `.kry` widget args;
- `data_*`, `dom_data_*`, and `html_data_*`;
- `aria_*`, `dom_aria_*`, and `html_aria_*`;
- `attr_*`, `dom_attr_*`, and `html_attr_*`;
- runtime mutations through mounted DOM helper APIs.

## Steps

1. Keep compiler metadata authoritative over fallback runtime args.
2. Promote common native attributes into stable fields when they affect
   selectors, relationships, accessibility, or form behavior.
3. Keep arbitrary attributes in `extraAttrs`.
4. Normalize boolean attributes so snapshots and DOM output agree.
5. Reflect mounted mutations back into Web Document facts.
6. Ensure KSS selectors can match global attrs, ARIA attrs, data attrs, and
   extra attrs consistently before and after mount.

## Required Native Families

- global attributes: `id`, `name`, `title`, `lang`, `dir`, `hidden`,
  `draggable`, `contenteditable`, `tabindex`, `part`, `slot`, `popover`;
- form attributes: `type`, `value`, `placeholder`, `required`, `readonly`,
  `autocomplete`, `min`, `max`, `step`, `pattern`, `multiple`;
- link attributes: `href`, `target`, `rel`, `download`, `ping`,
  `hreflang`, `referrerpolicy`;
- media attributes: `src`, `srcset`, `sizes`, `loading`, `decoding`,
  `fetchpriority`, `crossorigin`, `poster`, `controls`, `preload`;
- document attrs: metadata, script, style, template, slot, object, iframe,
  and image-map attrs.

## Evidence

- Pre-mount snapshots expose the expected fields.
- Mounted DOM attributes match the same facts.
- Attribute selector queries work through both `webNodeQuery(...)` and
  `webDOMQuery(...)`.
