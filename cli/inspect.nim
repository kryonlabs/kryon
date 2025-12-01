## Kryon IR Inspection Tools
##
## Advanced debugging and analysis for IR files

import os, strutils, json, tables, sets, algorithm, sequtils
import ../bindings/nim/ir_core
import ../bindings/nim/ir_serialization

type
  InspectionReport* = object
    totalComponents*: int
    componentsByType*: Table[string, int]
    maxDepth*: int
    totalTextLength*: int
    componentsWithText*: int
    componentsWithEvents*: int
    componentsWithStyle*: int
    reactiveVars*: int
    reactiveBindings*: int
    warnings*: seq[string]
    componentTree*: JsonNode

  TreeStats = object
    depth: int
    count: int

proc analyzeComponent(comp: ptr IRComponent, depth: int, report: var InspectionReport, path: string): JsonNode =
  ## Recursively analyze a component and build inspection report
  result = newJObject()

  # Update stats
  report.totalComponents += 1
  if depth > report.maxDepth:
    report.maxDepth = depth

  # Component type
  let typeName = $comp.`type`
  result["type"] = %typeName
  result["id"] = %(comp.id)
  result["depth"] = %depth
  result["path"] = %path

  # Track type counts
  if not report.componentsByType.hasKey(typeName):
    report.componentsByType[typeName] = 0
  report.componentsByType[typeName] += 1

  # Text content analysis
  if comp.text_content != nil:
    let text = $comp.text_content
    if text.len > 0:
      report.componentsWithText += 1
      report.totalTextLength += text.len
      result["text_length"] = %text.len

      if text.len > 1000:
        report.warnings.add("Component at " & path & " has very long text (" & $text.len & " chars)")

      # Sample text for display
      let sample = if text.len > 100: text[0..99] & "..." else: text
      result["text_sample"] = %sample

  # Style analysis
  if comp.style != nil:
    report.componentsWithStyle += 1
    var styleInfo = newJObject()

    # Background color
    if comp.style.background.`type` != IR_COLOR_TRANSPARENT:
      let bg = comp.style.background
      styleInfo["background"] = %(
        "rgba(" & $bg.data.r & "," & $bg.data.g & "," & $bg.data.b & "," & $bg.data.a & ")"
      )

    # Dimensions
    if comp.style.width.`type` != IR_DIMENSION_AUTO:
      styleInfo["width"] = %($comp.style.width.value & " " & $comp.style.width.`type`)

    if comp.style.height.`type` != IR_DIMENSION_AUTO:
      styleInfo["height"] = %($comp.style.height.value & " " & $comp.style.height.`type`)

    # Padding/margin
    let padding = comp.style.padding
    if padding.top != 0 or padding.right != 0 or padding.bottom != 0 or padding.left != 0:
      styleInfo["padding"] = %("t:" & $padding.top & " r:" & $padding.right &
                               " b:" & $padding.bottom & " l:" & $padding.left)

    let margin = comp.style.margin
    if margin.top != 0 or margin.right != 0 or margin.bottom != 0 or margin.left != 0:
      styleInfo["margin"] = %("t:" & $margin.top & " r:" & $margin.right &
                              " b:" & $margin.bottom & " l:" & $margin.left)

    # Font info
    if comp.style.font.size > 0:
      styleInfo["font_size"] = %comp.style.font.size

    result["style"] = styleInfo

  # Event analysis
  if comp.events != nil:
    report.componentsWithEvents += 1
    result["has_events"] = %true

  # Children analysis
  result["child_count"] = %(comp.child_count)

  if comp.child_count > 0:
    var children = newJArray()
    let childArray = cast[ptr UncheckedArray[ptr IRComponent]](comp.children)

    # Warn about excessive children
    if comp.child_count > 100:
      report.warnings.add("Component at " & path & " has many children (" & $comp.child_count & ")")

    for i in 0..<comp.child_count:
      let childPath = path & "/" & $i
      children.add(analyzeComponent(childArray[i], depth + 1, report, childPath))

    result["children"] = children

  return result

proc inspectIRFile*(irFile: string): InspectionReport =
  ## Inspect an IR file and generate detailed report
  result.componentsByType = initTable[string, int]()
  result.warnings = @[]

  # Load IR file
  if not fileExists(irFile):
    result.warnings.add("IR file not found: " & irFile)
    return

  let buffer = ir_buffer_create_from_file(cstring(irFile))
  if buffer == nil:
    result.warnings.add("Failed to read IR file")
    return

  # Deserialize with manifest
  var manifest: ptr IRReactiveManifest = nil
  let root = ir_deserialize_binary_with_manifest(buffer, addr manifest)
  ir_buffer_destroy(buffer)

  if root == nil:
    result.warnings.add("Failed to deserialize IR")
    return

  # Analyze component tree
  result.componentTree = analyzeComponent(root, 0, result, "root")

  # Analyze reactive manifest
  if manifest != nil:
    result.reactiveVars = int(manifest.variable_count)
    result.reactiveBindings = int(manifest.binding_count)

    # Warn about excessive reactive state
    if manifest.variable_count > 100:
      result.warnings.add("High number of reactive variables: " & $manifest.variable_count)

    ir_reactive_manifest_destroy(manifest)

  # Cleanup
  ir_destroy_component(root)

