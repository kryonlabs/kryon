## Style System and Utility Macros
##
## This module provides:
## - style macro: Define reusable style functions with conditional logic
## - Re-exports all helper functions from the helpers module

import macros, strutils
import ./properties
import ./helpers

export properties, helpers

# Re-export runtime symbols needed by style macro
import ../runtime
import ../ir_core

export runtime, ir_core

macro style*(styleCall: untyped, body: untyped): untyped =
  ## Define a reusable style function
  ## Usage:
  ##   style calendarDayStyle(day: CalendarDay):
  ##     backgroundColor = "#3d3d3d"
  ##     textColor = "#ffffff"
  ##     if day.isCompleted:
  ##       backgroundColor = "#4a90e2"

  # Parse the style call: calendarDayStyle(day: CalendarDay)
  var styleName: NimNode
  var params: seq[NimNode]

  if styleCall.kind == nnkCall:
    # Function call syntax: calendarDayStyle(day: CalendarDay)
    styleName = styleCall[0]  # The function name

    # Extract individual parameter definitions
    var userParams: seq[NimNode] = @[]

    for i in 1..<styleCall.len:
      let arg = styleCall[i]
      if arg.kind == nnkExprEqExpr:
        # Parameter with type: day: CalendarDay
        userParams.add(nnkIdentDefs.newTree(arg[0], arg[1], newEmptyNode()))
      else:
        # Just a parameter name, infer type as auto
        userParams.add(nnkIdentDefs.newTree(arg, ident("auto"), newEmptyNode()))

    params = userParams

  elif styleCall.kind == nnkObjConstr:
    # Object constructor syntax: calendarDayStyle(day: CalendarDay)
    styleName = styleCall[0]  # The function name

    # Extract individual parameter definitions (not as FormalParams yet)
    var userParams: seq[NimNode] = @[]

    for i in 1..<styleCall.len:
      let arg = styleCall[i]
      if arg.kind == nnkExprColonExpr:
        # Parameter with type: day: CalendarDay
        userParams.add(nnkIdentDefs.newTree(arg[0], arg[1], newEmptyNode()))
      else:
        # Just a parameter name, infer type as auto
        userParams.add(nnkIdentDefs.newTree(arg, ident("auto"), newEmptyNode()))

    # Convert userParams to a simpler format for later processing
    params = userParams

  elif styleCall.kind == nnkIdent:
    # Simple name: myStyle
    styleName = styleCall
    params = @[]  # No user parameters
  else:
    echo "Style call kind: ", styleCall.kind
    echo "Style call repr: ", styleCall.treeRepr
    error("Invalid style function call format", styleCall)

  # Generate the style function name
  let styleFuncName = ident($styleName)

  # Parse the body to extract property assignments
  var propertyStmts: seq[NimNode] = @[]

  for stmt in body.children:
    if stmt.kind == nnkAsgn:
      let propName = $stmt[0]
      let value = stmt[1]

      case propName.toLowerAscii():
      of "backgroundcolor":
        propertyStmts.add newCall(
          ident("kryon_component_set_background_color"),
          ident("component"),
          colorNode(value)
        )
      of "textcolor", "color":
        propertyStmts.add newCall(
          ident("kryon_component_set_text_color"),
          ident("component"),
          colorNode(value)
        )
      of "bordercolor":
        propertyStmts.add newCall(
          ident("kryon_component_set_border_color"),
          ident("component"),
          colorNode(value)
        )
      of "borderwidth":
        propertyStmts.add newCall(
          ident("kryon_component_set_border_width"),
          ident("component"),
          newCall("uint8", value)
        )
      of "textfade":
        # Parse: textFade = horizontal(15) or textFade = none
        if value.kind == nnkCall:
          let fadeTypeStr = ($value[0]).toLowerAscii()
          var fadeType = ident("IR_TEXT_FADE_NONE")
          var fadeLength = newFloatLitNode(0.0)
          case fadeTypeStr:
          of "horizontal":
            fadeType = ident("IR_TEXT_FADE_HORIZONTAL")
            if value.len > 1:
              fadeLength = value[1]
          of "vertical":
            fadeType = ident("IR_TEXT_FADE_VERTICAL")
            if value.len > 1:
              fadeLength = value[1]
          of "radial":
            fadeType = ident("IR_TEXT_FADE_RADIAL")
            if value.len > 1:
              fadeLength = value[1]
          of "none":
            fadeType = ident("IR_TEXT_FADE_NONE")
          else:
            discard
          propertyStmts.add newCall(
            ident("kryon_component_set_text_fade"),
            ident("component"),
            fadeType,
            newCall("float", fadeLength)
          )
        elif value.kind == nnkIdent and ($value).toLowerAscii() == "none":
          propertyStmts.add newCall(
            ident("kryon_component_set_text_fade"),
            ident("component"),
            ident("IR_TEXT_FADE_NONE"),
            newFloatLitNode(0.0)
          )
      of "textoverflow":
        # Parse: textOverflow = ellipsis | clip | fade | visible
        let overflowStr = if value.kind == nnkIdent: ($value).toLowerAscii() else: ""
        var overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
        case overflowStr:
        of "visible": overflowType = ident("IR_TEXT_OVERFLOW_VISIBLE")
        of "clip": overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
        of "ellipsis": overflowType = ident("IR_TEXT_OVERFLOW_ELLIPSIS")
        of "fade": overflowType = ident("IR_TEXT_OVERFLOW_FADE")
        else: discard
        propertyStmts.add newCall(
          ident("kryon_component_set_text_overflow"),
          ident("component"),
          overflowType
        )
      of "opacity":
        propertyStmts.add newCall(
          ident("kryon_component_set_opacity"),
          ident("component"),
          newCall("float", value)
        )
      else:
        # Unknown property - skip for now
        discard
    elif stmt.kind == nnkIfStmt:
      # Handle conditional styling
      var ifBranches: seq[NimNode] = @[]
      for branch in stmt.children:
        if branch.len >= 2:
          let condition = branch[0]
          let branchStmts = branch[1]

          var branchProperties: seq[NimNode] = @[]
          for branchStmt in branchStmts:
            if branchStmt.kind == nnkAsgn:
              let propName = $branchStmt[0]
              let value = branchStmt[1]

              case propName.toLowerAscii():
              of "backgroundcolor":
                branchProperties.add newCall(
                  ident("kryon_component_set_background_color"),
                  ident("component"),
                  colorNode(value)
                )
              of "textcolor", "color":
                branchProperties.add newCall(
                  ident("kryon_component_set_text_color"),
                  ident("component"),
                  colorNode(value)
                )
              of "bordercolor":
                branchProperties.add newCall(
                  ident("kryon_component_set_border_color"),
                  ident("component"),
                  colorNode(value)
                )
              of "textfade":
                if value.kind == nnkCall:
                  let fadeTypeStr = ($value[0]).toLowerAscii()
                  var fadeType = ident("IR_TEXT_FADE_NONE")
                  var fadeLength = newFloatLitNode(0.0)
                  case fadeTypeStr:
                  of "horizontal":
                    fadeType = ident("IR_TEXT_FADE_HORIZONTAL")
                    if value.len > 1: fadeLength = value[1]
                  of "vertical":
                    fadeType = ident("IR_TEXT_FADE_VERTICAL")
                    if value.len > 1: fadeLength = value[1]
                  of "radial":
                    fadeType = ident("IR_TEXT_FADE_RADIAL")
                    if value.len > 1: fadeLength = value[1]
                  of "none":
                    fadeType = ident("IR_TEXT_FADE_NONE")
                  else: discard
                  branchProperties.add newCall(
                    ident("kryon_component_set_text_fade"),
                    ident("component"),
                    fadeType,
                    newCall("float", fadeLength)
                  )
                elif value.kind == nnkIdent and ($value).toLowerAscii() == "none":
                  branchProperties.add newCall(
                    ident("kryon_component_set_text_fade"),
                    ident("component"),
                    ident("IR_TEXT_FADE_NONE"),
                    newFloatLitNode(0.0)
                  )
              of "textoverflow":
                let overflowStr = if value.kind == nnkIdent: ($value).toLowerAscii() else: ""
                var overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
                case overflowStr:
                of "visible": overflowType = ident("IR_TEXT_OVERFLOW_VISIBLE")
                of "clip": overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
                of "ellipsis": overflowType = ident("IR_TEXT_OVERFLOW_ELLIPSIS")
                of "fade": overflowType = ident("IR_TEXT_OVERFLOW_FADE")
                else: discard
                branchProperties.add newCall(
                  ident("kryon_component_set_text_overflow"),
                  ident("component"),
                  overflowType
                )
              of "opacity":
                branchProperties.add newCall(
                  ident("kryon_component_set_opacity"),
                  ident("component"),
                  newCall("float", value)
                )
              else:
                discard

          if branchProperties.len > 0:
            ifBranches.add(newTree(nnkElifBranch, condition, newStmtList(branchProperties)))

      if ifBranches.len > 0:
        # Build a single if statement with all branches
        var ifStmt = newTree(nnkIfStmt, ifBranches[0])
        for i in 1..<ifBranches.len:
          ifStmt.add(ifBranches[i])
        propertyStmts.add(ifStmt)

  # Generate the style function with the component and any parameters
  let componentParam = ident("component")

  # Start with the return type
  var paramDecl = nnkFormalParams.newTree(ident("void"))

  # Always include component parameter first
  paramDecl.add(nnkIdentDefs.newTree(componentParam, ident("KryonComponent"), newEmptyNode()))

  # Add user parameters (params is now a seq of individual parameter definitions)
  for paramDef in params:
    paramDecl.add(paramDef)

  # Extract complete parameter list for newProc (skip return type, include component)
  var paramList: seq[NimNode] = @[]
  for i in 1..<paramDecl.len:  # Skip return type at index 0
    paramList.add(paramDecl[i])

  # Use the parsed parameters and body to create the complete style function
  # Convert paramDecl to a sequence for newProc
  var paramSeq: seq[NimNode] = @[]
  for param in paramDecl:
    paramSeq.add(param)

  result = newProc(
    styleFuncName,
    paramSeq,
    newStmtList(propertyStmts)
  )
