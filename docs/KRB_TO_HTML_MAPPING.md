# KRB to HTML Mapping Reference v2.0
## Smart Hybrid System Mapping

## Overview
This document describes how the Smart Hybrid System (CSS-like styling + Widget layout) maps to HTML, CSS, and JavaScript for web deployment.

## Style System Mapping

### Style Definitions to CSS

KRY style definitions map directly to CSS classes:

```kry
style "primaryButton" {
    background: "#007AFF"
    color: "#ffffff"
    fontSize: 16
    fontWeight: 600
    borderRadius: 6
    padding: "12px 24px"
    border: "none"
    cursor: "pointer"
    transition: "all 0.2s ease"
}
```

Maps to:

```css
.kryon-style-primaryButton {
    background-color: #007AFF;
    color: #ffffff;
    font-size: 16px;
    font-weight: 600;
    border-radius: 6px;
    padding: 12px 24px;
    border: none;
    cursor: pointer;
    transition: all 0.2s ease;
}
```

### Style Inheritance Mapping

```kry
style "button" {
    fontSize: 16
    padding: "8px 16px"
    borderRadius: 6
    border: "none"
    cursor: "pointer"
}

style "primaryButton" extends "button" {
    background: "#007AFF"
    color: "#ffffff"
}
```

Maps to:

```css
.kryon-style-button {
    font-size: 16px;
    padding: 8px 16px;
    border-radius: 6px;
    border: none;
    cursor: pointer;
}

.kryon-style-primaryButton {
    /* Inherits from button */
    font-size: 16px;
    padding: 8px 16px;
    border-radius: 6px;
    border: none;
    cursor: pointer;
    /* Override properties */
    background-color: #007AFF;
    color: #ffffff;
}
```

## Theme Variable Mapping

### Theme Variables to CSS Custom Properties

```kry
@theme colors {
    primary: "#007AFF"
    secondary: "#34C759"
    background: "#ffffff"
    text: "#000000"
}

@theme spacing {
    sm: 8
    md: 16
    lg: 24
}
```

Maps to:

```css
:root {
    --kryon-colors-primary: #007AFF;
    --kryon-colors-secondary: #34C759;
    --kryon-colors-background: #ffffff;
    --kryon-colors-text: #000000;
    --kryon-spacing-sm: 8px;
    --kryon-spacing-md: 16px;
    --kryon-spacing-lg: 24px;
}
```

### Theme Variable Usage

```kry
style "card" {
    background: $colors.background
    color: $colors.text
    padding: $spacing.md
}
```

Maps to:

```css
.kryon-style-card {
    background-color: var(--kryon-colors-background);
    color: var(--kryon-colors-text);
    padding: var(--kryon-spacing-md);
}
```

### Theme Switching

```kry
@theme light {
    background: "#ffffff"
    text: "#000000"
}

@theme dark {
    background: "#000000"
    text: "#ffffff"
}
```

Maps to:

```css
:root {
    --kryon-background: #ffffff;
    --kryon-text: #000000;
}

[data-theme="dark"] {
    --kryon-background: #000000;
    --kryon-text: #ffffff;
}
```

## Widget Layout Mapping

### Layout Widgets to HTML + CSS

| KRY Widget | HTML Element | CSS Classes | CSS Properties |
|------------|--------------|-------------|----------------|
| Column | `<div>` | `kryon-column` | `display: flex; flex-direction: column` |
| Row | `<div>` | `kryon-row` | `display: flex; flex-direction: row` |
| Center | `<div>` | `kryon-center` | `display: flex; align-items: center; justify-content: center` |
| Container | `<div>` | `kryon-container` | `display: block` |
| Flex | `<div>` | `kryon-flex` | `display: flex` with dynamic properties |
| Spacer | `<div>` | `kryon-spacer` | `flex: 1; min-width: 0; min-height: 0` |

### Content Widgets to HTML

