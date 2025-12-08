## .kry to .kir Transpiler
##
## Converts a .kry AST into JSON IR (.kir format v3.0)

import std/[json, tables, strutils, strformat, sequtils]
import kry_ast, kry_parser

# Note: Named colors (e.g., "yellow", "red") are resolved by the IR layer
# in json_parse_color() via ir_color_named(). No normalization needed here.

type
  ConstValue = object
    typeName: string
    value: KryNode
    jsonValue: JsonNode  # Pre-computed JSON for use in expansion

  TranspilerContext = object
    componentDefs: Table[string, KryNode]
    globalStates: seq[KryNode]
    constants: Table[string, ConstValue]  # Now stores type + value
    loopBindings: Table[string, KryNode]  # Loop variable bindings for const for
    nextId: int
    logicFunctions: JsonNode
    eventBindings: seq[JsonNode]
    reactiveVars: seq[JsonNode]
    conditionals: seq[JsonNode]
    foreachLoops: seq[JsonNode]  # For reactive for loops
    sources: Table[string, string]  # Language-specific source code blocks
    preserveStatic: bool  # If true, preserve static blocks as structured nodes for codegen
    staticBlocks: seq[JsonNode]  # Preserved static block definitions

proc newContext(preserveStatic = false): TranspilerContext =
  TranspilerContext(
    componentDefs: initTable[string, KryNode](),
    globalStates: @[],
    constants: initTable[string, ConstValue](),
    loopBindings: initTable[string, KryNode](),
    nextId: 1,
    logicFunctions: newJObject(),
    eventBindings: @[],
    reactiveVars: @[],
    conditionals: @[],
    foreachLoops: @[],
    sources: initTable[string, string](),
    preserveStatic: preserveStatic,
    staticBlocks: @[]
  )

proc genId(ctx: var TranspilerContext): int =
  result = ctx.nextId
  ctx.nextId.inc

# Forward declarations
proc transpileExpression(ctx: var TranspilerContext, node: KryNode): JsonNode
proc transpileComponent(ctx: var TranspilerContext, node: KryNode, parentId = 0): JsonNode
proc transpileChildren(ctx: var TranspilerContext, children: seq[KryNode], parentId: int): seq[JsonNode]

