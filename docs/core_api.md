# Kryon Core API Reference

## Overview

The Kryon Core API is a pure C99 interface that provides the fundamental building blocks for the framework. It is designed to be embedded in any application and linked with any renderer.

## Component System

### `kryon_component_create`
Creates a new component instance.

### `kryon_component_add_child`
Adds a child component to a parent container.

### `kryon_component_set_bounds`
Sets the layout bounds of a component.

## Layout System

### `kryon_layout_calculate`
Performs a layout pass on the component tree.

## Event System

### `kryon_dispatch_event`
Dispatches an event (mouse, keyboard, touch) to the component tree.

## State Management

### `kryon_state_create`
Creates a new state object.

## Renderer Interface

Backends must implement the following function pointers:

*   `draw_rect_fn`
*   `draw_text_fn`
*   `measure_text_fn`
*   `get_window_size_fn`
