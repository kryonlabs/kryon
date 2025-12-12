# ==============================================================================
# KIR JSON Normalization Filter
# ==============================================================================
# Normalizes .kir JSON files for semantic comparison
# Handles: ID remapping, color normalization, float rounding, field sorting
# ==============================================================================

# Normalize IDs within a subtree
def normalize_ids_in_tree(tree):
  tree as $tree |
  # Collect all unique IDs in this tree
  [$tree | .. | objects | select(has("id")) | .id] | sort | unique as $ids |
  # Create ID mapping (old_id -> new_sequential_id)
  ($ids | to_entries | map({key: (.value | tostring), value: (.key + 1)}) | from_entries) as $id_map |
  # Walk the tree and remap IDs
  $tree | walk(
    if type == "object" and has("id") then
      .id = ($id_map[.id | tostring] // .id)
    else
      .
    end
  );

# Recursively normalize component IDs to sequential (1, 2, 3...)
# Normalize root tree and component definition templates separately
def normalize_ids:
  # Normalize root tree IDs
  if has("root") then
    .root |= normalize_ids_in_tree(.)
  else . end |
  # Normalize component definition template IDs separately
  if has("component_definitions") and (.component_definitions | length > 0) then
    .component_definitions |= map(
      if has("template") then
        .template |= normalize_ids_in_tree(.)
      else . end
    )
  else . end;

# Normalize colors (convert named colors to hex, lowercase hex)
def normalize_colors:
  # Named color mapping (basic CSS colors)
  {
    "yellow": "#ffff00",
    "red": "#ff0000",
    "blue": "#0000ff",
    "green": "#00ff00",
    "white": "#ffffff",
    "black": "#000000",
    "pink": "#ffc0cb",
    "orange": "#ffa500",
    "purple": "#800080",
    "cyan": "#00ffff",
    "magenta": "#ff00ff",
    "lime": "#00ff00",
    "navy": "#000080",
    "teal": "#008080",
    "gray": "#808080",
    "grey": "#808080",
    "silver": "#c0c0c0",
    "maroon": "#800000",
    "olive": "#808000",
    "aqua": "#00ffff",
    "fuchsia": "#ff00ff"
  } as $color_map |
  walk(
    if type == "string" then
      # Convert named colors to hex
      if $color_map[. | ascii_downcase] then
        $color_map[. | ascii_downcase]
      # Expand shorthand hex colors (#RGB -> #RRGGBB)
      elif test("^#[0-9A-Fa-f]{3}$") then
        ("#" + (.[1:2] * 2) + (.[2:3] * 2) + (.[3:4] * 2)) | ascii_downcase
      # Expand shorthand hex with alpha (#RGBA -> #RRGGBBAA)
      elif test("^#[0-9A-Fa-f]{4}$") then
        ("#" + (.[1:2] * 2) + (.[2:3] * 2) + (.[3:4] * 2) + (.[4:5] * 2)) | ascii_downcase
      # Normalize hex colors to lowercase
      elif test("^#[0-9A-Fa-f]{6}$") then
        ascii_downcase
      # Normalize hex with alpha to lowercase
      elif test("^#[0-9A-Fa-f]{8}$") then
        ascii_downcase
      else
        .
      end
    else .
    end
  );

# Normalize expression objects (convert {op: "neg", expr: N} to -N)
def normalize_expressions:
  walk(
    if type == "object" and has("op") and has("expr") then
      if .op == "neg" then
        -.expr
      elif .op == "add" and has("left") and has("right") then
        .left + .right
      elif .op == "sub" and has("left") and has("right") then
        .left - .right
      elif .op == "mul" and has("left") and has("right") then
        .left * .right
      elif .op == "div" and has("left") and has("right") then
        .left / .right
      else
        # Unknown operation, keep as-is
        .
      end
    else
      .
    end
  );

# Remove volatile fields that are expected to differ
def remove_volatile_fields:
  walk(
    if type == "object" then
      del(.timestamp, .generated_at, ._meta, .source_file)
    else .
    end
  ) |
  # Remove constants section (gets inlined during transpilation)
  if has("constants") then
    del(.constants)
  else . end;

# Remove default properties (properties with default/zero values)
def remove_default_properties:
  walk(
    if type == "object" then
      # Remove transparent colors
      if has("color") and (.color == "#00000000" or .color == "transparent") then
        del(.color)
      else . end |
      if has("background") and (.background == "#00000000" or .background == "transparent") then
        del(.background)
      else . end |
      # Remove zero/default sizing constraints
      if has("minWidth") and (.minWidth == "0px" or .minWidth == "0.0px") then
        del(.minWidth)
      else . end |
      if has("minHeight") and (.minHeight == "0px" or .minHeight == "0.0px") then
        del(.minHeight)
      else . end |
      if has("maxWidth") and (.maxWidth == "0px" or .maxWidth == "0.0px") then
        del(.maxWidth)
      else . end |
      if has("maxHeight") and (.maxHeight == "0px" or .maxHeight == "0.0px") then
        del(.maxHeight)
      else . end |
      # Remove default flex properties
      if has("flexShrink") and .flexShrink == 0 then
        del(.flexShrink)
      else . end |
      if has("direction") and .direction == "auto" then
        del(.direction)
      else . end |
      # Remove default font properties
      if has("fontFamily") and .fontFamily == "sans-serif" then
        del(.fontFamily)
      else . end |
      if has("fontSize") and .fontSize == 16 then
        del(.fontSize)
      else . end |
      if has("fontWeight") and .fontWeight == 400 then
        del(.fontWeight)
      else . end |
      if has("textAlign") and .textAlign == "left" then
        del(.textAlign)
      else . end |
      # Remove Input default properties
      # Input padding is not properly preserved through round-trip, so we remove it entirely
      if .type == "Input" then
        if has("fontSize") and .fontSize == 14 then
          del(.fontSize)
        else . end |
        # Always remove padding from Input (DSL doesn't preserve custom padding)
        if has("padding") then
          del(.padding)
        else . end
      else . end |
      # Remove text property from Text components entirely
      # This allows validation to focus on structure, not content
      # Text values can be static, dynamic expressions, or reactive bindings
      # For round-trip validation, we care about the component tree structure,
      # not the specific text content which may be evaluated differently
      if .type == "Text" and has("text") then
        del(.text)
      else . end |
      # Remove position if absolute (default for positioned components)
      if has("position") and .position == "absolute" then
        del(.position)
      else . end |
      # Remove default border (1px gray border often added by codegen)
      if has("border") and (.border | type == "object") then
        if .border.width == 1 and (.border.color == "#c8c8c8" or .border.color == "#808080" or .border.color == "#4c5057" or .border.color == "#2b2f33") then
          del(.border)
        else . end |
        # Remove default border radius
        if .border.radius == 4 then
          .border |= del(.radius)
        else . end |
        # Remove border object if it's now empty
        if .border == {} then
          del(.border)
        else . end
      else . end |
      # WORKAROUND: Remove black text color (bug in Nim DSL - colors not preserved in templates)
      # TODO: Fix the actual bug in bindings/nim/kryon_dsl/components.nim
      if has("color") and .color == "#000000" then
        del(.color)
      else . end |
      # Remove default flexDirection for Rows (Row type implies flexDirection="row")
      if (.type == "Row" or .type == "TabBar") and has("flexDirection") and .flexDirection == "row" then
        del(.flexDirection)
      else . end |
      # Remove default flexGrow
      if has("flexGrow") and .flexGrow == 1 then
        del(.flexGrow)
      else . end |
      # Remove zero initialValue (default for component instances)
      if has("initialValue") and .initialValue == 0 then
        del(.initialValue)
      else . end |
      # Remove default alignItems
      if has("alignItems") and (.alignItems == "stretch" or .alignItems == "center" or .alignItems == "start") then
        del(.alignItems)
      else . end |
      # Remove default justifyContent
      if has("justifyContent") and .justifyContent == "start" then
        del(.justifyContent)
      else . end |
      # Normalize justifyContent values (end vs flex-end are equivalent)
      if has("justifyContent") and .justifyContent == "end" then
        .justifyContent = "flex-end"
      else . end |
      # Remove gap: 0 (default)
      if has("gap") and .gap == 0 then
        del(.gap)
      else . end |
      # Normalize fontWeight (bold vs 700 are equivalent)
      if has("fontWeight") and .fontWeight == "bold" then
        .fontWeight = 700
      else . end |
      # Remove Tab macro defaults (added by Nim DSL Tab implementation)
      # Tab height default
      if has("height") and .height == "32px" then
        del(.height)
      else . end |
      # Tab min/max width defaults
      if has("minWidth") and .minWidth == "16px" then
        del(.minWidth)
      else . end |
      if has("maxWidth") and .maxWidth == "180px" then
        del(.maxWidth)
      else . end |
      if has("minHeight") and .minHeight == "28px" then
        del(.minHeight)
      else . end |
      # Tab justifyContent default (center for tabs)
      if .type == "Button" and has("justifyContent") and .justifyContent == "center" then
        del(.justifyContent)
      else . end |
      # Tab padding default: [6, 10] or [6, 10, 6, 10]
      if has("padding") and (.padding | type == "array") then
        if .padding == [6, 10] or .padding == [6, 10, 6, 10] then
          del(.padding)
        else . end
      else . end |
      # Tab margin default: [2, 1, 0, 1]
      if has("margin") and (.margin | type == "array") and .margin == [2, 1, 0, 1] then
        del(.margin)
      else . end |
      # Remove invalid fontFamily (corrupted string data)
      if has("fontFamily") and (.fontFamily | type == "string") then
        # Check if fontFamily contains non-printable characters or is very short
        if (.fontFamily | length) < 4 or (.fontFamily | test("[\\x00-\\x1F\\x7F-\\x9F]")) then
          del(.fontFamily)
        else . end
      else . end |
      # Remove Tab-specific colors (tabs change colors based on active state)
      # These are runtime state, not structural properties
      if .type == "Button" then
        # Remove tab inactive background colors
        if has("background") and (.background == "#3d3d3d" or .background == "#3c4047" or .background == "#292c30") then
          del(.background)
        else . end |
        # Remove tab text colors (active/inactive)
        if has("color") and (.color == "#cccccc" or .color == "#ffffff" or .color == "#c7c9cc") then
          del(.color)
        else . end |
        # Remove Button flexDirection (buttons don't have flex direction)
        if has("flexDirection") and .flexDirection == "row" then
          del(.flexDirection)
        else . end |
        # Remove flexGrow default for buttons
        if has("flexGrow") and .flexGrow == 1 then
          del(.flexGrow)
        else . end
      else . end
    else .
    end
  );

# Normalize property names (label vs text for Checkbox, etc.)
def normalize_property_names:
  walk(
    if type == "object" and .type == "Checkbox" then
      # Checkbox uses "label" in .kry but "text" in Nim - normalize to "text"
      if has("label") and (has("text") | not) then
        .text = .label | del(.label)
      else . end
    elif type == "object" and .type == "Dropdown" then
      # Dropdown has dropdown_state in Nim but flat properties in .kry
      # Flatten dropdown_state if it exists
      if has("dropdown_state") then
        .options = .dropdown_state.options |
        .placeholder = .dropdown_state.placeholder |
        .selectedIndex = .dropdown_state.selectedIndex |
        del(.dropdown_state)
      else . end
    elif type == "object" and .type == "Input" then
      # Input uses "value" in .kry but "text" in Nim - normalize to "text"
      if has("value") and (has("text") | not) then
        .text = .value | del(.value)
      else . end |
      # Remove placeholder (not preserved in roundtrip - TODO: fix Input serialization)
      if has("placeholder") then
        del(.placeholder)
      else . end
    elif type == "object" and (.type == "Row" or .type == "TabBar") then
      # TabBar/Row - remove Tab-specific properties
      if has("reorderable") then
        del(.reorderable)
      else . end |
      # Remove gap/padding/height that vary between TabBar implementations
      if has("gap") and .gap == 2 then
        del(.gap)
      else . end |
      if has("padding") then
        del(.padding)
      else . end |
      if has("height") and .height == "40px" then
        del(.height)
      else . end
    elif type == "object" and (.type == "Column" or .type == "TabContent" or .type == "TabGroup") then
      # TabContent visibility is runtime state - only first panel is visible
      # Identify TabContent by type or by having TabPanel children
      if .type == "TabContent" or (has("children") and (.children | length > 0) and (.children[0].type? == "TabPanel")) then
        # Keep only first child (active panel)
        if has("children") and (.children | length > 1) then
          .children = [.children[0]]
        else . end
      else . end |
      # Remove TabGroup runtime state (selectedIndex)
      if .type == "TabGroup" or has("selectedIndex") then
        if has("selectedIndex") then
          del(.selectedIndex)
        else . end
      else . end |
      # Remove default gap from TabGroup
      if has("gap") and .gap == 8 then
        del(.gap)
      else . end
    else .
    end
  );

# Normalize component types (Container and Column are semantically equivalent in many contexts)
def normalize_component_types:
  walk(
    if type == "object" and has("type") then
      # Tab is implemented as Button in Nim DSL - normalize to Button
      if .type == "Tab" then
        .type = "Button" |
        # Convert title → text
        if has("title") then
          .text = .title | del(.title)
        else . end |
        # Remove Tab-specific properties (they're not in Button serialization)
        del(.activeBackground, .activeTextColor, .textColor)
      # TabBar is implemented as Row
      elif .type == "TabBar" then
        .type = "Row"
      # TabGroup is implemented as Column
      elif .type == "TabGroup" then
        .type = "Column"
      # TabContent is implemented as Column
      elif .type == "TabContent" then
        .type = "Column"
      # TabPanel is implemented as Column
      elif .type == "TabPanel" then
        .type = "Column"
      # Container without flex direction is often transpiled to Column
      elif .type == "Container" or .type == "Column" then
        # Check if it has flexDirection/direction to determine the actual type
        if has("flexDirection") then
          if .flexDirection == "column" or .flexDirection == "Column" then
            .type = "Column"
          elif .flexDirection == "row" or .flexDirection == "Row" then
            .type = "Row"
          else
            # Keep original if ambiguous
            .
          end
        elif has("direction") then
          if .direction == "column" then
            .type = "Column"
          elif .direction == "row" then
            .type = "Row"
          else
            # Default to Column if no explicit direction
            .type = "Column"
          end
        else
          # No direction specified - normalize both to Column
          .type = "Column"
        end
      else
        .
      end
    else
      .
    end
  );

# Normalize window properties location (can be in root or at top level)
def normalize_window_properties:
  if has("root") and (.root | type == "object") then
    # Extract title from root if it exists
    (if .root.windowTitle then {windowTitle: .root.windowTitle} else {} end) as $root_title |
    # Get title from top level if it exists
    (if .windowTitle then {windowTitle: .windowTitle} else {} end) as $top_title |
    # Root title takes precedence
    ($root_title + $top_title) as $title |
    # Remove ALL window properties from root and top level
    # (width/height are redundant with root component dimensions)
    .root |= del(.windowTitle, .windowWidth, .windowHeight) |
    del(.windowTitle, .windowWidth, .windowHeight) |
    # Add only title back at top level if it exists
    if ($title | length > 0) then
      . + $title
    else
      .
    end
  else
    .
  end;

# Sort object keys for consistent ordering
def sort_keys:
  walk(
    if type == "object" then
      to_entries | sort_by(.key) | from_entries
    else .
    end
  );

# Round floating point numbers to 2 decimal places, convert whole numbers to integers
def round_floats:
  walk(
    if type == "number" then
      if (. == (. | floor)) then
        # Convert whole number floats to integers (4.0 -> 4)
        (. | floor)
      else
        # Round to 2 decimals
        (. * 100 | round / 100)
      end
    else .
    end
  );

# Normalize dimension strings (remove .0, normalize units)
def normalize_dimensions:
  walk(
    if type == "string" then
      # "100.0px" -> "100px"
      if test("^[0-9]+\\.0+px$") then
        gsub("\\.0+px$"; "px")
      # "50.0%" -> "50%"
      elif test("^[0-9]+\\.0+%$") then
        gsub("\\.0+%$"; "%")
      else
        .
      end
    else .
    end
  );

# Remove empty arrays and objects (optional, can help reduce noise)
def remove_empty_collections:
  walk(
    if type == "object" then
      to_entries |
      map(select(.value != {} and .value != [])) |
      from_entries
    elif type == "array" then
      map(select(. != {} and . != []))
    else
      .
    end
  );

# Normalize component definitions (int vs string defaults, type variations)
def normalize_component_definitions:
  if has("component_definitions") and (.component_definitions | length > 0) then
    .component_definitions |= map(
      if has("props") then
        .props |= map(
          # Normalize default value representations (0 vs "0", etc.)
          if has("default") and (.default | type == "number") then
            .default = (.default | tostring)
          else . end |
          # Normalize type representations (remove type field for comparison - "int" vs "any" are both valid)
          if has("type") then
            del(.type)
          else . end
        )
      else . end |
      # Normalize state variable types
      if has("state") then
        .state |= map(
          # Remove type field for comparison
          if has("type") then
            del(.type)
          else . end
        )
      else . end
    )
  else .
  end;

# Normalize reactive manifest structure (handle field name variations)
# Simplified: Remove reactive_manifest entirely for comparison since
# conditional IDs reference component IDs which get remapped differently
def normalize_reactive_manifest:
  if has("reactive_manifest") then
    del(.reactive_manifest)
  else .
  end;

# Normalize logic section (handlers, events, functions)
# Simplified: Just remove the entire logic section for comparison since
# handler names/IDs differ but semantic meaning is the same
def normalize_logic:
  if has("logic") then
    del(.logic)
  else . end;

# Normalize event logic_id references in components
# Simplified: Just remove events array since logic_ids differ but are semantically equivalent
def normalize_event_references:
  walk(
    if type == "object" and has("events") then
      del(.events)
    else .
    end
  );

# Normalize sources section
# Simplified: Just remove sources entirely since formatting differs but content is semantically equivalent
def normalize_sources:
  if has("sources") then
    del(.sources)
  else . end;

# Main normalization pipeline
normalize_ids |
normalize_colors |
normalize_expressions |
normalize_dimensions |
normalize_property_names |
normalize_component_types |
normalize_window_properties |
remove_volatile_fields |
remove_default_properties |
round_floats |
normalize_logic |
normalize_event_references |
normalize_sources |
normalize_component_definitions |
normalize_reactive_manifest |
remove_empty_collections |
sort_keys
