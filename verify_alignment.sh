#!/bin/bash

# Test alignment generation

echo "Testing Column with center alignment..."
cat > /tmp/test_col_center.kir << 'EOF'
{
  "format": "kir",
  "metadata": {
    "source_language": "kry",
    "compiler_version": "kryon-1.0.0"
  },
  "app": {
    "windowTitle": "Test",
    "windowWidth": 800,
    "windowHeight": 600
  },
  "root": {
    "type": "Column",
    "justifyContent": "center",
    "alignItems": "center",
    "children": [
      {
        "type": "Container",
        "width": "100px",
        "height": "50px",
        "background": "#ff0000"
      }
    ]
  }
}
EOF

echo "Testing Column with end alignment..."
cat > /tmp/test_col_end.kir << 'EOF'
{
  "format": "kir",
  "metadata": {
    "source_language": "kry",
    "compiler_version": "kryon-1.0.0"
  },
  "app": {
    "windowTitle": "Test",
    "windowWidth": 800,
    "windowHeight": 600
  },
  "root": {
    "type": "Column",
    "justifyContent": "end",
    "alignItems": "end",
    "children": [
      {
        "type": "Container",
        "width": "100px",
        "height": "50px",
        "background": "#ff0000"
      }
    ]
  }
}
EOF

echo "Testing Row with center alignment..."
cat > /tmp/test_row_center.kir << 'EOF'
{
  "format": "kir",
  "metadata": {
    "source_language": "kry",
    "compiler_version": "kryon-1.0.0"
  },
  "app": {
    "windowTitle": "Test",
    "windowWidth": 800,
    "windowHeight": 600
  },
  "root": {
    "type": "Row",
    "justifyContent": "center",
    "alignItems": "center",
    "children": [
      {
        "type": "Container",
        "width": "50px",
        "height": "100px",
        "background": "#ff0000"
      }
    ]
  }
}
EOF

echo "Test files created in /tmp/"