# Transpile expressions to universal logic format
proc transpileExpression(ctx: var TranspilerContext, node: KryNode): JsonNode =
  if node.isNil:
    return newJNull()

  case node.kind
  of nkExprLiteral:
    case node.litKind
    of lkString:
      %node.litString
    of lkInt:
      %node.litInt
    of lkFloat:
      %node.litFloat
    of lkBool:
      %node.litBool
    of lkPercent:
      %(&"{node.litFloat}%")
    of lkAuto:
      %"auto"
    of lkColor:
      %node.litString
  of nkExprIdent:
    # Check if it's a loop variable binding (const for expansion)
    if ctx.loopBindings.hasKey(node.identName):
      return ctx.transpileExpression(ctx.loopBindings[node.identName])
    # Check if it's a constant
    if ctx.constants.hasKey(node.identName):
      return ctx.constants[node.identName].jsonValue
    # Otherwise it's a runtime variable reference
    %*{"var": node.identName}
  of nkExprBinary:
    let opStr = case node.binOp
      of opAdd: "add"
      of opSub: "sub"
      of opMul: "mul"
      of opDiv: "div"
      of opMod: "mod"
      of opEq: "eq"
      of opNe: "ne"
      of opLt: "lt"
      of opLe: "le"
      of opGt: "gt"
      of opGe: "ge"
      of opAnd: "and"
      of opOr: "or"
      of opAssign: "assign"
      of opAddAssign: "add_assign"
      of opSubAssign: "sub_assign"

    if node.binOp in {opAssign, opAddAssign, opSubAssign}:
      # Assignment becomes a statement
      var target = ""
      if node.binLeft.kind == nkExprIdent:
        target = node.binLeft.identName

      if node.binOp == opAssign:
        %*{
          "op": "assign",
          "target": target,
          "expr": ctx.transpileExpression(node.binRight)
        }
      else:
        # Compound assignment: x += 1 → x = x + 1
        let innerOp = if node.binOp == opAddAssign: "add" else: "sub"
        %*{
          "op": "assign",
          "target": target,
          "expr": {
            "op": innerOp,
            "left": {"var": target},
            "right": ctx.transpileExpression(node.binRight)
          }
        }
    else:
      %*{
        "op": opStr,
        "left": ctx.transpileExpression(node.binLeft),
        "right": ctx.transpileExpression(node.binRight)
      }
  of nkExprUnary:
    let opStr = case node.unaryOp
      of opNot: "not"
      of opNeg: "neg"
    %*{
      "op": opStr,
      "expr": ctx.transpileExpression(node.unaryExpr)
    }
  of nkExprCall:
    # Special case: index(array, int) - try compile-time evaluation
    if node.callName == "index" and node.callArgs.len == 2:
      let arrExpr = node.callArgs[0]
      let idxExpr = node.callArgs[1]

      # Try to get array value (constant or loop binding)
      var arrValue: KryNode = nil
      if arrExpr.kind == nkExprIdent:
        if ctx.loopBindings.hasKey(arrExpr.identName):
          arrValue = ctx.loopBindings[arrExpr.identName]
        elif ctx.constants.hasKey(arrExpr.identName):
          arrValue = ctx.constants[arrExpr.identName].value
      elif arrExpr.kind == nkExprArray:
        arrValue = arrExpr
      elif arrExpr.kind == nkExprMember:
        # Handle item.colors where item is a loop-bound object
        if arrExpr.memberObj.kind == nkExprIdent:
          let objName = arrExpr.memberObj.identName
          if ctx.loopBindings.hasKey(objName):
            let bound = ctx.loopBindings[objName]
            if bound.kind == nkExprObject:
              for (key, value) in bound.objectFields:
                if key == arrExpr.memberName:
                  arrValue = value
                  break

      # Try to get index value (loop binding or literal)
      var idxValue: int = -1
      var idxKnown = false
      if idxExpr.kind == nkExprIdent:
        if ctx.loopBindings.hasKey(idxExpr.identName):
          let bound = ctx.loopBindings[idxExpr.identName]
          if bound.kind == nkExprLiteral and bound.litKind == lkInt:
            idxValue = bound.litInt.int
            idxKnown = true
      elif idxExpr.kind == nkExprLiteral and idxExpr.litKind == lkInt:
        idxValue = idxExpr.litInt.int
        idxKnown = true

      # If both are known at compile time, return the element
      if arrValue != nil and arrValue.kind == nkExprArray and idxKnown:
        if idxValue >= 0 and idxValue < arrValue.arrayElems.len:
          return ctx.transpileExpression(arrValue.arrayElems[idxValue])

    # Fallback to runtime call
    %*{
      "call": node.callName,
      "args": node.callArgs.mapIt(ctx.transpileExpression(it))
    }
  of nkExprMember:
    # Check if object is a loop binding (for static loop expansion)
    if node.memberObj.kind == nkExprIdent:
      let objName = node.memberObj.identName
      if ctx.loopBindings.hasKey(objName):
        let bound = ctx.loopBindings[objName]
        # If bound to an object, resolve member at compile time
        if bound.kind == nkExprObject:
          for (key, value) in bound.objectFields:
            if key == node.memberName:
              return ctx.transpileExpression(value)
    # Fallback to runtime member access
    %*{
      "member": node.memberName,
      "object": ctx.transpileExpression(node.memberObj)
    }
  of nkExprTernary:
    %*{
      "op": "ternary",
      "condition": ctx.transpileExpression(node.ternCond),
      "then": ctx.transpileExpression(node.ternThen),
      "else": ctx.transpileExpression(node.ternElse)
    }
  of nkExprInterp:
    # $value → "{{value}}"
    if node.interpExpr.kind == nkExprIdent:
      %("{{" & node.interpExpr.identName & "}}")
    else:
      ctx.transpileExpression(node.interpExpr)
  of nkExprArray:
    %*(node.arrayElems.mapIt(ctx.transpileExpression(it)))
  of nkExprBlock:
    # Block of statements → universal logic
    %*{
      "statements": node.blockStmts.mapIt(ctx.transpileExpression(it))
    }
  of nkExprObject:
    # Object literal { key: value, ... } → JSON object
    var obj = newJObject()
    for (key, value) in node.objectFields:
      obj[key] = ctx.transpileExpression(value)
    obj
  else:
    %(&"<unsupported: {node.kind}>")

