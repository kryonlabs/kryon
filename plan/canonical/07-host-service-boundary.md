# Remaining host-service inventory

`docs/CANONICAL_WIDGET_SURFACE.md` records widget ownership and the completed
C release-call classification. Complete the inventory at behavior granularity:

- Native and Go retained focus/selection/menu state: storage is host support;
  navigation targets, close rules, and ownership transitions are policy.
- Text: buffers/IME/measurement/paint are host services; edit, selection,
  reflow and composition decisions need explicit generated owners.
- Blocks: clipping and paint stacks are host services; scope visibility,
  activation, restoration and dismissal behavior must agree across backends.
- Game2D: keep physics/audio/rendering and scene ownership explicitly outside
  the widget migration unless a separate scene-policy migration is agreed.
- Terminal: retain only reusable runtime/host primitives in Kryon. Kapsule's
  terminal emulator and product behavior belong in Kapsule.

Done when every retained native/Go path has either a specific service reason
or a shared `.kry` policy owner, with no old API adapters left in maintained use.
The paused JS/web path is future-roadmap reference material, not a current
completion gate.
