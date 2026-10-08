# Compact navigation

`NavigationBar` can compose a compact vertical rail or horizontal dock using
the same themed buttons, keyboard activation and retained pointer capture as
other Kryon controls.

```ziran
props: NavigationBarProps
props.key = KeyHash("navigation")
props.compact = true
props.vertical = true
props.bounds = Rectangle.{0.0, 0.0, 88.0, 720.0}
props.footer_count = 2
props.scroll_offset = navigation_scroll
result := NavigationBar(session, props, items[:])
navigation_scroll = result.scroll_offset
```

The last `footer_count` items stay at the bottom of a rail or the trailing
edge of a dock. Other items scroll without covering those actions. Supply
stable item keys when shortcuts can move. Items carry route IDs, labels,
`ImageProps`, optional vector `icon_shape`, selected state and disabled state;
the application applies `result.clicked_route` to its own routing.

Set `draggable` to receive `result.drag`, `drag_route` and `drag_bounds` for
shortcut cards. While dragging a card, supply `dragging` and `drag_position`;
`drop_index` reports the insertion slot among the scrolling shortcuts, or
`-1` outside their viewport. Footer actions are never insertion slots.

The surface resolves `NavigationBar` style rules and buttons resolve
`NavigationBarItem` rules. `class_name` and `item_class_name` let applications
customize those faces with their current palette. Compact items use ordinary
button hover, focus, press, selection and accessibility behavior.