proc printTreeVisual(tree: JsonNode, prefix: string = "", isLast: bool = true, maxDepth: int = 10, currentDepth: int = 0) =
  ## Print visual tree representation
  if currentDepth >= maxDepth:
    echo prefix & "└── (max depth reached...)"
    return

  let connector = if isLast: "└── " else: "├── "
  let typeName = tree["type"].getStr()
  var label = typeName

  # Add ID
  if tree.hasKey("id"):
    label &= " #" & $tree["id"].getInt()

  # Add text sample
  if tree.hasKey("text_sample"):
    label &= " \"" & tree["text_sample"].getStr() & "\""

  # Add child count if has children
  if tree.hasKey("child_count"):
    let count = tree["child_count"].getInt()
    if count > 0:
      label &= " (" & $count & " children)"

  echo prefix & connector & label

  # Recurse into children
  if tree.hasKey("children"):
    let children = tree["children"].getElems()
    let newPrefix = prefix & (if isLast: "    " else: "│   ")

    for i, child in children:
      let isLastChild = i == children.len - 1
      printTreeVisual(child, newPrefix, isLastChild, maxDepth, currentDepth + 1)

proc handleInspectDetailedCommand*(args: seq[string]) =
  ## Handle 'kryon inspect-detailed' command
  if args.len == 0:
    echo "Error: IR file required"
    echo "Usage: kryon inspect-detailed <file.kir> [--json] [--tree] [--max-depth=N]"
    return

  let irFile = args[0]
  var jsonOutput = false
  var treeVisual = false
  var maxDepth = 10

  # Parse options
  for arg in args[1..^1]:
    if arg == "--json":
      jsonOutput = true
    elif arg == "--tree":
      treeVisual = true
    elif arg.startsWith("--max-depth="):
      maxDepth = parseInt(arg[12..^1])

  echo "🔍 Inspecting IR file: ", irFile
  echo ""

  let report = inspectIRFile(irFile)

  if jsonOutput:
    # JSON output
    var output = newJObject()
    output["totalComponents"] = %report.totalComponents
    output["maxDepth"] = %report.maxDepth
    output["componentsWithText"] = %report.componentsWithText
    output["componentsWithStyle"] = %report.componentsWithStyle
    output["componentsWithEvents"] = %report.componentsWithEvents
    output["reactiveVars"] = %report.reactiveVars
    output["reactiveBindings"] = %report.reactiveBindings
    output["warnings"] = %report.warnings

    var typeTable = newJObject()
    for typeName, count in report.componentsByType.pairs:
      typeTable[typeName] = %count
    output["componentsByType"] = typeTable

    output["componentTree"] = report.componentTree

    echo output.pretty()
  else:
    # Human-readable output
    if report.warnings.len > 0:
      echo "⚠️  Warnings:"
      for warning in report.warnings:
        echo "   " & warning
      echo ""

    echo "📊 Statistics:"
    echo "   Total components: ", report.totalComponents
    echo "   Max tree depth: ", report.maxDepth
    echo "   Components with text: ", report.componentsWithText
    echo "   Components with style: ", report.componentsWithStyle
    echo "   Components with events: ", report.componentsWithEvents

    if report.componentsWithText > 0:
      let avgTextLen = report.totalTextLength div report.componentsWithText
      echo "   Average text length: ", avgTextLen, " chars"

    if report.reactiveVars > 0 or report.reactiveBindings > 0:
      echo ""
      echo "🔄 Reactive State:"
      echo "   Variables: ", report.reactiveVars
      echo "   Bindings: ", report.reactiveBindings

    echo ""
    echo "📦 Component Types:"
    for typeName in sorted(toSeq(report.componentsByType.keys())):
      let count = report.componentsByType[typeName]
      let percentage = (count * 100) div report.totalComponents
      echo "   " & typeName & ": " & $count & " (" & $percentage & "%)"

    if treeVisual and not report.componentTree.isNil:
      echo ""
      echo "🌳 Component Tree:"
      printTreeVisual(report.componentTree, "", true, maxDepth)

proc handleTreeCommand*(args: seq[string]) =
  ## Handle 'kryon tree' command - quick tree visualization
  if args.len == 0:
    echo "Error: IR file required"
    echo "Usage: kryon tree <file.kir> [--max-depth=N]"
    return

  let irFile = args[0]
  var maxDepth = 20

  # Parse options
  for arg in args[1..^1]:
    if arg.startsWith("--max-depth="):
      maxDepth = parseInt(arg[12..^1])

  let report = inspectIRFile(irFile)

  if report.warnings.len > 0:
    for warning in report.warnings:
      echo "⚠️  " & warning
    if report.componentTree.isNil:
      return

  echo "🌳 Component Tree: ", irFile
  echo ""
  printTreeVisual(report.componentTree, "", true, maxDepth)
  echo ""
  echo "Total: ", report.totalComponents, " components"
