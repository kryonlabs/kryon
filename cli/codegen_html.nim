## HTML Code Generation with JavaScript Support
##
## Generates vanilla HTML with inline JavaScript by leveraging existing TSX/JSX codegen logic

import json, strformat, strutils
import codegen_react_common  # Reuse generateHandlerBody, generateReactExpression

proc toKebabCase(s: string): string =
  ## Convert PascalCase to kebab-case
  ## "CodeBlock" → "code-block"
  result = ""
  for i, c in s:
    if c.isUpperAscii() and i > 0:
      result.add('-')
    result.add(c.toLowerAscii())

proc toScreamingSnakeCase(s: string): string =
  ## Convert PascalCase to SCREAMING_SNAKE_CASE
  ## "CodeBlock" → "CODE_BLOCK"
  result = ""
  for i, c in s:
    if c.isUpperAscii() and i > 0:
      result.add('_')
    result.add(c.toUpperAscii())

proc formatDimension(val: JsonNode): string =
  ## Format a dimension value to px without unnecessary decimals
  if val.kind == JInt:
    return $val.getInt() & "px"
  elif val.kind == JFloat:
    let f = val.getFloat()
    if f == f.int.float:  # Check if it's a whole number
      return $f.int & "px"
    else:
      return &"{f:.2f}px"
  else:
    return "0px"