# Get simple value from expression (for properties)
proc getSimpleValue(ctx: var TranspilerContext, node: KryNode): JsonNode =
  case node.kind
  of nkExprLiteral:
    case node.litKind
    of lkString: %node.litString
    of lkInt: %node.litInt
    of lkFloat: %node.litFloat
    of lkBool: %node.litBool
    of lkPercent: %(&"{node.litFloat}%")
    of lkAuto: %"auto"
    of lkColor: %node.litString
  of nkExprInterp:
    if node.interpExpr.kind == nkExprIdent:
      %("{{" & node.interpExpr.identName & "}}")
    else:
      ctx.transpileExpression(node.interpExpr)
  else:
    ctx.transpileExpression(node)

# Map property names to IR property names
proc mapPropertyName(name: string): string =
  case name
  of "backgroundColor": "background"
  of "borderColor": "border_color"  # Handled specially for nested border object
  of "fontSize": "fontSize"
  of "fontFamily": "fontFamily"
  of "fontWeight": "fontWeight"
  of "alignItems": "alignItems"
  of "justifyContent": "justifyContent"
  of "contentAlignment": "contentAlignment"  # Handled specially
  of "flexDirection": "flexDirection"
  of "flexGrow": "flexGrow"
  of "flexShrink": "flexShrink"
  of "paddingTop": "paddingTop"
  of "paddingBottom": "paddingBottom"
  of "paddingLeft": "paddingLeft"
  of "paddingRight": "paddingRight"
  of "marginTop": "marginTop"
  of "marginBottom": "marginBottom"
  of "marginLeft": "marginLeft"
  of "marginRight": "marginRight"
  of "windowTitle": "window_title"
  of "windowWidth": "window_width"
  of "windowHeight": "window_height"
  of "onClick": "on_click"
  of "onLoad": "on_load"
  of "onHover": "on_hover"
  else: name

# Transpile a component instance to IR
proc transpileComponent(ctx: var TranspilerContext, node: KryNode, parentId = 0): JsonNode =
  let id = ctx.genId()

  result = %*{
    "id": id,
    "type": node.componentType
  }

  # Collect border properties for nested object
  var borderWidth: JsonNode = nil
  var borderColor: JsonNode = nil
  var borderRadius: JsonNode = nil
  var posX: JsonNode = nil
  var posY: JsonNode = nil

  # Handle positional arguments for custom components
  # Map positional args to parameter names from component definition
  if node.componentArgs.len > 0 and ctx.componentDefs.hasKey(node.componentType):
    let compDef = ctx.componentDefs[node.componentType]
    for i, arg in node.componentArgs:
      if i < compDef.compParams.len:
        let paramName = compDef.compParams[i].name
        let simpleVal = ctx.getSimpleValue(arg)
        result[paramName] = simpleVal

  # Process properties
  for name, value in node.componentProps:
    let irName = mapPropertyName(name)

    # Handle event handlers specially
    if irName.startsWith("on_"):
      let eventType = irName[3..^1]  # e.g., "click" from "on_click"
      let handlerName = &"handler_{id}_{eventType}"

      # Add to logic functions
      if value.kind == nkExprBlock:
        ctx.logicFunctions[handlerName] = %*{
          "universal": {
            "statements": value.blockStmts.mapIt(ctx.transpileExpression(it))
          }
        }
      else:
        ctx.logicFunctions[handlerName] = %*{
          "universal": {
            "statements": [ctx.transpileExpression(value)]
          }
        }

      # Add event binding
      ctx.eventBindings.add(%*{
        "component_id": id,
        "event": eventType,
        "handler": handlerName
      })

      # Add events array to component
      if not result.hasKey("events"):
        result["events"] = newJArray()
      result["events"].add(%*{
        "type": eventType,
        "logic_id": handlerName
      })
    else:
      # Regular property
      let simpleVal = ctx.getSimpleValue(value)

      case irName
      of "width", "height", "minWidth", "minHeight", "maxWidth", "maxHeight":
        if simpleVal.kind == JInt:
          result[irName] = %(&"{simpleVal.getInt}.0px")
        elif simpleVal.kind == JFloat:
          result[irName] = %(&"{simpleVal.getFloat}px")
        elif simpleVal.kind == JString:
          let s = simpleVal.getStr
          if s.endsWith("%") or s == "auto":
            result[irName] = simpleVal
          else:
            result[irName] = %(&"{s}px")
        else:
          result[irName] = simpleVal
      of "background", "color":
        # Pass color through - named colors resolved by IR layer
        result[irName] = simpleVal
      of "text":
        result["text"] = simpleVal
        # Check for text_expression
        if value.kind == nkExprInterp:
          if value.interpExpr.kind == nkExprIdent:
            result["text_expression"] = %("{{" & value.interpExpr.identName & "}}")
      # Handle border properties - collect for nested object
      of "borderWidth":
        borderWidth = simpleVal
      of "border_color":
        # Pass color through - named colors resolved by IR layer
        borderColor = simpleVal
      of "borderRadius":
        borderRadius = simpleVal
      # Handle contentAlignment → justifyContent + alignItems
      of "contentAlignment":
        let alignStr = simpleVal.getStr("start")
        result["justifyContent"] = %alignStr
        result["alignItems"] = %alignStr
      # Handle absolute positioning
      of "posX":
        posX = simpleVal
      of "posY":
        posY = simpleVal
      else:
        result[irName] = simpleVal

  # Build nested border object if any border properties were set
  if not borderWidth.isNil or not borderColor.isNil or not borderRadius.isNil:
    var borderObj = newJObject()
    if not borderWidth.isNil:
      borderObj["width"] = borderWidth
    if not borderColor.isNil:
      borderObj["color"] = borderColor
    if not borderRadius.isNil:
      borderObj["radius"] = borderRadius
    result["border"] = borderObj

  # Add absolute positioning if specified
  if not posX.isNil or not posY.isNil:
    result["position"] = %"absolute"
    if not posX.isNil:
      result["left"] = posX
    if not posY.isNil:
      result["top"] = posY

  # Process children
  if node.componentChildren.len > 0:
    let children = ctx.transpileChildren(node.componentChildren, id)
    if children.len > 0:
      result["children"] = %children

