# Native Element Surface

Goal: Kry names that have clear browser semantics render as native elements,
while ambiguous app widgets keep conservative tags.

## Current Direction

Use native tags when the widget maps cleanly to HTML semantics:

- document and metadata: `Base`, `Meta`, `Title`;
- content: `Article`, `Aside`, `Header`, `Footer`, `Main`, `Section`,
  `Search`, `Hgroup`;
- text: `Paragraph`, `BlockQuote`, `Quote`, `Code`, `Strong`, `Em`, `Abbr`,
  `Data`, `Del`, `Ins`, `Sub`, `Sup`, `Kbd`, `Samp`, `Var`, `Cite`, `Ruby`,
  `Rt`, `Rp`, `Bdi`, `Bdo`, `Br`, `LineBreak`, `Wbr`;
- lists and tables: `List`, `OrderedList`, `ListItem`, `DescriptionList`,
  `DescriptionTerm`, `DescriptionDetails`, `Table`, `TableHead`, `TableBody`,
  `TableFoot`, `TableRow`, `TableCell`, `ColGroup`, `Col`;
- forms: `Form`, `Label`, `Fieldset`, `Legend`, `Select`, `OptionGroup`,
  `Option`, `Datalist`, `Output`;
- media and embedding: `Image`, `ImageMap`, `Area`, `Video`, `Audio`,
  `Source`, `Track`, `IFrame`, `Embed`, `EmbeddedObject`, `Param`;
- composition: `Template`, `Slot`, `Script`, `StyleElement`, `NoScript`.

## Steps

1. Generate an inventory of widget names that still render as `div`.
2. Classify each as native-safe, ARIA-only, app-specific, or intentionally
   conservative.
3. Add native aliases only when browser semantics are stable and unsurprising.
4. Avoid misleading names and avoid compatibility names that conflict with
   existing public naming rules.
5. Update runtime tag mapping, parser whitelist, TypeScript declarations,
   feature matrix, canonical surface docs, and tests together.

## Evidence

- `widgetTag(...)` maps every accepted native alias.
- `tests/public_api_names_test.sh` guards the runtime constructor surface.
- Browser tests assert actual `tagName` values after mount.
