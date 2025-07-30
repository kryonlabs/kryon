# Kryon Dropdown Widget Example

This example demonstrates the comprehensive dropdown widget functionality in Kryon-C.

## Features Demonstrated

### 1. Basic Single-Select Dropdown
- Standard dropdown with multiple options
- Event handling for selection changes
- Dynamic text updates based on selection

### 2. Multi-Select Dropdown
- Multiple item selection support
- Maximum height constraint with scrolling
- Display of multiple selected values

### 3. Styling and States
- Custom styling with CSS-like properties
- Hover and focus states
- Disabled state demonstration

### 4. HTML Mapping
- Automatic conversion to HTML `<select>` elements
- Proper `<option>` generation with values
- Event binding for web compatibility

## Running the Example

```bash
# Compile the KRY file to KRB
./tools/run examples/dropdown/simple-dropdown.kry

# Or build and run manually
kryon compile examples/dropdown/simple-dropdown.kry -o dropdown-demo.krb
kryon run dropdown-demo.krb --renderer sdl2
```

## Dropdown Syntax

### Basic Dropdown
```kry
Dropdown {
    id: "my_dropdown"
    selectedIndex: 0
    onChange: "handleChange"
    
    options: [
        { text: "Option 1", value: "opt1" },
        { text: "Option 2", value: "opt2" },
        { text: "Option 3", value: "opt3" }
    ]
}
```

### Multi-Select Dropdown
```kry
Dropdown {
    id: "multi_dropdown"
    multiSelect: true
    maxHeight: 150
    selectedIndices: [0, 2]
    onChange: "handleMultiChange"
    
    options: [
        { text: "Item A", value: "a" },
        { text: "Item B", value: "b" },
        { text: "Item C", value: "c" }
    ]
}
```

### Styling
```kry
@style "dropdown" {
    width: 200
    height: 35
    backgroundColor: "#ffffff"
    borderColor: "#cccccc"
    borderWidth: 1
    borderRadius: 6
    fontSize: 14
    padding: 8
}
```

## HTML Output

The dropdown automatically generates proper HTML when targeting web platforms:

```html
<select id="my_dropdown" class="kryon-dropdown" onchange="handleDropdownChange(this)">
    <option value="opt1">Option 1</option>
    <option value="opt2">Option 2</option>
    <option value="opt3" selected>Option 3</option>
</select>
```

## Event Handling

Dropdown events are automatically handled and can trigger custom JavaScript functions:

```javascript
function handleColorChange(dropdown, selectedIndex, selectedValue) {
    console.log('Selected:', selectedValue);
    // Update UI based on selection
}
```

## Platform Support

- **Desktop**: Native dropdown rendering with SDL2/Raylib
- **Web**: HTML `<select>` elements with CSS styling
- **Mobile**: Platform-native picker controls
- **Terminal**: Text-based selection interface

The dropdown widget provides a consistent API across all platforms while adapting to platform-specific UI conventions.