# Forward declaration for transpileChildrenAsTemplate
proc transpileChildrenAsTemplate(ctx: var TranspilerContext, children: seq[KryNode], parentId: int, loopVar: string): seq[JsonNode]

# Transpile an expression as a template reference (preserving loop var references)
proc transpileExpressionAsTemplate(ctx: var TranspilerContext, node: KryNode, loopVar: string): JsonNode =
  if node.isNil:
    return newJNull()

  case node.kind
  of nkExprMember:
    # item.name → "{{item.name}}"
    if node.memberObj.kind == nkExprIdent and node.memberObj.identName == loopVar:
      return %(&"{{{{{loopVar}.{node.memberName}}}}}")
    # Recurse for nested members
    let objStr = ctx.transpileExpressionAsTemplate(node.memberObj, loopVar)
    if objStr.kind == JString and objStr.getStr.startsWith("{{"):
      return %(&"{objStr.getStr[0..^3]}.{node.memberName}}}}}")
    return %*{"member": node.memberName, "object": objStr}

  of nkExprIdent:
    if node.identName == loopVar:
      return %(&"{{{{{loopVar}}}}}")
    return ctx.transpileExpression(node)

  of nkExprCall:
    # Handle item.colors[0] → "{{item.colors[0]}}"
    if node.callName == "index" and node.callArgs.len == 2:
      let arrExpr = node.callArgs[0]
      let idxExpr = node.callArgs[1]

      # Check if array is a member of loop var: item.colors
      if arrExpr.kind == nkExprMember and arrExpr.memberObj.kind == nkExprIdent:
        if arrExpr.memberObj.identName == loopVar:
          if idxExpr.kind == nkExprLiteral and idxExpr.litKind == lkInt:
            return %(&"{{{{{loopVar}.{arrExpr.memberName}[{idxExpr.litInt}]}}}}")

    return ctx.transpileExpression(node)

  else:
    return ctx.transpileExpression(node)