proc generateInlineStyles(node: JsonNode): string =
  ## Generate inline CSS from KIR node properties
  var styles: seq[string] = @[]

  # Dimensions
  if node.hasKey("width"):
    let w = node["width"]
    if w.kind == JFloat:
      styles.add(&"width: {w.getFloat():.2f}px")
    elif w.kind == JInt:
      styles.add(&"width: {w.getInt()}px")

  if node.hasKey("height"):
    let h = node["height"]
    if h.kind == JFloat:
      styles.add(&"height: {h.getFloat():.2f}px")
    elif h.kind == JInt:
      styles.add(&"height: {h.getInt()}px")

  # Background color (canonical KIR property: "background")
  if node.hasKey("background"):
    let color = node["background"]
    if color.kind == JString:
      let colorStr = color.getStr()
      if colorStr != "transparent" and colorStr != "":
        styles.add(&"background-color: {colorStr}")
    elif color.kind == JObject:
      let r = color{"r"}.getInt(0)
      let g = color{"g"}.getInt(0)
      let b = color{"b"}.getInt(0)
      let a = color{"a"}.getFloat(1.0)
      styles.add(&"background-color: rgba({r}, {g}, {b}, {a:.2f})")

  if node.hasKey("color"):
    let color = node["color"]
    if color.kind == JString:
      styles.add(&"color: {color.getStr()}")
    elif color.kind == JObject:
      let r = color{"r"}.getInt(0)
      let g = color{"g"}.getInt(0)
      let b = color{"b"}.getInt(0)
      let a = color{"a"}.getFloat(1.0)
      styles.add(&"color: rgba({r}, {g}, {b}, {a:.2f})")

  # Padding
  if node.hasKey("padding"):
    let p = node["padding"]
    if p.kind == JFloat or p.kind == JInt:
      styles.add(&"padding: {formatDimension(p)}")
    elif p.kind == JObject:
      let top = formatDimension(p{"top"})
      let right = formatDimension(p{"right"})
      let bottom = formatDimension(p{"bottom"})
      let left = formatDimension(p{"left"})
      styles.add(&"padding: {top} {right} {bottom} {left}")

  # Flexbox layout (map component type to flex-direction)
  if node.hasKey("children") and node["children"].len > 0:
    styles.add("display: flex")

    # Map component type to flex-direction (don't rely on direction field)
    let compType = node{"type"}.getStr("Container")
    if compType == "Row":
      styles.add("flex-direction: row")
    elif compType == "Column" or compType == "Container":
      styles.add("flex-direction: column")

    # Gap
    if node.hasKey("gap"):
      let gap = node["gap"]
      if gap.kind == JFloat or gap.kind == JInt:
        styles.add(&"gap: {gap.getInt()}px")

  # Typography
  if node.hasKey("fontSize"):
    let fs = node["fontSize"]
    if fs.kind == JFloat or fs.kind == JInt:
      styles.add(&"font-size: {formatDimension(fs)}")

  if node.hasKey("fontWeight"):
    let fw = node["fontWeight"]
    styles.add(&"font-weight: {fw.getInt()}")

  # Text alignment
  if node.hasKey("textAlign"):
    let align = node["textAlign"].getStr()
    if align != "":
      styles.add(&"text-align: {align}")

  # Margins (canonical property names)
  if node.hasKey("marginTop"):
    let mt = node["marginTop"]
    if mt.kind == JFloat or mt.kind == JInt:
      styles.add(&"margin-top: {formatDimension(mt)}")

  if node.hasKey("marginBottom"):
    let mb = node["marginBottom"]
    if mb.kind == JFloat or mb.kind == JInt:
      styles.add(&"margin-bottom: {formatDimension(mb)}")

  if node.hasKey("marginLeft"):
    let ml = node["marginLeft"]
    if ml.kind == JFloat or ml.kind == JInt:
      styles.add(&"margin-left: {formatDimension(ml)}")

  if node.hasKey("marginRight"):
    let mr = node["marginRight"]
    if mr.kind == JFloat or mr.kind == JInt:
      styles.add(&"margin-right: {formatDimension(mr)}")

  # Borders
  if node.hasKey("border"):
    let border = node["border"].getStr("")
    if border != "":
      styles.add(&"border: {border}")

  if node.hasKey("borderTop"):
    let borderTop = node["borderTop"].getStr("")
    if borderTop != "":
      styles.add(&"border-top: {borderTop}")

  if node.hasKey("borderBottom"):
    let borderBottom = node["borderBottom"].getStr("")
    if borderBottom != "":
      styles.add(&"border-bottom: {borderBottom}")

  if node.hasKey("borderRadius"):
    let br = node["borderRadius"]
    if br.kind == JFloat or br.kind == JInt:
      styles.add(&"border-radius: {formatDimension(br)}")

  # Flexbox alignment
  if node.hasKey("alignItems"):
    let align = node["alignItems"].getStr("")
    if align != "":
      styles.add(&"align-items: {align}")

  if node.hasKey("justifyContent"):
    let justify = node["justifyContent"].getStr("")
    if justify != "":
      styles.add(&"justify-content: {justify}")

  # Size constraints
  if node.hasKey("maxWidth"):
    let mw = node["maxWidth"]
    if mw.kind == JFloat or mw.kind == JInt:
      styles.add(&"max-width: {mw.getInt()}px")

  if node.hasKey("minWidth"):
    let mw = node["minWidth"]
    if mw.kind == JFloat or mw.kind == JInt:
      styles.add(&"min-width: {mw.getInt()}px")

  if node.hasKey("maxHeight"):
    let mh = node["maxHeight"]
    if mh.kind == JFloat or mh.kind == JInt:
      styles.add(&"max-height: {mh.getInt()}px")

  if node.hasKey("minHeight"):
    let mh = node["minHeight"]
    if mh.kind == JFloat or mh.kind == JInt:
      styles.add(&"min-height: {mh.getInt()}px")

  # Individual padding properties
  if node.hasKey("paddingTop"):
    let pt = node["paddingTop"]
    if pt.kind == JFloat or pt.kind == JInt:
      styles.add(&"padding-top: {formatDimension(pt)}")

  if node.hasKey("paddingBottom"):
    let pb = node["paddingBottom"]
    if pb.kind == JFloat or pb.kind == JInt:
      styles.add(&"padding-bottom: {formatDimension(pb)}")

  if node.hasKey("paddingLeft"):
    let pl = node["paddingLeft"]
    if pl.kind == JFloat or pl.kind == JInt:
      styles.add(&"padding-left: {formatDimension(pl)}")

  if node.hasKey("paddingRight"):
    let pr = node["paddingRight"]
    if pr.kind == JFloat or pr.kind == JInt:
      styles.add(&"padding-right: {formatDimension(pr)}")

  return styles.join("; ")