| KRY Widget | HTML Element | CSS Classes | Notes |
|------------|--------------|-------------|-------|
| Text | `<span>` or `<p>` | `kryon-text` | Based on context |
| Button | `<button>` | `kryon-button` | Interactive button |
| Image | `<img>` | `kryon-image` | Image element |
| Input | `<input>` | `kryon-input` | Various input types |

## Complete Mapping Example

### KRY Source

```kry
@theme colors {
    primary: "#007AFF"
    background: "#ffffff"
    text: "#000000"
}

@theme spacing {
    sm: 8
    md: 16
}

style "card" {
    background: $colors.background
    borderRadius: 8
    padding: $spacing.md
    boxShadow: "0 2px 4px rgba(0,0,0,0.1)"
}

style "primaryButton" {
    background: $colors.primary
    color: $colors.background
    fontSize: 16
    padding: "$spacing.sm $spacing.md"
    borderRadius: 6
    border: "none"
}

App {
    windowWidth: 800
    windowHeight: 600
    
    Center {
        child: Container {
            style: "card"
            width: 400
            
            Column {
                spacing: $spacing.md
                
                Text {
                    text: "Hello, World!"
                    fontSize: 24
                    color: $colors.text
                }
                
                Button {
                    text: "Click Me"
                    style: "primaryButton"
                    onClick: "handleClick"
                }
            }
        }
    }
}
```

### Generated HTML

```html
<!DOCTYPE html>
<html>
<head>
    <style>
        :root {
            --kryon-colors-primary: #007AFF;
            --kryon-colors-background: #ffffff;
            --kryon-colors-text: #000000;
            --kryon-spacing-sm: 8px;
            --kryon-spacing-md: 16px;
        }
        
        .kryon-app {
            width: 800px;
            height: 600px;
            margin: 0 auto;
        }
        
        .kryon-center {
            display: flex;
            align-items: center;
            justify-content: center;
            width: 100%;
            height: 100%;
        }
        
        .kryon-container {
            display: block;
        }
        
        .kryon-column {
            display: flex;
            flex-direction: column;
        }
        
        .kryon-column[data-spacing="16"] > * + * {
            margin-top: 16px;
        }
        
        .kryon-text {
            margin: 0;
        }
        
        .kryon-button {
            border: none;
            cursor: pointer;
            background: transparent;
        }
        
        .kryon-style-card {
            background-color: var(--kryon-colors-background);
            border-radius: 8px;
            padding: var(--kryon-spacing-md);
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        
        .kryon-style-primaryButton {
            background-color: var(--kryon-colors-primary);
            color: var(--kryon-colors-background);
            font-size: 16px;
            padding: var(--kryon-spacing-sm) var(--kryon-spacing-md);
            border-radius: 6px;
            border: none;
        }
    </style>
</head>
<body>
    <div class="kryon-app">
        <div class="kryon-center">
            <div class="kryon-container kryon-style-card" style="width: 400px;">
                <div class="kryon-column" data-spacing="16">
                    <p class="kryon-text" style="font-size: 24px; color: var(--kryon-colors-text);">
                        Hello, World!
                    </p>
                    <button class="kryon-button kryon-style-primaryButton" onclick="handleClick()">
                        Click Me
                    </button>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
```

## Widget Property Mapping

### Layout Widget Properties

#### Column Widget

```kry
Column {
    spacing: 16
    mainAxis: "center"
    crossAxis: "stretch"
}
```

Maps to:

```css
.kryon-column {
    display: flex;
    flex-direction: column;
    justify-content: center;  /* mainAxis */
    align-items: stretch;     /* crossAxis */
}

.kryon-column[data-spacing="16"] > * + * {
    margin-top: 16px;
}
```

#### Row Widget

```kry
Row {
    spacing: 8
    mainAxis: "spaceBetween"
    crossAxis: "center"
}
```

Maps to:

```css
.kryon-row {
    display: flex;
    flex-direction: row;
    justify-content: space-between;  /* mainAxis */
    align-items: center;            /* crossAxis */
    gap: 8px;                       /* spacing */
}
```

#### Flex Widget

```kry
Flex {
    direction: "row"
    align: "center"
    justify: "spaceBetween"
    gap: 12
}
```

Maps to:

```css
.kryon-flex {
    display: flex;
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
}
```

## Responsive Property Mapping

### Responsive Values

```kry
Container {
    width: {
        mobile: "100%"
        tablet: 600
        desktop: 800
    }
    padding: {
        mobile: $spacing.sm
        tablet: $spacing.md
        desktop: $spacing.lg
    }
}
```

Maps to:

```css
.kryon-container {
    /* Mobile (default) */
    width: 100%;
    padding: var(--kryon-spacing-sm);
}

@media (min-width: 768px) {
    .kryon-container {
        /* Tablet */
        width: 600px;
        padding: var(--kryon-spacing-md);
    }
}

@media (min-width: 1024px) {
    .kryon-container {
        /* Desktop */
        width: 800px;
        padding: var(--kryon-spacing-lg);
    }
}
```

## Theme Switching JavaScript

### Theme Manager

```javascript
class KryonThemeManager {
    constructor() {
        this.currentTheme = 'light';
        this.themes = new Map();
    }
    
    registerTheme(name, variables) {
        this.themes.set(name, variables);
    }
    
    setTheme(themeName) {
        const theme = this.themes.get(themeName);
        if (!theme) return;
        
        const root = document.documentElement;
        
        // Apply theme variables
        Object.entries(theme).forEach(([key, value]) => {
            root.style.setProperty(`--kryon-${key}`, value);
        });
        
        // Set theme attribute
        root.setAttribute('data-theme', themeName);
        this.currentTheme = themeName;
    }
}

// Initialize theme manager
const themeManager = new KryonThemeManager();

// Register themes
themeManager.registerTheme('light', {
    background: '#ffffff',
    text: '#000000',
    primary: '#007AFF'
});

themeManager.registerTheme('dark', {
    background: '#000000',
    text: '#ffffff',
    primary: '#0A84FF'
});

// Auto-detect system theme
const prefersDark = window.matchMedia('(prefers-color-scheme: dark)');
themeManager.setTheme(prefersDark.matches ? 'dark' : 'light');

// Listen for system theme changes
prefersDark.addEventListener('change', (e) => {
    themeManager.setTheme(e.matches ? 'dark' : 'light');
});
```

## Event Handling

### Event Mapping

```kry
Button {
    onClick: "handleClick"
    onMouseEnter: "handleHover"
}

Input {
    onChange: "handleChange"
    onFocus: "handleFocus"
}
```

Maps to:

```html
<button onclick="handleClick()" onmouseenter="handleHover()">
    Click Me
</button>

<input onchange="handleChange(event)" onfocus="handleFocus(event)">
```

## Performance Optimizations

### Style Deduplication

Identical styles are automatically deduplicated:

```kry
style "button1" { background: "#007AFF" }
style "button2" { background: "#007AFF" }
```

Results in single CSS rule:

```css
.kryon-style-button1,
.kryon-style-button2 {
    background-color: #007AFF;
}
```

### CSS Custom Property Optimization

Theme variables are optimized for minimal CSS custom property usage:

```css
:root {
    /* Only variables that are actually used */
    --kryon-colors-primary: #007AFF;
    --kryon-spacing-md: 16px;
}
```

### Widget CSS Classes

Base widget classes provide consistent styling and performance:

```css
.kryon-column,
.kryon-row,
.kryon-flex {
    box-sizing: border-box;
}

.kryon-text {
    margin: 0;
    padding: 0;
}

.kryon-button {
    border: none;
    background: transparent;
    cursor: pointer;
}
```

This mapping system provides clean, efficient HTML/CSS output that preserves the hybrid nature of CSS-like styling with widget-based layout structure.