# Transpile a component as a template (preserving loop variable references)
proc transpileComponentAsTemplate(ctx: var TranspilerContext, node: KryNode, parentId: int, loopVar: string): JsonNode =
  let componentType = node.componentType
  let id = ctx.genId()

  result = %*{
    "id": id,
    "type": componentType
  }

  # Process properties, preserving loop var references
  for name, value in node.componentProps:
    let irName = case name.toLowerAscii
      of "backgroundcolor": "background"
      of "mainaxisalignment", "mainalignment", "justify": "justifyContent"
      of "crossaxisalignment", "crossalignment", "align": "alignItems"
      of "direction", "flexdirection": "flexDirection"
      of "layoutdirection": "layoutDirection"
      else: name

    # Use template expression preserving loop var references
    let templVal = ctx.transpileExpressionAsTemplate(value, loopVar)
    result[irName] = templVal

  # Process children as templates
  if node.componentChildren.len > 0:
    let children = ctx.transpileChildrenAsTemplate(node.componentChildren, id, loopVar)
    if children.len > 0:
      result["children"] = %children

# Transpile children as templates (preserving loop var refs)
proc transpileChildrenAsTemplate(ctx: var TranspilerContext, children: seq[KryNode], parentId: int, loopVar: string): seq[JsonNode] =
  result = @[]
  for child in children:
    case child.kind
    of nkComponent:
      result.add(ctx.transpileComponentAsTemplate(child, parentId, loopVar))
    else:
      discard

# Transpile children inside a static block (compile-time evaluation)
# All for loops are expanded, conditionals evaluated if possible
proc transpileStaticChildren(ctx: var TranspilerContext, children: seq[KryNode], parentId: int): seq[JsonNode] =
  result = @[]
  for child in children:
    case child.kind
    of nkComponent:
      result.add(ctx.transpileComponent(child, parentId))

    of nkForLoop:
      # All for loops inside static are expanded at compile time
      var iterableValues: seq[KryNode] = @[]

      if child.loopExpr.kind == nkExprArray:
        iterableValues = child.loopExpr.arrayElems
      elif child.loopExpr.kind == nkExprIdent:
        let constName = child.loopExpr.identName
        if ctx.constants.hasKey(constName):
          let cv = ctx.constants[constName]
          if cv.value.kind == nkExprArray:
            iterableValues = cv.value.arrayElems
          else:
            iterableValues = @[cv.value]

      for iterVal in iterableValues:
        ctx.loopBindings[child.loopVar] = iterVal
        for bodyNode in child.loopBody:
          if bodyNode.kind == nkComponent:
            result.add(ctx.transpileComponent(bodyNode, parentId))
          elif bodyNode.kind == nkForLoop:
            # Nested static for loop - recursively expand
            result.add(ctx.transpileStaticChildren(@[bodyNode], parentId))
        ctx.loopBindings.del(child.loopVar)

    of nkConditional:
      # Try to evaluate condition at compile time
      var condValue: bool = false
      var canEvaluate = false

      # Check if condition is a simple constant or literal
      if child.condExpr.kind == nkExprLiteral and child.condExpr.litKind == lkBool:
        condValue = child.condExpr.litBool
        canEvaluate = true
      elif child.condExpr.kind == nkExprIdent:
        let constName = child.condExpr.identName
        if ctx.constants.hasKey(constName):
          let cv = ctx.constants[constName]
          if cv.value.kind == nkExprLiteral and cv.value.litKind == lkBool:
            condValue = cv.value.litBool
            canEvaluate = true

      if canEvaluate:
        # Condition is known at compile time - only include appropriate branch
        if condValue:
          result.add(ctx.transpileStaticChildren(child.thenBranch, parentId))
        else:
          result.add(ctx.transpileStaticChildren(child.elseBranch, parentId))
      else:
        # Condition is not known - treat as runtime conditional
        let condId = ctx.genId()
        var thenIds: seq[int] = @[]
        for thenChild in child.thenBranch:
          if thenChild.kind == nkComponent:
            let thenComp = ctx.transpileComponent(thenChild, parentId)
            thenIds.add(thenComp["id"].getInt)
            result.add(thenComp)
        var elseIds: seq[int] = @[]
        for elseChild in child.elseBranch:
          if elseChild.kind == nkComponent:
            let elseComp = ctx.transpileComponent(elseChild, parentId)
            elseIds.add(elseComp["id"].getInt)
            result.add(elseComp)
        ctx.conditionals.add(%*{
          "id": condId,
          "parent_id": parentId,
          "condition": ctx.transpileExpression(child.condExpr),
          "then_children_ids": thenIds,
          "else_children_ids": elseIds
        })

    of nkStaticBlock:
      # Nested static block - just process contents
      result.add(ctx.transpileStaticChildren(child.staticBody, parentId))

    else:
      discard