proc lookupComponentTemplate(compType: string, componentDefs: JsonNode): JsonNode =
  ## Look up component template by name in component_definitions array
  ## Returns the template node if found, otherwise nil
  if componentDefs == nil or componentDefs.kind != JArray:
    return nil

  for compDef in componentDefs:
    if compDef.hasKey("name") and compDef["name"].getStr() == compType:
      if compDef.hasKey("template"):
        return compDef["template"]

  return nil

proc generateVanillaHTMLElement(node: JsonNode, ctx: var ReactContext, indent: int, actualFuncName: string = ""): string =
  ## Generate HTML element from KIR node (adapted from generateReactElement)
  let indentStr = "  ".repeat(indent)
  let compType = node{"type"}.getStr("Container")

  # Check if this is a custom component reference (not a primitive)
  const primitiveTypes = [
    "Button", "Text", "TextInput", "Image",
    "Column", "Row", "Container", "Center",
    "TabGroup", "TabBar", "TabContent", "Tab", "TabPanel",
    "Table", "TableHead", "TableBody", "TableRow", "TableCell", "TableHeaderCell"
  ]

  if compType notin primitiveTypes:
    # Try to look up custom component template
    let templateNode = lookupComponentTemplate(compType, ctx.kirComponentDefs)
    if templateNode != nil:
      # Found custom component - inline its template instead
      return generateVanillaHTMLElement(templateNode, ctx, indent, actualFuncName)

  # Map component type to HTML tag
  let htmlTag = case compType:
    of "Button": "button"
    of "Text": "span"
    of "TextInput": "input"
    of "Image": "img"
    of "Link": "a"
    of "Column", "Row", "Container", "Center": "div"
    of "TabGroup", "TabBar", "TabContent", "Tab", "TabPanel": "div"
    of "Table": "table"
    of "TableHead": "thead"
    of "TableBody": "tbody"
    of "TableRow": "tr"
    of "TableCell": "td"
    of "TableHeaderCell": "th"
    else: "div"

  # Start tag
  result = &"{indentStr}<{htmlTag}"

  # Add ID attribute
  if node.hasKey("id"):
    let nodeId = node["id"].getInt()
    result &= &" id=\"kryon-{nodeId}\""

  # Add class attribute
  result &= &" class=\"kryon-{compType.toKebabCase()}\""

  # Add data-ir-type for debugging
  result &= &" data-ir-type=\"{compType.toScreamingSnakeCase()}\""

  # Add href attribute for links
  if htmlTag == "a" and node.hasKey("href"):
    let href = node["href"].getStr("#")
    result &= &" href=\"{href}\""

    # Add target attribute if present
    if node.hasKey("target"):
      let target = node["target"].getStr("_self")
      result &= &" target=\"{target}\""

  # Add inline styles
  let styleAttr = generateInlineStyles(node)
  if styleAttr != "":
    result &= &" style=\"{styleAttr}\""

  # Add event handlers as HTML attributes
  if node.hasKey("events"):
    for event in node["events"]:
      let eventType = event{"type"}.getStr()
      if eventType == "click":
        let logicId = event{"logic_id"}.getStr("")
        if logicId != "":
          # Try to use actual function name from sources.js
          if actualFuncName != "":
            result &= &" onclick=\"{actualFuncName}()\""
          elif ctx.logicFunctions.hasKey(logicId):
            # Try to use generateHandlerBody for inline handlers
            let handlerBody = generateHandlerBody(ctx.logicFunctions, logicId, "value")
            if handlerBody != "":
              result &= &" onclick=\"{handlerBody}\""
            else:
              # Fallback: use logic_id as function name
              result &= &" onclick=\"{logicId}()\""
          else:
            # Last resort: use logic_id as function name
            result &= &" onclick=\"{logicId}()\""

  # Handle component-specific attributes
  if compType == "Image":
    # For images, add src and alt attributes
    if node.hasKey("src"):
      result &= &" src=\"{node[\"src\"].getStr()}\""
    if node.hasKey("alt"):
      result &= &" alt=\"{node[\"alt\"].getStr()}\""
  elif node.hasKey("text"):
    # Add text data attribute (for button text and other components)
    let text = node["text"].getStr()
    result &= &" data-text=\"{text}\""

  result &= ">"

  # Text content (for Text components, Buttons, and Links)
  if node.hasKey("text"):
    let text = node["text"].getStr()
    if htmlTag in ["span", "button", "th", "td", "a"]:
      result &= &"\n{indentStr}  {text}\n{indentStr}"

  # Children
  if node.hasKey("children"):
    result &= "\n"
    for child in node["children"]:
      result &= generateVanillaHTMLElement(child, ctx, indent + 1, actualFuncName)

  # Close tag
  result &= &"</{htmlTag}>\n"

