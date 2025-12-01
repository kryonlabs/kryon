## Animation and Body Component Macros
##
## This module provides:
## - Body: Top-level container with animation support

import macros, strutils, sequtils
import ./properties
import ./reactive
import ./core
import ./layout
import ./helpers  # For colorNode and other helper functions

export properties, reactive, core, layout, helpers

# Re-export runtime symbols needed by animation macros
import ../runtime
import ../ir_core

export runtime, ir_core


macro Body*(props: untyped, windowWidth: untyped = nil, windowHeight: untyped = nil): untyped =
  ## Body macro - top-level container that fills the window by default
  var
    bodyStmt = newTree(nnkStmtList)
    backgroundSet = false
    posXSet = false
    posYSet = false
    flexGrowSet = false
    widthSet = false
    heightSet = false
    layoutDirectionSet = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "backgroundcolor":
        backgroundSet = true
      of "posx", "x":
        posXSet = true
      of "posy", "y":
        posYSet = true
      of "flexgrow":
        flexGrowSet = true
      of "width":
        widthSet = true
      of "height":
        heightSet = true
      of "layoutdirection":
        layoutDirectionSet = true
      else:
        discard

  # Add defaults first (before user properties)
  if not posXSet:
    bodyStmt.add newTree(nnkAsgn, ident("posX"), newIntLitNode(0))
  if not posYSet:
    bodyStmt.add newTree(nnkAsgn, ident("posY"), newIntLitNode(0))
  if not backgroundSet:
    bodyStmt.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("#101820FF"))
  if not layoutDirectionSet:
    # Default to LAYOUT_COLUMN (value 0) to support contentAlignment and proper centering
    bodyStmt.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))

  # Then add user properties (including width/height from ensureBodyDimensions)
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "backgroundcolor":
        backgroundSet = true
      of "posx", "x":
        posXSet = true
      of "posy", "y":
        posYSet = true
      of "flexgrow":
        flexGrowSet = true
      of "width":
        widthSet = true
      of "height":
        heightSet = true
      of "layoutdirection":
        layoutDirectionSet = true
      bodyStmt.add(node)
    elif node.kind == nnkCaseStmt:
      # Convert case statement to reactive conditional using existing infrastructure
      echo "[kryon][case] Converting case statement to reactive conditional"
      # Use provided window dimensions or defaults
      let bodyWidth = if windowWidth != nil: windowWidth else: newIntLitNode(800)
      let bodyHeight = if windowHeight != nil: windowHeight else: newIntLitNode(600)
      let reactiveResult = convertCaseStmtToReactiveConditional(node, bodyWidth, bodyHeight)
      bodyStmt.add(reactiveResult)
    elif node.kind == nnkIfStmt:
      # Convert if statement to reactive conditional using existing infrastructure
      echo "[kryon][if] Converting if statement to reactive conditional"
      # Use provided window dimensions or defaults
      let bodyWidth = if windowWidth != nil: windowWidth else: newIntLitNode(800)
      let bodyHeight = if windowHeight != nil: windowHeight else: newIntLitNode(600)
      let reactiveResult = convertIfStmtToReactiveConditional(node, bodyWidth, bodyHeight)
      bodyStmt.add(reactiveResult)
    else:
      # Regular node - add as-is
      bodyStmt.add(node)

  result = newTree(nnkCall, ident("Container"), bodyStmt)