proc transpileChildren(ctx: var TranspilerContext, children: seq[KryNode], parentId: int): seq[JsonNode] =
  result = @[]
  for child in children:
    case child.kind
    of nkComponent:
      result.add(ctx.transpileComponent(child, parentId))
    of nkConditional:
      # Transpile conditional
      let condId = ctx.genId()

      # Transpile then branch
      var thenIds: seq[int] = @[]
      for thenChild in child.thenBranch:
        if thenChild.kind == nkComponent:
          let thenComp = ctx.transpileComponent(thenChild, parentId)
          thenIds.add(thenComp["id"].getInt)
          result.add(thenComp)

      # Transpile else branch
      var elseIds: seq[int] = @[]
      for elseChild in child.elseBranch:
        if elseChild.kind == nkComponent:
          let elseComp = ctx.transpileComponent(elseChild, parentId)
          elseIds.add(elseComp["id"].getInt)
          result.add(elseComp)

      # Add to conditionals manifest
      ctx.conditionals.add(%*{
        "id": condId,
        "parent_id": parentId,
        "condition": ctx.transpileExpression(child.condExpr),
        "then_children_ids": thenIds,
        "else_children_ids": elseIds
      })

    of nkForLoop:
      if child.loopIsConst:
        # Const for loop: expand at compile time
        # The iterable must be a known constant or literal array
        var iterableValues: seq[KryNode] = @[]

        if child.loopExpr.kind == nkExprArray:
          # Direct array literal: const for x in [1, 2, 3]
          iterableValues = child.loopExpr.arrayElems
        elif child.loopExpr.kind == nkExprIdent:
          # Reference to a constant: const for x in myColors
          let constName = child.loopExpr.identName
          if ctx.constants.hasKey(constName):
            let cv = ctx.constants[constName]
            if cv.value.kind == nkExprArray:
              iterableValues = cv.value.arrayElems
            else:
              # Single value constant - iterate once
              iterableValues = @[cv.value]
        else:
          # Unsupported iterable expression for const for
          iterableValues = @[]

        # Expand the loop body for each value
        for iterVal in iterableValues:
          # Bind the loop variable
          ctx.loopBindings[child.loopVar] = iterVal

          # Transpile each body element with the binding
          for bodyNode in child.loopBody:
            if bodyNode.kind == nkComponent:
              result.add(ctx.transpileComponent(bodyNode, parentId))
            elif bodyNode.kind == nkConditional:
              # Handle nested conditionals
              let condId = ctx.genId()
              var thenIds: seq[int] = @[]
              for thenChild in bodyNode.thenBranch:
                if thenChild.kind == nkComponent:
                  let thenComp = ctx.transpileComponent(thenChild, parentId)
                  thenIds.add(thenComp["id"].getInt)
                  result.add(thenComp)
              var elseIds: seq[int] = @[]
              for elseChild in bodyNode.elseBranch:
                if elseChild.kind == nkComponent:
                  let elseComp = ctx.transpileComponent(elseChild, parentId)
                  elseIds.add(elseComp["id"].getInt)
                  result.add(elseComp)
              ctx.conditionals.add(%*{
                "id": condId,
                "parent_id": parentId,
                "condition": ctx.transpileExpression(bodyNode.condExpr),
                "then_children_ids": thenIds,
                "else_children_ids": elseIds
              })

          # Clear the binding after processing
          ctx.loopBindings.del(child.loopVar)
      else:
        # Reactive for loop: create a foreach node for runtime iteration
        let forId = ctx.genId()

        # Create template from body
        var templateNode: JsonNode = newJNull()
        if child.loopBody.len > 0 and child.loopBody[0].kind == nkComponent:
          templateNode = ctx.transpileComponent(child.loopBody[0], parentId)

        # Add to reactive manifest foreach_loops
        ctx.foreachLoops.add(%*{
          "id": forId,
          "parent_id": parentId,
          "variable": child.loopVar,
          "iterable": ctx.transpileExpression(child.loopExpr),
          "template": templateNode
        })

    of nkStaticBlock:
      if ctx.preserveStatic:
        # Preserve static block structure for codegen
        for bodyNode in child.staticBody:
          if bodyNode.kind == nkForLoop:
            # Get iterable name
            var iterableName = ""
            if bodyNode.loopExpr.kind == nkExprIdent:
              iterableName = bodyNode.loopExpr.identName

            # Transpile template without expanding (create placeholder refs)
            var templateNodes: seq[JsonNode] = @[]
            for loopChild in bodyNode.loopBody:
              if loopChild.kind == nkComponent:
                # Transpile with loop var as template reference
                let tmpl = ctx.transpileComponentAsTemplate(loopChild, parentId, bodyNode.loopVar)
                templateNodes.add(tmpl)

            # Emit as static_for node (will be a marker in children)
            result.add(%*{
              "type": "static_for",
              "id": ctx.genId(),
              "loop_var": bodyNode.loopVar,
              "iterable": iterableName,
              "template": if templateNodes.len == 1: templateNodes[0] else: %templateNodes
            })
          else:
            # Non-loop content in static - just expand
            result.add(ctx.transpileStaticChildren(@[bodyNode], parentId))
      else:
        # Normal mode: expand static blocks at compile time
        result.add(ctx.transpileStaticChildren(child.staticBody, parentId))

    else:
      discard

