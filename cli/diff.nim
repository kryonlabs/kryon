## Kryon IR Diff Tool
##
## Compare two IR files and show differences

import os, strutils, json, tables
import ../bindings/nim/ir_core
import ../bindings/nim/ir_serialization

type
  DiffResult* = object
    same*: bool
    differences*: seq[string]
    file1Stats*: JsonNode
    file2Stats*: JsonNode

  ComponentDiff = object
    path: string
    field: string
    value1: string
    value2: string

proc collectComponentStats(comp: ptr IRComponent, path: string = "root"): JsonNode =
  ## Recursively collect statistics about a component tree
  result = newJObject()

  # Component type
  result["type"] = %($comp.`type`)
  result["id"] = %(comp.id)
  result["path"] = %path

  # Text content if present
  if comp.text_content != nil:
    let text = $comp.text_content
    if text.len > 0:
      result["text"] = %(if text.len > 50: text[0..50] & "..." else: text)

  # Child count
  result["child_count"] = %(comp.child_count)

  # Style info if present
  if comp.style != nil:
    var styleInfo = newJObject()
    styleInfo["has_background"] = %(comp.style.background.`type` != IR_COLOR_TRANSPARENT)
    styleInfo["has_padding"] = %(comp.style.padding.top != 0 or comp.style.padding.right != 0 or
                                  comp.style.padding.bottom != 0 or comp.style.padding.left != 0)
    result["style"] = styleInfo

  # Recursively process children
  if comp.child_count > 0:
    var children = newJArray()
    let childArray = cast[ptr UncheckedArray[ptr IRComponent]](comp.children)
    for i in 0..<comp.child_count:
      let childPath = path & "/" & $i
      children.add(collectComponentStats(childArray[i], childPath))
    result["children"] = children

  return result

proc compareComponentTrees(tree1, tree2: JsonNode, path: string = "root"): seq[ComponentDiff] =
  ## Compare two component trees and return differences
  result = @[]

  # Compare type
  if tree1["type"].getStr() != tree2["type"].getStr():
    result.add(ComponentDiff(
      path: path,
      field: "type",
      value1: tree1["type"].getStr(),
      value2: tree2["type"].getStr()
    ))

  # Compare child count
  if tree1["child_count"].getInt() != tree2["child_count"].getInt():
    result.add(ComponentDiff(
      path: path,
      field: "child_count",
      value1: $tree1["child_count"].getInt(),
      value2: $tree2["child_count"].getInt()
    ))
    # If child counts differ, don't recurse into children
    return result

  # Compare text content
  if tree1.hasKey("text") and tree2.hasKey("text"):
    if tree1["text"].getStr() != tree2["text"].getStr():
      result.add(ComponentDiff(
        path: path,
        field: "text",
        value1: tree1["text"].getStr(),
        value2: tree2["text"].getStr()
      ))
  elif tree1.hasKey("text") or tree2.hasKey("text"):
    result.add(ComponentDiff(
      path: path,
      field: "text",
      value1: if tree1.hasKey("text"): tree1["text"].getStr() else: "(none)",
      value2: if tree2.hasKey("text"): tree2["text"].getStr() else: "(none)"
    ))

  # Recurse into children if present
  if tree1.hasKey("children") and tree2.hasKey("children"):
    let children1 = tree1["children"].getElems()
    let children2 = tree2["children"].getElems()
    for i in 0..<min(children1.len, children2.len):
      let childPath = path & "/" & $i
      result.add(compareComponentTrees(children1[i], children2[i], childPath))

proc diffIRFiles*(file1: string, file2: string): DiffResult =
  ## Compare two IR files and return differences
  result.same = false
  result.differences = @[]

  # Validate files exist
  if not fileExists(file1):
    result.differences.add("File 1 not found: " & file1)
    return

  if not fileExists(file2):
    result.differences.add("File 2 not found: " & file2)
    return

  # Load both IR files
  let buffer1 = ir_buffer_create_from_file(cstring(file1))
  if buffer1 == nil:
    result.differences.add("Failed to read file 1: " & file1)
    return

  let buffer2 = ir_buffer_create_from_file(cstring(file2))
  if buffer2 == nil:
    ir_buffer_destroy(buffer1)
    result.differences.add("Failed to read file 2: " & file2)
    return

  # Deserialize both files
  let root1 = ir_deserialize_binary(buffer1)
  let root2 = ir_deserialize_binary(buffer2)

  ir_buffer_destroy(buffer1)
  ir_buffer_destroy(buffer2)

  if root1 == nil:
    result.differences.add("Failed to deserialize file 1")
    return

  if root2 == nil:
    ir_destroy_component(root1)
    result.differences.add("Failed to deserialize file 2")
    return

  # Collect stats for both files
  result.file1Stats = collectComponentStats(root1)
  result.file2Stats = collectComponentStats(root2)

  # Compare component trees
  let componentDiffs = compareComponentTrees(result.file1Stats, result.file2Stats)

  # Format differences
  for diff in componentDiffs:
    result.differences.add(diff.path & " [" & diff.field & "]: " &
                          diff.value1 & " -> " & diff.value2)

  # Cleanup
  ir_destroy_component(root1)
  ir_destroy_component(root2)

  result.same = result.differences.len == 0

proc countComponents(tree: JsonNode): int =
  ## Count total components in a tree
  result = 1
  if tree.hasKey("children"):
    for child in tree["children"].getElems():
      result += countComponents(child)

proc handleDiffCommand*(args: seq[string]) =
  ## Handle 'kryon diff' command
  if args.len < 2:
    echo "Error: Two IR files required"
    echo "Usage: kryon diff <file1.kir> <file2.kir> [--json]"
    return

  let file1 = args[0]
  let file2 = args[1]
  let jsonOutput = args.len > 2 and args[2] == "--json"

  echo "📊 Comparing IR files..."
  echo "   File 1: ", file1
  echo "   File 2: ", file2
  echo ""

  let result = diffIRFiles(file1, file2)

  if jsonOutput:
    # JSON output
    var output = newJObject()
    output["same"] = %result.same
    output["differences"] = %result.differences
    output["file1"] = result.file1Stats
    output["file2"] = result.file2Stats
    echo output.pretty()
  else:
    # Human-readable output
    if result.same:
      echo "✅ Files are identical"
    else:
      echo "❌ Files differ (" & $result.differences.len & " differences)"
      echo ""

      # Show summary stats
      if not result.file1Stats.isNil and not result.file2Stats.isNil:
        let count1 = countComponents(result.file1Stats)
        let count2 = countComponents(result.file2Stats)

        echo "Component counts:"
        echo "  File 1: ", count1, " components"
        echo "  File 2: ", count2, " components"

        if count1 != count2:
          echo "  Δ: ", (if count2 > count1: "+" else: ""), count2 - count1
        echo ""

      # Show differences
      echo "Differences:"
      for i, diff in result.differences:
        echo "  " & $(i+1) & ". " & diff
