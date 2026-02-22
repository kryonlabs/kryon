# Widget Catalog

Complete catalog of all 89 widgets in Kryon.

## Table of Contents

1. [Input Widgets (24)](#input-widgets)
2. [Button Widgets (8)](#button-widgets)
3. [Display Widgets (11)](#display-widgets)
4. [Container Widgets (11)](#container-widgets)
5. [List & Data Widgets (8)](#list--data-widgets)
6. [Navigation Widgets (8)](#navigation-widgets)
7. [Menu Widgets (4)](#menu-widgets)
8. [Form Widgets (4)](#form-widgets)
9. [Window Widgets (4)](#window-widgets)
10. [Specialized Widgets (8)](#specialized-widgets)

---

## Input Widgets

### Text Inputs

#### 1. TextInput
**Type**: `textinput`
**Description**: Single-line text input field
**Properties**:
- `text` (string): Current text content
- `placeholder` (string): Placeholder text
- `max_length` (int): Maximum characters (0 = unlimited)
- `password` (bool): Obscure text (0/1)
**Events**: `changed`, `submitted`, `focused`, `blurred`
**Example**: `echo "create_widget 1 textinput 0 placeholder='Enter text'" > /ctl`

#### 2. PasswordInput
**Type**: `passwordinput`
**Description**: Password field (text obscured with bullets/dots)
**Properties**: Same as TextInput, always obscured
**Events**: `changed`, `submitted`

#### 3. TextArea
**Type**: `textarea`
**Description**: Multi-line text input
**Properties**:
- `text` (string): Text content (multiline)
- `placeholder` (string): Placeholder text
- `max_length` (int): Maximum characters
- `readonly` (bool): Read-only mode
- `line_numbers` (bool): Show line numbers
**Events**: `changed`, `focused`, `blurred`

#### 4. SearchInput
**Type**: `searchinput`
**Description**: Search field with clear button
**Properties**: Same as TextInput, plus:
- `show_clear` (bool): Show clear button
**Events**: `changed`, `submitted`, `cleared`

#### 5. NumberInput
**Type**: `numberinput`
**Description**: Numeric input with validation
**Properties**:
- `value` (int): Numeric value
- `min` (int): Minimum value
- `max` (int): Maximum value
- `step` (int): Increment/decrement step
**Events**: `changed`, `increment`, `decrement`

#### 6. EmailInput
**Type**: `emailinput`
**Description**: Email input with validation
**Properties**: Same as TextInput, validates email format
**Events**: `changed`, `validated`, `invalid`

#### 7. UrlInput
**Type**: `urlinput`
**Description**: URL input with validation
**Properties**: Same as TextInput, validates URL format
**Events**: `changed`, `validated`, `invalid`

#### 8. TelInput
**Type**: `telinput`
**Description**: Telephone number input
**Properties**: Same as TextInput, formats phone numbers
**Events**: `changed`, `formatted`

### Selection Widgets

#### 9. Checkbox
**Type**: `checkbox`
**Description**: Boolean toggle checkbox
**Properties**:
- `value` (bool): Checked state (0/1)
- `label` (string): Label text
- `tristate` (bool): Allow indeterminate state
**Events**: `changed`

#### 10. RadioButton
**Type**: `radiobutton`
**Description**: Mutually exclusive selection
**Properties**:
- `value` (bool): Selected state (0/1)
- `label` (string): Label text
- `group` (string): Radio group name
**Events**: `changed`

#### 11. Dropdown
**Type**: `dropdown`
**Description**: Dropdown select (single selection)
**Properties**:
- `value` (string): Selected value
- `options` (array): Available options
- `placeholder` (string): Placeholder text
**Events**: `changed`, `opened`, `closed`

#### 12. MultiSelect
**Type**: `multiselect`
**Description**: Dropdown select (multi-selection)
**Properties**:
- `values` (array): Selected values
- `options` (array): Available options
- `placeholder` (string): Placeholder text
**Events**: `changed`, `opened`, `closed`

#### 13. ListBox
**Type**: `listbox`
**Description**: Scrollable list selection
**Properties**:
- `value` (string): Selected value (or first of multiple)
- `values` (array): Selected values
- `options` (array): Available options
- `multiple` (bool): Allow multiple selection
**Events**: `changed`, `selected`, `deselected`

#### 14. Switch
**Type**: `switch`
**Description**: Toggle switch (iOS style)
**Properties**:
- `value` (bool): On/off state (0/1)
- `label` (string): Label text
**Events**: `changed`

#### 15. Slider
**Type**: `slider`
**Description**: Numeric slider
**Properties**:
- `value` (int): Current value
- `min` (int): Minimum value
- `max` (int): Maximum value
- `step` (int): Step size
- `show_value` (bool): Display current value
**Events**: `changed`, `drag_start`, `drag_end`

#### 16. RangeSlider
**Type**: `rangeslider`
**Description**: Dual-handle range slider
**Properties**:
- `value_min` (int): Lower bound
- `value_max` (int): Upper bound
- `min` (int): Minimum possible value
- `max` (int): Maximum possible value
**Events**: `changed`, `drag_start`, `drag_end`

#### 17. Rating
**Type**: `rating`
**Description**: Star rating widget
**Properties**:
- `value` (int): Rating (0 to max)
- `max` (int): Maximum rating (default: 5)
- `icon` (string): Icon for filled (default: "star")
- `empty_icon` (string): Icon for empty
**Events**: `changed`

#### 18. Spinbox
**Type**: `spinbox`
**Description**: Numeric input with increment/decrement buttons
**Properties**: Same as NumberInput
**Events**: `changed`, `increment`, `decrement`

### Date/Time Widgets

#### 19. DateInput
**Type**: `dateinput`
**Description**: Date picker
**Properties**:
- `value` (string): ISO date format "YYYY-MM-DD"
- `min` (string): Minimum date
- `max` (string): Maximum date
- `format` (string): Display format
**Events**: `changed`, `opened`, `closed`

#### 20. TimeInput
**Type**: `timeinput`
**Description**: Time picker
**Properties**:
- `value` (string): Time format "HH:MM"
- `format` (string): Display format (12h/24h)
**Events**: `changed`

#### 21. DateTimeInput
**Type**: `datetimeinput`
**Description**: Combined date/time picker
**Properties**: Combination of DateInput and TimeInput
**Events**: `changed`

#### 22. MonthInput
**Type**: `monthinput`
**Description**: Month picker
**Properties**:
- `value` (string): "YYYY-MM"
- `min` (string): Minimum month
- `max` (string): Maximum month
**Events**: `changed`

#### 23. WeekInput
**Type**: `weekinput`
**Description**: Week picker
**Properties**:
- `value` (string): "YYYY-Www" (week number)
- `min` (string): Minimum week
- `max` (string): Maximum week
**Events**: `changed`

### File Input

#### 24. FileInput
**Type**: `fileinput`
**Description**: File upload widget
**Properties**:
- `value` (string): Selected file path
- `accept` (string): Accepted file types (MIME)
- `multiple` (bool): Allow multiple files
**Events**: `changed`, `selected`

---

## Button Widgets

#### 25. Button
**Type**: `button`
**Description**: Standard button
**Properties**:
- `text` (string): Button label
- `icon` (string): Icon name
- `style` (enum): "default", "primary", "secondary", "danger"
**Events**: `clicked`

#### 26. SubmitButton
**Type**: `submitbutton`
**Description**: Form submit button
**Properties**: Same as Button, submits parent form
**Events**: `clicked`

#### 27. ResetButton
**Type**: `resetbutton`
**Description**: Form reset button
**Properties**: Same as Button, resets parent form
**Events**: `clicked`

#### 28. IconButton
**Type**: `iconbutton`
**Description**: Icon-only button
**Properties**:
- `icon` (string): Icon name
- `tooltip` (string): Tooltip text
**Events**: `clicked`

#### 29. FloatingActionButton
**Type**: `floatingactionbutton`
**Description**: FAB (Material Design)
**Properties**:
- `icon` (string): Icon name
- `position` (enum): "top-left", "top-right", "bottom-left", "bottom-right"
**Events**: `clicked`

#### 30. DropdownButton
**Type**: `dropdownbutton`
**Description**: Button with dropdown menu
**Properties**:
- `text` (string): Button label
- `icon` (string): Icon name
- `menu` (array): Menu items
**Events**: `clicked`, `menu_opened`, `menu_closed`, `item_selected`

#### 31. SegmentedControl
**Type**: `segmentedcontrol`
**Description**: Segmented button group
**Properties**:
- `options` (array): Available options
- `value` (string): Selected option
**Events**: `changed`

---

## Display Widgets

#### 32. Label
**Type**: `label`
**Description**: Static text label
**Properties**:
- `text` (string): Label text
- `color` (color): Text color
- `font` (font): Font specification
- `align` (enum): "left", "center", "right"
**Events**: None (display only)

#### 33. Heading
**Type**: `heading`
**Description**: Heading (h1-h6)
**Properties**:
- `text` (string): Heading text
- `level` (int): Heading level (1-6)
- `color` (color): Text color
**Events**: None

#### 34. Paragraph
**Type**: `paragraph`
**Description**: Paragraph text
**Properties**:
- `text` (string): Paragraph text (may contain newlines)
- `line_height` (real): Line spacing multiplier
**Events**: None

#### 35. RichText
**Type**: `richtext`
**Description**: Formatted text with spans
**Properties**:
- `text` (string): Text with markup (Markdown/HTML subset)
- `format` (enum): "markdown", "html"
**Events**: `link_clicked`

#### 36. Code
**Type**: `code`
**Description**: Monospace code block
**Properties**:
- `text` (string): Code content
- `language` (string): Language for syntax highlighting
- `line_numbers` (bool): Show line numbers
**Events**: None

#### 37. Image
**Type**: `image`
**Description**: Image display
**Properties**:
- `src` (string): Image path/URL
- `width` (int): Display width (0 = natural)
- `height` (int): Display height (0 = natural)
- `fit` (enum): "contain", "cover", "fill", "none"
**Events**: `loaded`, `error`

#### 38. ProgressBar
**Type**: `progressbar`
**Description**: Linear progress indicator
**Properties**:
- `value` (int): Progress (0-100)
- `indeterminate` (bool): Show indeterminate animation
**Events**: None

#### 39. ActivityIndicator
**Type**: `activityindicator`
**Description**: Spinning activity indicator
**Properties**:
- `active` (bool): Show/hide animation
**Events**: None

#### 40. Badge
**Type**: `badge`
**Description**: Small status badge
**Properties**:
- `text` (string): Badge text
- `color` (color): Badge color
**Events**: None

#### 41. Divider
**Type**: `divider`
**Description**: Horizontal/vertical divider
**Properties**:
- `orientation` (enum): "horizontal", "vertical"
- `thickness` (int): Line thickness (pixels)
**Events**: None

#### 42. Tooltip
**Type**: `tooltip`
**Description**: Hover tooltip
**Properties**:
- `text` (string): Tooltip text
- `position` (enum): "top", "bottom", "left", "right"
**Events**: `shown`, `hidden`

---

## Container Widgets

#### 43. Container
**Type**: `container`
**Description**: Generic container with styling
**Properties**:
- `color` (color): Background color
- `border_color` (color): Border color
- `border_width` (int): Border thickness
- `corner_radius` (int): Rounded corners
- `shadow` (bool): Drop shadow
**Events**: None

#### 44. Box
**Type**: `box`
**Description**: Simple box container
**Properties**: Same as Container, minimal
**Events**: None

#### 45. Frame
**Type**: `frame`
**Description**: Decorative frame/border
**Properties**:
- `title` (string): Frame title
- `border_width` (int): Border thickness
**Events**: None

#### 46. Card
**Type**: `card`
**Description**: Card panel with elevation
**Properties**:
- `elevation` (int): Shadow depth (0-5)
- `corner_radius` (int): Rounded corners
**Events**: `clicked`

#### 47. ScrollArea
**Type**: `scrollarea`
**Description**: Scrollable content area
**Properties**:
- `scroll_x` (int): Horizontal scroll position
- `scroll_y` (int): Vertical scroll position
- `overflow` (enum): "scroll", "auto", "hidden"
**Events**: `scrolled`

#### 48. SplitPane
**Type**: `splitpane`
**Description**: Resizable split panes
**Properties**:
- `orientation` (enum): "horizontal", "vertical"
- `split_position` (int): Divider position
- `min_size` (int): Minimum pane size
**Events**: `split_moved`

#### 49. TabContainer
**Type**: `tabcontainer`
**Description**: Tabbed container
**Properties**:
- `value` (string): Active tab
- `tabs` (array): Tab definitions
- `position` (enum): "top", "bottom", "left", "right"
**Events**: `changed`

#### 50. Dialog
**Type**: `dialog`
**Description**: Modal dialog
**Properties**:
- `title` (string): Dialog title
- `closable` (bool): Show close button
**Events**: `closed`

#### 51. Modal
**Type**: `modal`
**Description**: Modal overlay
**Properties**:
- `backdrop` (bool): Show dimmed backdrop
**Events**: `backdrop_clicked`

#### 52. ActionSheet
**Type**: `actionsheet`
**Description**: iOS-style action sheet
**Properties**:
- `actions` (array): Action buttons
**Events**: `action_selected`, `dismissed`

#### 53. Drawer
**Type**: `drawer`
**Description**: Side drawer/panel
**Properties**:
- `position` (enum): "left", "right", "top", "bottom"
- `size` (int): Drawer size (pixels)
- `open` (bool): Open/closed state
**Events**: `opened`, `closed`

---

## List & Data Widgets

#### 54. ListItem
**Type**: `listitem`
**Description**: Single list item
**Properties**:
- `text` (string): Item text
- `selected` (bool): Selected state
**Events**: `clicked`, `selected`

#### 55. ListTile
**Type**: `listtile`
**Description**: Material-style list tile
**Properties**:
- `text` (string): Primary text
- `secondary_text` (string): Secondary text
- `icon` (string): Leading icon
- `trailing` (string): Trailing icon/text
**Events**: `clicked`

#### 56. ListView
**Type**: `listview`
**Description**: Scrollable list
**Properties**:
- `items` (array): List items
- `selected` (int): Selected index
**Events**: `selected`, `scrolled`

#### 57. GridView
**Type**: `gridview`
**Description**: Grid layout
**Properties**:
- `items` (array): Grid items
- `columns` (int): Number of columns
- `spacing` (int): Space between items
**Events**: `selected`

#### 58. DataTable
**Type**: `datatable`
**Description**: Data table with sorting
**Properties**:
- `columns` (array): Column definitions
- `rows` (array): Row data
- `sort_column` (int): Sorted column
- `sort_order` (enum): "asc", "desc"
**Events**: `cell_clicked`, `sorted`

#### 59. TreeView
**Type**: `treeview`
**Description**: Hierarchical tree view
**Properties**:
- `nodes` (array): Tree nodes
- `expanded` (array): Expanded node IDs
**Events**: `expanded`, `collapsed`, `selected`

#### 60. Chip
**Type**: `chip`
**Description**: Material-style chip
**Properties**:
- `text` (string): Chip text
- `icon` (string): Icon (optional)
- `deletable` (bool): Show delete button
**Events**: `deleted`

#### 61. Tag
**Type**: `tag`
**Description**: Simple tag
**Properties**:
- `text` (string): Tag text
- `color` (color): Tag color
**Events**: `clicked`

---

## Navigation Widgets

#### 62. MenuBar
**Type**: `menubar`
**Description**: Menu bar
**Properties**:
- `items` (array): Menu items
**Events**: `item_selected`

#### 63. ToolBar
**Type**: `toolbar`
**Description**: Tool bar
**Properties**:
- `items` (array): Tool buttons
**Events**: `item_clicked`

#### 64. NavBar
**Type**: `navbar`
**Description**: Bottom navigation bar
**Properties**:
- `items` (array): Nav items
- `value` (string): Active item
**Events**: `changed`

#### 65. NavRail
**Type**: `navrail`
**Description**: Side navigation rail
**Properties**: Same as NavBar
**Events**: `changed`

#### 66. Breadcrumb
**Type**: `breadcrumb`
**Description**: Breadcrumb navigation
**Properties**:
- `items` (array): Breadcrumb items
**Events**: `item_clicked`

#### 67. Pagination
**Type**: `pagination`
**Description**: Pagination control
**Properties**:
- `page` (int): Current page
- `total_pages` (int): Total pages
**Events**: `changed`

#### 68. Stepper
**Type**: `stepper`
**Description**: Step wizard
**Properties**:
- `step` (int): Current step
- `steps` (array): Step definitions
**Events**: `next`, `previous`, `completed`

#### 69. PageView
**Type**: `pageview`
**Description**: Swipeable pages
**Properties**:
- `page` (int): Current page
- `pages` (int): Total pages
**Events**: `changed`, `swiped`

---

## Menu Widgets

#### 70. DropdownMenu
**Type**: `dropdownmenu`
**Description**: Dropdown menu
**Properties**:
- `items` (array): Menu items
**Events**: `opened`, `closed`, `item_selected`

#### 71. ContextMenu
**Type**: `contextmenu`
**Description**: Right-click context menu
**Properties**:
- `items` (array): Menu items
**Events**: `triggered`, `item_selected`

#### 72. CascadingMenu
**Type**: `cascadingmenu`
**Description**: Nested menu
**Properties**:
- `items` (array): Menu items (with submenus)
**Events**: `item_selected`

#### 73. PopupMenu
**Type**: `popupmenu`
**Description**: Positioned popup menu
**Properties**:
- `items` (array): Menu items
- `x`, `y` (int): Position
**Events**: `opened`, `closed`, `item_selected`

---

## Form Widgets

#### 74. Form
**Type**: `form`
**Description**: Form container
**Properties**:
- `action` (string): Submit action
- `method` (enum): "post", "get"
**Events**: `submitted`, `reset`

#### 75. FormGroup
**Type**: `formgroup`
**Description**: Form group with label
**Properties**:
- `label` (string): Group label
- `error` (string): Validation error
**Events**: None

#### 76. ValidationMessage
**Type**: `validationmessage`
**Description**: Validation error message
**Properties**:
- `text` (string): Error message
- `severity` (enum): "error", "warning", "info"
**Events**: None

#### 77. InputGroup
**Type**: `inputgroup`
**Description**: Input with prefix/suffix
**Properties**:
- `prefix` (string): Prefix text/icon
- `suffix` (string): Suffix text/icon
**Events**: None (container)

---

## Window Widgets

#### 78. Window
**Type**: `window`
**Description**: Main window widget
**Properties**: (See Multi-Window Architecture)
**Events**: (See Multi-Window Architecture)

#### 79. DialogWindow
**Type**: `dialogwindow`
**Description**: Dialog window
**Properties**: (See Multi-Window Architecture)
**Events**: (See Multi-Window Architecture)

#### 80. PageScaffold
**Type**: `pagescaffold`
**Description**: Page scaffold (app bar + body + FAB)
**Properties**:
- `app_bar` (widget): Top app bar
- `body` (widget): Main content
- `fab` (widget): Floating action button
**Events**: None

#### 81. Fullscreen
**Type**: `fullscreen`
**Description**: Fullscreen container
**Properties**: None (fills entire screen)
**Events**: None

---

## Specialized Widgets

#### 82. Canvas
**Type**: `canvas`
**Description**: Drawing canvas
**Properties**:
- `width` (int): Canvas width
- `height` (int): Canvas height
**Events**: `draw`, `mouse_down`, `mouse_move`, `mouse_up`

#### 83. Svg
**Type**: `svg`
**Description**: SVG rendering
**Properties**:
- `src` (string): SVG content/path
- `width` (int): Display width
- `height` (int): Display height
**Events**: `loaded`, `error`

#### 84. Chart
**Type**: `chart`
**Description**: Chart/graph widget
**Properties**:
- `type` (enum): "line", "bar", "pie", "scatter"
- `data` (array): Chart data
- `labels` (array): Axis labels
**Events**: `point_clicked`

#### 85. ColorPicker
**Type**: `colorpicker`
**Description**: Color picker
**Properties**:
- `value` (color): Selected color
**Events**: `changed`

#### 86. SignaturePad
**Type**: `signaturepad`
**Description**: Signature capture
**Properties**:
- `data` (string): Signature data (base64/image)
**Events**: `signed`, `cleared`

#### 87. Resizable
**Type**: `resizable`
**Description**: Resizable handle
**Properties**:
- `direction` (enum): "horizontal", "vertical", "both"
**Events**: `resized`

#### 88. Calendar
**Type**: `calendar`
**Description**: Full calendar widget
**Properties**:
- `date` (string): Selected date
- `events` (array): Calendar events
**Events**: `date_selected`, `event_clicked`

#### 89. TableView
**Type**: `tableview`
**Description**: Two-dimensional table
**Properties**:
- `rows` (int): Number of rows
- `columns` (int): Number of columns
- `cells` (array): Cell data
**Events**: `cell_selected`, `cell_edited`

---

## Widget Index

By Category:
- **Input**: 1-24 (24 widgets)
- **Buttons**: 25-31 (7 widgets)
- **Display**: 32-42 (11 widgets)
- **Containers**: 43-53 (11 widgets)
- **Lists/Data**: 54-61 (8 widgets)
- **Navigation**: 62-69 (8 widgets)
- **Menus**: 70-73 (4 widgets)
- **Forms**: 74-77 (4 widgets)
- **Windows**: 78-81 (4 widgets)
- **Specialized**: 82-89 (8 widgets)

**Total: 89 widgets**

---

## See Also

- [Base Widget Interface](00-base-widget.md) - Base widget interface
- [Property System](01-property-system.md) - Property types
- [Event System](02-event-system.md) - Event types
- [Layout System](03-layout-system.md) - Layout algorithms