# Transpile a component definition
proc transpileComponentDef(ctx: var TranspilerContext, node: KryNode): JsonNode =
  result = %*{
    "name": node.compName,
    "props": node.compParams.mapIt(%*{
      "name": it.name,
      "type": it.typeName,
      "default": if it.defaultValue != nil: ctx.getSimpleValue(it.defaultValue)
                 else: newJNull()
    }),
    "state": newJArray()
  }

  # Extract state declarations from body
  var templateChildren: seq[KryNode] = @[]
  for item in node.compBody:
    case item.kind
    of nkStateDef:
      result["state"].add(%*{
        "name": item.stateName,
        "type": item.stateType,
        "initial": if item.stateInit != nil:
                     let v = ctx.getSimpleValue(item.stateInit)
                     if v.kind == JString: v.getStr else: $v
                   else: "null"
      })
      # Add to reactive manifest
      ctx.reactiveVars.add(%*{
        "id": ctx.genId(),
        "name": &"{node.compName}:{item.stateName}",
        "type": item.stateType,
        "initial_value": if item.stateInit != nil:
                           let v = ctx.getSimpleValue(item.stateInit)
                           if v.kind == JString: v.getStr else: $v
                         else: "null",
        "scope": "component"
      })
    of nkComponent:
      templateChildren.add(item)
    of nkLangBlock:
      # Store language-specific code
      if not result.hasKey("sources"):
        result["sources"] = newJObject()
      result["sources"][item.langName] = %item.langCode
    else:
      discard

  # Create template from first component child
  if templateChildren.len > 0:
    result["template"] = ctx.transpileComponent(templateChildren[0])
    result["template"]["component_ref"] = %node.compName