proc extractFunctionName(jsSource: string): string =
  ## Extract the first function name from JavaScript source
  ## Looks for pattern: "function functionName("
  if jsSource == "":
    return ""

  let funcStart = jsSource.find("function ")
  if funcStart == -1:
    return ""

  let nameStart = funcStart + 9  # Length of "function "
  let parenPos = jsSource.find("(", nameStart)
  if parenPos == -1:
    return ""

  result = jsSource[nameStart..<parenPos].strip()

proc generateVanillaHTML*(kirFile: string): string =
  ## Main entry point for HTML generation with JavaScript support
  let kirJson = parseFile(kirFile)

  # Extract logic functions and component definitions for HTML generation
  var ctx = ReactContext(
    logicFunctions: if kirJson.hasKey("logic") and kirJson["logic"].hasKey("functions"):
                      kirJson["logic"]["functions"]
                    else:
                      newJObject(),
    kirComponentDefs: if kirJson.hasKey("component_definitions"):
                        kirJson["component_definitions"]
                      else:
                        newJArray()  # Empty array if no definitions
  )

  # Extract standalone JavaScript from sources.js
  let jsSource = if kirJson.hasKey("sources") and kirJson["sources"].hasKey("js"):
                   kirJson["sources"]["js"].getStr()
                 else:
                   ""

  # Extract the actual function name from JS source
  let actualFunctionName = extractFunctionName(jsSource)

  # Generate HTML document structure
  result = "<!DOCTYPE html>\n"
  result &= "<html lang=\"en\">\n"
  result &= "<head>\n"
  result &= "  <meta charset=\"UTF-8\">\n"
  result &= "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
  result &= "  <title>Kryon Web Application</title>\n"

  # Emit standalone JavaScript functions from sources.js
  if jsSource != "":
    result &= "  <script>\n"
    # Add proper indentation to JavaScript source
    for line in jsSource.splitLines():
      if line.strip() != "":
        result &= &"    {line}\n"
    result &= "  </script>\n"

  result &= "</head>\n<body"

  # Add root background color to body
  if kirJson.hasKey("root"):
    let rootNode = kirJson["root"]
    if rootNode.hasKey("background"):
      let bg = rootNode["background"].getStr("#0D1117")
      result &= &" style=\"background-color: {bg}; color: #E6EDF3; margin: 0; padding: 0;\""

  result &= ">\n"

  # Generate body from root component
  # Handle two KIR formats:
  # 1. Standard format with "root" key (from .kry, .nim, .tsx)
  # 2. Direct component format (from markdown)
  if kirJson.hasKey("root"):
    result &= generateVanillaHTMLElement(kirJson["root"], ctx, 1, actualFunctionName)
  elif kirJson.hasKey("type"):
    # Direct component at root (markdown format)
    result &= generateVanillaHTMLElement(kirJson, ctx, 1, actualFunctionName)

  result &= "</body>\n</html>\n"