# Transpile App block to root
proc transpileAppBlock(ctx: var TranspilerContext, node: KryNode): JsonNode =
  let rootId = ctx.genId()

  result = %*{
    "id": rootId,
    "type": "Column"
  }

  # Extract app-level properties
  var children: seq[KryNode] = @[]
  var appWidth = 800
  var appHeight = 600
  var appTitle = "Kryon App"
  var appBg = "#101820"

  for item in node.appBody:
    case item.kind
    of nkProperty:
      case item.propName
      of "width", "windowWidth", "window_width":
        if item.propValue.kind == nkExprLiteral and item.propValue.litKind == lkInt:
          appWidth = item.propValue.litInt.int
      of "height", "windowHeight", "window_height":
        if item.propValue.kind == nkExprLiteral and item.propValue.litKind == lkInt:
          appHeight = item.propValue.litInt.int
      of "title", "windowTitle", "window_title":
        if item.propValue.kind == nkExprLiteral and item.propValue.litKind == lkString:
          appTitle = item.propValue.litString
      of "backgroundColor", "background_color", "background":
        appBg = ctx.getSimpleValue(item.propValue).getStr("#101820")
      else:
        discard
    of nkComponent:
      children.add(item)
    of nkStateDef:
      ctx.globalStates.add(item)
      ctx.reactiveVars.add(%*{
        "id": ctx.genId(),
        "name": item.stateName,
        "type": item.stateType,
        "initial_value": if item.stateInit != nil:
                           let v = ctx.getSimpleValue(item.stateInit)
                           if v.kind == JString: v.getStr else: $v
                         else: "null",
        "scope": "global"
      })
    else:
      discard

  result["width"] = %(&"{appWidth}.0px")
  result["height"] = %(&"{appHeight}.0px")
  result["background"] = %appBg
  result["windowTitle"] = %appTitle

  # Transpile children
  if children.len > 0:
    result["children"] = %ctx.transpileChildren(children, rootId)

# Main transpile function
# Set preserveStatic=true to preserve static block structure for codegen
proc transpileToKir*(ast: KryNode, preserveStatic = false): JsonNode =
  var ctx = newContext(preserveStatic)

  # First pass: collect component definitions and global state
  for stmt in ast.statements:
    case stmt.kind
    of nkComponentDef:
      ctx.componentDefs[stmt.compName] = stmt
    of nkStateDef:
      ctx.globalStates.add(stmt)
      ctx.reactiveVars.add(%*{
        "id": ctx.genId(),
        "name": stmt.stateName,
        "type": stmt.stateType,
        "initial_value": if stmt.stateInit != nil:
                           let v = ctx.getSimpleValue(stmt.stateInit)
                           if v.kind == JString: v.getStr else: $v
                         else: "null",
        "scope": "global"
      })
    of nkConstDef:
      let jsonVal = ctx.getSimpleValue(stmt.constValue)
      ctx.constants[stmt.constName] = ConstValue(
        typeName: stmt.constType,
        value: stmt.constValue,
        jsonValue: jsonVal
      )
    of nkLangBlock:
      # Collect language-specific source code
      if ctx.sources.hasKey(stmt.langName):
        # Append to existing source for this language
        ctx.sources[stmt.langName] &= "\n" & stmt.langCode
      else:
        ctx.sources[stmt.langName] = stmt.langCode
    else:
      discard

  # Second pass: generate IR
  var root: JsonNode = nil
  var componentDefs: seq[JsonNode] = @[]

  for stmt in ast.statements:
    case stmt.kind
    of nkComponentDef:
      componentDefs.add(ctx.transpileComponentDef(stmt))
    of nkAppBlock:
      root = ctx.transpileAppBlock(stmt)
    else:
      discard

  # Build constants section
  var constantsArr = newJArray()
  for name, cv in ctx.constants:
    constantsArr.add(%*{
      "name": name,
      "type": if cv.typeName.len > 0: cv.typeName else: "any",
      "value": cv.jsonValue
    })

  # Build final IR
  result = %*{
    "format_version": "3.0",
    "component_definitions": componentDefs,
    "root": root,
    "logic": {
      "functions": ctx.logicFunctions,
      "event_bindings": %ctx.eventBindings
    },
    "reactive_manifest": {
      "variables": %ctx.reactiveVars
    }
  }

  # Add constants if present
  if constantsArr.len > 0:
    result["constants"] = constantsArr

  # Add conditionals if present
  if ctx.conditionals.len > 0:
    result["reactive_manifest"]["conditionals"] = %ctx.conditionals

  # Add foreach_loops if present
  if ctx.foreachLoops.len > 0:
    result["reactive_manifest"]["foreach_loops"] = %ctx.foreachLoops

  # Add sources if present (language-specific code blocks)
  if ctx.sources.len > 0:
    var sourcesObj = newJObject()
    for lang, code in ctx.sources:
      sourcesObj[lang] = %code
    result["sources"] = sourcesObj

# Convenience function to transpile .kry source to .kir JSON
proc kryToKir*(source: string, filename = "<input>"): JsonNode =
  let ast = parseKry(source, filename)
  transpileToKir(ast)
