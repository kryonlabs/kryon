## Layout Component Macros
##
## This module provides all layout-related component macros:
## - Container: Flexible layout container with full property support
## - Grid: CSS Grid layout container
## - Row: Horizontal flexbox layout (convenience wrapper for Container)
## - Column: Vertical flexbox layout (convenience wrapper for Container)
## - Center: Centers children horizontally and vertically
## - Spacer/Gap: Spacing elements

import macros, strutils, sequtils
import ./properties
import ./reactive
import ./core
import ./helpers  # For colorNode and other helper functions

export properties, reactive, core, helpers

# Re-export runtime symbols needed by layout macros
import ../runtime
import ../ir_core

export runtime, ir_core


macro Container*(props: untyped): untyped =
  ## Simplified Container component using C core layout API
  var
    containerName = genSym(nskLet, "container")
    childNodes: seq[NimNode] = @[]
    sideEffectStmts: seq[NimNode] = @[]
    deferredIfStmts: seq[NimNode] = @[]  # If statements to process with parent context
    # Layout properties - default to HTML-like behavior (column layout)
    layoutDirectionVal = newIntLitNode(0)  # Default to column layout
    justifyContentVal = alignmentNode("start")  # Start alignment
    alignItemsVal = alignmentNode("start")      # Start alignment
    gapVal = newIntLitNode(0)
    # Visual properties
    bgColorVal: NimNode = nil
    bgGradientVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    borderRadiusVal: NimNode = nil
    boxShadowVal: NimNode = nil
    # Filter properties
    blurVal: NimNode = nil
    brightnessVal: NimNode = nil
    contrastVal: NimNode = nil
    grayscaleVal: NimNode = nil
    hueRotateVal: NimNode = nil
    invertVal: NimNode = nil
    filterOpacityVal: NimNode = nil
    saturateVal: NimNode = nil
    sepiaVal: NimNode = nil
    # Position properties
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    minWidthVal: NimNode = nil
    maxWidthVal: NimNode = nil
    minHeightVal: NimNode = nil
    maxHeightVal: NimNode = nil
    aspectRatioVal: NimNode = nil
    zIndexVal: NimNode = nil
    # Spacing properties
    paddingAll: NimNode = nil
    paddingTopVal: NimNode = nil
    paddingRightVal: NimNode = nil
    paddingBottomVal: NimNode = nil
    paddingLeftVal: NimNode = nil
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    # Flex properties
    flexGrowVal: NimNode = nil
    flexShrinkVal: NimNode = nil
    # Style properties
    styleName: NimNode = nil
    styleData: NimNode = nil
    # Grid placement properties
    gridRowVal: NimNode = nil
    gridColumnVal: NimNode = nil
    gridRowStartVal: NimNode = nil
    gridRowEndVal: NimNode = nil
    gridColumnStartVal: NimNode = nil
    gridColumnEndVal: NimNode = nil
    justifySelfVal: NimNode = nil
    alignSelfVal: NimNode = nil
    # Transition properties
    transitionOpacityVal: NimNode = nil
    transitionTransformVal: NimNode = nil
    transitionSizeVal: NimNode = nil
    transitionColorVal: NimNode = nil
    # Container query properties
    containerTypeVal: NimNode = nil
    containerNameVal: NimNode = nil
    breakpointVals: seq[NimNode] = @[]
    # Animation properties
    animationVal: NimNode = nil

  # Parse properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      # Evaluate const expressions (e.g., config.field) to their literal values
      var value = evalConstExpr(node[1])

      case propName.toLowerAscii():
      of "backgroundcolor", "bg":
        bgColorVal = colorNode(value)
      of "background", "backgroundgradient", "gradient":
        # Can be either a color or a gradient
        if value.kind == nnkTupleConstr or value.kind == nnkPar:
          # Assume it's a gradient spec
          bgGradientVal = value
        else:
          # Assume it's a color
          bgColorVal = colorNode(value)
      of "color", "textcolor":
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "borderradius":
        borderRadiusVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "width", "w":
        widthVal = value
      of "height", "h":
        heightVal = value
      of "minwidth":
        minWidthVal = value
      of "maxwidth":
        maxWidthVal = value
      of "minheight":
        minHeightVal = value
      of "maxheight":
        maxHeightVal = value
      of "aspectratio":
        aspectRatioVal = value
      of "zindex", "z":
        zIndexVal = value
      of "padding", "p":
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "margin", "m":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "flex", "flexgrow":
        flexGrowVal = value
      of "flexshrink":
        flexShrinkVal = value
      of "justifycontent", "mainaxisalignment", "justify":
        justifyContentVal = parseAlignmentValue(value)
      of "alignitems", "crossaxisalignment", "align":
        alignItemsVal = parseAlignmentValue(value)
      of "contentalignment":
        justifyContentVal = parseAlignmentValue(value)
        alignItemsVal = parseAlignmentValue(value)
      of "layoutdirection":
        layoutDirectionVal = value
      of "style":
        if value.kind == nnkCall:
          styleName = newStrLitNode($value[0])
          if value.len > 1:
            styleData = value[1]
        else:
          styleName = newStrLitNode($value)
      of "boxshadow":
        boxShadowVal = value
      of "blur":
        blurVal = value
      of "brightness":
        brightnessVal = value
      of "contrast":
        contrastVal = value
      of "grayscale":
        grayscaleVal = value
      of "huerotate":
        hueRotateVal = value
      of "invert":
        invertVal = value
      of "filteropacity":
        filterOpacityVal = value
      of "saturate":
        saturateVal = value
      of "sepia":
        sepiaVal = value
      of "gridrow":
        gridRowVal = value
      of "gridcolumn":
        gridColumnVal = value
      of "gridrowstart":
        gridRowStartVal = value
      of "gridrowend":
        gridRowEndVal = value
      of "gridcolumnstart":
        gridColumnStartVal = value
      of "gridcolumnend":
        gridColumnEndVal = value
      of "justifyself":
        justifySelfVal = parseAlignmentValue(value)
      of "alignself":
        alignSelfVal = parseAlignmentValue(value)
      of "transitionopacity":
        transitionOpacityVal = value
      of "transitiontransform":
        transitionTransformVal = value
      of "transitionsize":
        transitionSizeVal = value
      of "transitioncolor":
        transitionColorVal = value
      of "containertype":
        containerTypeVal = value
      of "containername":
        containerNameVal = value
      of "breakpoint":
        breakpointVals.add(value)
      of "animation":
        animationVal = value
      else:
        discard
    else:
      # Handle static blocks specially using the same pattern as staticFor
      if isEchoStatement(node):
        # Allow echo/logging statements inside templates without treating them as children
        sideEffectStmts.add(node)
      elif node.kind in {nnkLetSection, nnkVarSection, nnkConstSection}:
        # Local bindings inside DSL blocks should execute but not become children
        sideEffectStmts.add(node)
      elif node.kind == nnkStaticStmt:
        # Static blocks will be processed later after initStmts is initialized
        # Don't add them to childNodes - they'll be handled separately
        discard
      elif node.kind == nnkIfStmt:
        # Defer if statement processing - will be handled with parent context
        # This avoids creating wrapper containers and respects parent layout
        deferredIfStmts.add(node)
      else:
        childNodes.add(node)

  # Generate initialization statements using C core API
  var initStmts = newTree(nnkStmtList)

  # Add standalone side-effect statements (e.g., echo) before component setup
  for stmt in sideEffectStmts:
    initStmts.add(stmt)

  # Process static blocks from the original props after initStmts is available
  for node in props.children:
    if node.kind == nnkStaticStmt:
      for staticChild in node.children:
        # Handle static block children appropriately based on their type
        if staticChild.kind == nnkStmtList:
          # Static block content is wrapped in a statement list - iterate through it
          for stmt in staticChild.children:
            if stmt.kind == nnkForStmt:
              # Handle for loop from statement list - force value capture for reactive expressions
              # CRITICAL FIX: Use immediately-invoked closure to force value capture
              let loopVar = stmt[0]  # The loop variable (e.g., "alignment")
              let capturedParam = genSym(nskParam, "capturedIdx")
              var innerBody = newTree(nnkStmtList)

              for bodyNode in stmt[^1].children:
                # Transform the body node to replace loop variable references with captured parameter
                let transformedBodyNode = transformLoopVariableReferences(bodyNode, loopVar, capturedParam)

                if transformedBodyNode.kind == nnkDiscardStmt and transformedBodyNode.len > 0:
                  let componentExpr = transformedBodyNode[0]
                  let childSym = genSym(nskLet, "loopChild")
                  innerBody.add(newLetStmt(childSym, componentExpr))
                  innerBody.add quote do:
                    discard kryon_component_add_child(`containerName`, `childSym`)
                elif transformedBodyNode.kind == nnkCall:
                  let childSym = genSym(nskLet, "loopChild")
                  innerBody.add(newLetStmt(childSym, transformedBodyNode))
                  innerBody.add quote do:
                    discard kryon_component_add_child(`containerName`, `childSym`)
                else:
                  # For other nodes, just add them directly
                  innerBody.add(transformedBodyNode)

              if innerBody.len > 0:
                # Create immediately-invoked closure: (proc(capturedIdx: typeof(loopVar)) = body)(loopVar)
                let closureProc = newProc(
                  name = newEmptyNode(),
                  params = [newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
                  body = innerBody,
                  procType = nnkLambda
                )
                closureProc.addPragma(ident("closure"))
                let closureCall = newCall(closureProc, loopVar)
                let newForLoop = newTree(nnkForStmt, loopVar, stmt[1], newStmtList(closureCall))
                initStmts.add(newForLoop)
            else:
              # Handle other statements from statement list
              initStmts.add(stmt)
        elif staticChild.kind == nnkDiscardStmt:
          # Static child already has discard - extract the component and add to container
          if staticChild.len > 0:
            let childExpr = staticChild[0]
            let childSym = genSym(nskLet, "staticChild")
            initStmts.add(newLetStmt(childSym, childExpr))
            initStmts.add quote do:
              discard kryon_component_add_child(`containerName`, `childSym`)
          else:
            # Discard without expression - just execute as-is
            initStmts.add(staticChild)
        elif staticChild.kind == nnkCall:
          # Direct component call in static block (e.g., Container(...))
          let childSym = genSym(nskLet, "staticChild")
          initStmts.add(newLetStmt(childSym, staticChild))
          initStmts.add quote do:
            discard kryon_component_add_child(`containerName`, `childSym`)
        else:
          # For control flow statements (for loops, if statements, etc.),
          # transform them to capture component results and add to parent
          if staticChild.kind == nnkForStmt:
            # Transform for loops to capture component results and add them to parent
            # CRITICAL FIX: Wrap loop body in immediately-invoked closure to force value capture
            # Without this, all closures share the same loop variable and see the final value
            let loopVar = staticChild[0]
            let capturedParam = genSym(nskParam, "capturedIdx")
            var innerBody = newTree(nnkStmtList)

            # Process the body of the for loop
            for bodyNode in staticChild[^1].children:
              # Transform the body node to replace loop variable references with captured parameter
              let transformedBodyNode = transformLoopVariableReferences(bodyNode, loopVar, capturedParam)

              if transformedBodyNode.kind == nnkDiscardStmt and transformedBodyNode.len > 0:
                # Extract component from discard statement
                let componentExpr = transformedBodyNode[0]
                let childSym = genSym(nskLet, "loopChild")
                innerBody.add(newLetStmt(childSym, componentExpr))
                innerBody.add quote do:
                  let added = kryon_component_add_child(`containerName`, `childSym`)
                  echo "[kryon][static] Added child to container: ", `containerName`, " -> ", `childSym`, " success: ", added
              elif transformedBodyNode.kind == nnkCall:
                # Direct component call
                let childSym = genSym(nskLet, "loopChild")
                innerBody.add(newLetStmt(childSym, transformedBodyNode))
                innerBody.add quote do:
                  let added = kryon_component_add_child(`containerName`, `childSym`)
                  echo "[kryon][static] Added direct child to container: ", `containerName`, " -> ", `childSym`, " success: ", added
              else:
                # Other statements - just add as-is
                innerBody.add(transformedBodyNode)

            # Create immediately-invoked closure: (proc(capturedIdx: typeof(i)) = body)(i)
            # This ensures each iteration captures its own copy of the loop variable
            if innerBody.len > 0:
              let loopRange = staticChild[1]
              # Create closure proc type with parameter matching the loop variable type
              let closureProc = newProc(
                name = newEmptyNode(),
                params = [newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
                body = innerBody,
                procType = nnkLambda
              )
              closureProc.addPragma(ident("closure"))

              # Create call to closure with loop variable as argument
              let closureCall = newCall(closureProc, loopVar)

              # Recreate the for loop with closure call as body
              let newForLoop = newTree(nnkForStmt, loopVar, loopRange, newStmtList(closureCall))
              initStmts.add(newForLoop)
            else:
              # If no transformable content, add original
              initStmts.add(staticChild)
          else:
            # Other control flow statements - add as-is for now
            initStmts.add(staticChild)

  # Set layout direction first (most important)
  initStmts.add quote do:
    kryon_component_set_layout_direction(`containerName`, int(`layoutDirectionVal`))

  # Set layout alignment
  initStmts.add quote do:
    kryon_component_set_layout_alignment(`containerName`,
      `justifyContentVal`, `alignItemsVal`)

  # Set gap (parse string or extract .px suffix)
  let gapParsed = parseUnitString(gapVal)
  let gapExpr = if gapParsed.numericValue.kind != nnkIntLit or gapParsed.numericValue.intVal != 0:
    gapParsed.numericValue  # Use parsed string value
  elif isPxExpression(gapVal):
    extractPxValue(gapVal)
  else:
    gapVal

  initStmts.add quote do:
    kryon_component_set_gap(`containerName`, uint8(`gapExpr`))

  # Apply style if specified
  if styleName != nil:
    if styleData != nil:
      initStmts.add quote do:
        `styleName`(`containerName`, `styleData`)
    else:
      initStmts.add quote do:
        `styleName`(`containerName`)

  # Set position and size
  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)

  # Parse string values and check for unit suffixes
  # Skip parsing for 'auto' identifier
  let widthParsed = if widthVal != nil and widthVal.kind != nnkIdent:
    parseUnitString(widthVal)
  else:
    (newIntLitNode(0), false, false)
  let heightParsed = if heightVal != nil and heightVal.kind != nnkIdent:
    parseUnitString(heightVal)
  else:
    (newIntLitNode(0), false, false)

  # Check if values are 'auto' identifier
  let widthIsAuto = widthVal != nil and widthVal.kind == nnkIdent and widthVal.strVal == "auto"
  let heightIsAuto = heightVal != nil and heightVal.kind == nnkIdent and heightVal.strVal == "auto"

  let widthIsPercent = (widthVal != nil and isPercentExpression(widthVal)) or widthParsed.isPercent
  let heightIsPercent = (heightVal != nil and isPercentExpression(heightVal)) or heightParsed.isPercent
  let widthIsPx = (widthVal != nil and isPxExpression(widthVal)) or widthParsed.isPx
  let heightIsPx = (heightVal != nil and isPxExpression(heightVal)) or heightParsed.isPx

  let wExpr = if widthVal != nil:
    if widthIsAuto:
      newIntLitNode(0)  # Auto dimension uses 0 for value
    elif widthParsed.numericValue.kind != nnkIntLit or widthParsed.numericValue.intVal != 0:
      widthParsed.numericValue  # Use parsed string value
    elif widthIsPercent:
      extractPercentValue(widthVal)
    elif widthIsPx:
      extractPxValue(widthVal)
    else:
      widthVal
  else:
    newIntLitNode(0)

  let hExpr = if heightVal != nil:
    if heightIsAuto:
      newIntLitNode(0)  # Auto dimension uses 0 for value
    elif heightParsed.numericValue.kind != nnkIntLit or heightParsed.numericValue.intVal != 0:
      heightParsed.numericValue  # Use parsed string value
    elif heightIsPercent:
      extractPercentValue(heightVal)
    elif heightIsPx:
      extractPxValue(heightVal)
    else:
      heightVal
  else:
    newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08

  # Use dimension-typed setter if any dimension uses percent or auto
  if widthIsPercent or heightIsPercent or widthIsAuto or heightIsAuto:
    let widthType = if widthIsAuto:
      bindSym("IR_DIMENSION_AUTO")
    elif widthIsPercent:
      bindSym("IR_DIMENSION_PERCENT")
    else:
      bindSym("IR_DIMENSION_PX")

    let heightType = if heightIsAuto:
      bindSym("IR_DIMENSION_AUTO")
    elif heightIsPercent:
      bindSym("IR_DIMENSION_PERCENT")
    else:
      bindSym("IR_DIMENSION_PX")

    initStmts.add quote do:
      kryon_component_set_bounds_with_types(`containerName`,
        toFixed(`xExpr`), toFixed(`yExpr`), toFixed(`wExpr`), toFixed(`hExpr`),
        `widthType`, `heightType`, uint8(`maskVal`))
  else:
    initStmts.add quote do:
      kryon_component_set_bounds_mask(`containerName`,
        toFixed(`xExpr`), toFixed(`yExpr`), toFixed(`wExpr`), toFixed(`hExpr`), uint8(`maskVal`))

  # Set padding (support string format, tuple with .px, and individual values)
  var padTop, padRight, padBottom, padLeft: NimNode

  # Check if paddingAll is a string like "20px, 20px, 20px, 20px"
  if paddingAll != nil and paddingAll.kind == nnkStrLit:
    let parsed = parseQuadString(paddingAll)
    padTop = parsed.top
    padRight = parsed.right
    padBottom = parsed.bottom
    padLeft = parsed.left
  # Check if paddingAll is a tuple like (12.px, 24.px, 12.px, 24.px)
  elif paddingAll != nil and (paddingAll.kind == nnkTupleConstr or paddingAll.kind == nnkPar):
    # Extract values from tuple and handle .px suffixes
    let top = if paddingAll.len >= 1: paddingAll[0] else: newIntLitNode(0)
    let right = if paddingAll.len >= 2: paddingAll[1] else: newIntLitNode(0)
    let bottom = if paddingAll.len >= 3: paddingAll[2] else: newIntLitNode(0)
    let left = if paddingAll.len >= 4: paddingAll[3] else: newIntLitNode(0)
    padTop = if isPxExpression(top): extractPxValue(top) else: top
    padRight = if isPxExpression(right): extractPxValue(right) else: right
    padBottom = if isPxExpression(bottom): extractPxValue(bottom) else: bottom
    padLeft = if isPxExpression(left): extractPxValue(left) else: left
  else:
    # Individual values or single paddingAll value
    let allVal = if paddingAll != nil:
      (if isPxExpression(paddingAll): extractPxValue(paddingAll) else: paddingAll)
    else: newIntLitNode(0)

    padTop = if paddingTopVal != nil:
      (if isPxExpression(paddingTopVal): extractPxValue(paddingTopVal) else: paddingTopVal)
    else: allVal
    padRight = if paddingRightVal != nil:
      (if isPxExpression(paddingRightVal): extractPxValue(paddingRightVal) else: paddingRightVal)
    else: allVal
    padBottom = if paddingBottomVal != nil:
      (if isPxExpression(paddingBottomVal): extractPxValue(paddingBottomVal) else: paddingBottomVal)
    else: allVal
    padLeft = if paddingLeftVal != nil:
      (if isPxExpression(paddingLeftVal): extractPxValue(paddingLeftVal) else: paddingLeftVal)
    else: allVal

  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      let style = ir_get_style(`containerName`)
      if style.isNil:
        let newStyle = ir_create_style()
        ir_set_padding(newStyle,
          float32(`padTop`), float32(`padRight`),
          float32(`padBottom`), float32(`padLeft`))
        ir_set_style(`containerName`, newStyle)
      else:
        ir_set_padding(style,
          float32(`padTop`), float32(`padRight`),
          float32(`padBottom`), float32(`padLeft`))

      # Invalidate layout cache after modifying padding
      kryon_component_mark_dirty(`containerName`)

  # Set margin
  let marginTop = if marginTopVal != nil: marginTopVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginRight = if marginRightVal != nil: marginRightVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginBottom = if marginBottomVal != nil: marginBottomVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginLeft = if marginLeftVal != nil: marginLeftVal elif marginAll != nil: marginAll else: newIntLitNode(0)

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`containerName`,
        uint8(`marginTop`), uint8(`marginRight`), uint8(`marginBottom`), uint8(`marginLeft`))

  # Set visual properties
  if bgColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`containerName`, `bgColorVal`)

  # Set background gradient
  if bgGradientVal != nil:
    let gradientSpec = parseGradientSpec(bgGradientVal)
    let gradientVar = genSym(nskLet, "gradient")
    let gradientCode = buildGradientCreation(gradientSpec, gradientVar)
    initStmts.add(gradientCode)
    initStmts.add quote do:
      kryon_component_set_background_gradient(`containerName`, `gradientVar`)

  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`containerName`, `textColorVal`)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`containerName`, `borderColorVal`)

  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`containerName`, uint8(`borderWidthVal`))

  if borderRadiusVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_radius(`containerName`, uint8(`borderRadiusVal`))

  if boxShadowVal != nil:
    # Support both named parameter syntax and positional tuple syntax
    # Check if it's a named tuple by looking at the first child's kind
    let isNamedTuple = (boxShadowVal.kind == nnkTableConstr) or
                       ((boxShadowVal.kind == nnkTupleConstr or boxShadowVal.kind == nnkPar) and
                        boxShadowVal.len > 0 and boxShadowVal[0].kind == nnkExprColonExpr)

    if isNamedTuple:
      # Named parameter syntax: (offsetY: 8.0, blur: 24.0, color: "#000")
      let spec = parseBoxShadowSpec(boxShadowVal)
      let shadowCall = buildBoxShadowCreation(containerName, spec)
      initStmts.add(shadowCall)
    elif boxShadowVal.kind == nnkTupleConstr or boxShadowVal.kind == nnkPar:
      # Positional tuple syntax (backward compatible): (offsetX, offsetY, blur, spread, color) or (offsetX, offsetY, blur, spread, color, inset)
      let offsetX = boxShadowVal[0]
      let offsetY = boxShadowVal[1]
      let blurRadius = boxShadowVal[2]
      let spreadRadius = boxShadowVal[3]
      let shadowColor = colorNode(boxShadowVal[4])
      let inset = if boxShadowVal.len >= 6: boxShadowVal[5] else: newLit(false)
      initStmts.add quote do:
        kryon_component_set_box_shadow(`containerName`, cfloat(`offsetX`), cfloat(`offsetY`), cfloat(`blurRadius`), cfloat(`spreadRadius`), `shadowColor`, `inset`)

  # CSS Filters
  if blurVal != nil:
    initStmts.add quote do:
      kryon_component_set_blur(`containerName`, cfloat(`blurVal`))

  if brightnessVal != nil:
    initStmts.add quote do:
      kryon_component_set_brightness(`containerName`, cfloat(`brightnessVal`))

  if contrastVal != nil:
    initStmts.add quote do:
      kryon_component_set_contrast(`containerName`, cfloat(`contrastVal`))

  if grayscaleVal != nil:
    initStmts.add quote do:
      kryon_component_set_grayscale(`containerName`, cfloat(`grayscaleVal`))

  if hueRotateVal != nil:
    initStmts.add quote do:
      kryon_component_set_hue_rotate(`containerName`, cfloat(`hueRotateVal`))

  if invertVal != nil:
    initStmts.add quote do:
      kryon_component_set_invert(`containerName`, cfloat(`invertVal`))

  if filterOpacityVal != nil:
    initStmts.add quote do:
      kryon_component_set_filter_opacity(`containerName`, cfloat(`filterOpacityVal`))

  if saturateVal != nil:
    initStmts.add quote do:
      kryon_component_set_saturate(`containerName`, cfloat(`saturateVal`))

  if sepiaVal != nil:
    initStmts.add quote do:
      kryon_component_set_sepia(`containerName`, cfloat(`sepiaVal`))

  if zIndexVal != nil:
    initStmts.add quote do:
      kryon_component_set_z_index(`containerName`, uint16(`zIndexVal`))

  # Set flex properties
  if flexGrowVal != nil or flexShrinkVal != nil:
    let growExpr = if flexGrowVal != nil: flexGrowVal else: newIntLitNode(0)
    let shrinkExpr = if flexShrinkVal != nil: flexShrinkVal else: newIntLitNode(1)
    initStmts.add quote do:
      kryon_component_set_flex(`containerName`, uint8(`growExpr`), uint8(`shrinkExpr`))

  # Apply min/max width/height constraints via layout
  if minWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_min_width(layout, `dimType`, cfloat(`minWidthVal`))

  if maxWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_max_width(layout, `dimType`, cfloat(`maxWidthVal`))

  if minHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_min_height(layout, `dimType`, cfloat(`minHeightVal`))

  if maxHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_max_height(layout, `dimType`, cfloat(`maxHeightVal`))

  # Apply aspect ratio constraint
  if aspectRatioVal != nil:
    initStmts.add quote do:
      let layout = ir_get_layout(`containerName`)
      if layout != nil:
        ir_set_aspect_ratio(layout, cfloat(`aspectRatioVal`))

  # Apply grid placement (for when this container is a child of a Grid)
  var needsGridPlacement = false
  var rowStart, rowEnd, colStart, colEnd: NimNode

  # Parse gridRow and gridColumn if provided
  if gridRowVal != nil:
    let (start, endVal) = parseGridLine(gridRowVal)
    rowStart = start
    rowEnd = endVal
    needsGridPlacement = true
  if gridColumnVal != nil:
    let (start, endVal) = parseGridLine(gridColumnVal)
    colStart = start
    colEnd = endVal
    needsGridPlacement = true

  # Override with individual values if provided
  if gridRowStartVal != nil:
    rowStart = gridRowStartVal
    needsGridPlacement = true
  if gridRowEndVal != nil:
    rowEnd = gridRowEndVal
    needsGridPlacement = true
  if gridColumnStartVal != nil:
    colStart = gridColumnStartVal
    needsGridPlacement = true
  if gridColumnEndVal != nil:
    colEnd = gridColumnEndVal
    needsGridPlacement = true

  # Set default values if needed
  if needsGridPlacement:
    if rowStart == nil: rowStart = newLit(int16(1))
    if rowEnd == nil: rowEnd = newLit(int16(-1))
    if colStart == nil: colStart = newLit(int16(1))
    if colEnd == nil: colEnd = newLit(int16(-1))

    initStmts.add quote do:
      kryon_component_set_grid_item_placement(`containerName`, int16(`rowStart`), int16(`rowEnd`), int16(`colStart`), int16(`colEnd`))

  # Apply grid item alignment
  if justifySelfVal != nil or alignSelfVal != nil:
    let justifySelf = if justifySelfVal != nil: justifySelfVal else: alignmentNode("auto")
    let alignSelf = if alignSelfVal != nil: alignSelfVal else: alignmentNode("auto")
    initStmts.add quote do:
      kryon_component_set_grid_item_alignment(`containerName`, `justifySelf`, `alignSelf`)

  # Apply transitions
  if transitionOpacityVal != nil:
    initStmts.add quote do:
      kryon_component_add_opacity_transition(`containerName`, cfloat(`transitionOpacityVal`))

  if transitionTransformVal != nil:
    initStmts.add quote do:
      kryon_component_add_transform_transition(`containerName`, cfloat(`transitionTransformVal`))

  if transitionSizeVal != nil:
    initStmts.add quote do:
      kryon_component_add_size_transition(`containerName`, cfloat(`transitionSizeVal`))

  if transitionColorVal != nil:
    initStmts.add quote do:
      kryon_component_add_color_transition(`containerName`, cfloat(`transitionColorVal`))

  # Apply container query settings
  if containerTypeVal != nil:
    initStmts.add quote do:
      kryon_component_set_container_type(`containerName`, `containerTypeVal`)

  if containerNameVal != nil:
    initStmts.add quote do:
      kryon_component_set_container_name(`containerName`, `containerNameVal`)

  # Apply breakpoints
  var breakpointIndex = 0
  for breakpointVal in breakpointVals:
    # Parse breakpoint tuple: (minWidth: 600, width: 50.pct)
    # Expected format: named tuple with optional fields
    if breakpointVal.kind == nnkTupleConstr or breakpointVal.kind == nnkPar:
      var minWidthVal: NimNode = newLit(-1.0)
      var maxWidthVal: NimNode = newLit(-1.0)
      var minHeightVal: NimNode = newLit(-1.0)
      var maxHeightVal: NimNode = newLit(-1.0)
      var widthVal: NimNode = newLit(-1.0)
      var heightVal: NimNode = newLit(-1.0)
      var visibleVal: NimNode = newLit(true)
      var opacityVal: NimNode = newLit(-1.0)

      # Parse each field in the tuple
      for field in breakpointVal:
        if field.kind == nnkExprColonExpr:
          let fieldName = $field[0]
          let fieldValue = field[1]
          case fieldName.toLowerAscii():
          of "minwidth":
            minWidthVal = fieldValue
          of "maxwidth":
            maxWidthVal = fieldValue
          of "minheight":
            minHeightVal = fieldValue
          of "maxheight":
            maxHeightVal = fieldValue
          of "width":
            widthVal = fieldValue
          of "height":
            heightVal = fieldValue
          of "visible":
            visibleVal = fieldValue
          of "opacity":
            opacityVal = fieldValue
          else:
            discard

      # Generate call to add breakpoint with index
      let bpIndexLit = newLit(uint8(breakpointIndex))
      initStmts.add quote do:
        kryon_component_add_breakpoint(`containerName`,
          breakpoint_index = `bpIndexLit`,
          min_width = float(`minWidthVal`),
          max_width = float(`maxWidthVal`),
          min_height = float(`minHeightVal`),
          max_height = float(`maxHeightVal`),
          width = float(`widthVal`),
          height = float(`heightVal`),
          visible = `visibleVal`,
          opacity = float(`opacityVal`))

      breakpointIndex.inc

  # Apply animation
  if animationVal != nil:
    var animCallNode = animationVal
    var useRuntimeParser = false

    # Check if it's a string literal
    if animationVal.kind == nnkStrLit:
      # Direct string literal: animation = "pulse(2.0, -1)"
      animCallNode = parseAnimationString(animationVal.strVal)
    elif animationVal.kind in {nnkDotExpr, nnkBracketExpr, nnkIdent}:
      # Const field access: animation = config.anim
      # Generate code to call runtime string parser with the const value
      # The value will be evaluated in the static block
      initStmts.add quote do:
        kryon_apply_animation_from_string(`containerName`, `animationVal`)
      useRuntimeParser = true

    # animation value should be a call like pulse(2.0) or fadeInOut(1.5)
    # Skip this if we're using the runtime parser for const field access
    if not useRuntimeParser and animCallNode.kind == nnkCall:
      let animFunc = animCallNode[0]
      let animFuncName = $animFunc

      # Generate call to create animation
      case animFuncName.toLowerAscii():
      of "pulse":
        # pulse(duration) or pulse(duration, iterations)
        if animCallNode.len == 2:
          let duration = animCallNode[1]
          initStmts.add quote do:
            let anim = kryon_animation_pulse(cfloat(`duration`))
            kryon_component_add_animation(`containerName`, anim)
        elif animCallNode.len == 3:
          let duration = animCallNode[1]
          let iterations = animCallNode[2]
          initStmts.add quote do:
            let anim = kryon_animation_pulse(cfloat(`duration`), int32(`iterations`))
            kryon_component_add_animation(`containerName`, anim)

      of "fadeinout", "fade_in_out":
        # fadeInOut(duration) or fadeInOut(duration, iterations)
        if animCallNode.len == 2:
          let duration = animCallNode[1]
          initStmts.add quote do:
            let anim = kryon_animation_fade_in_out(cfloat(`duration`))
            kryon_component_add_animation(`containerName`, anim)
        elif animCallNode.len == 3:
          let duration = animCallNode[1]
          let iterations = animCallNode[2]
          initStmts.add quote do:
            let anim = kryon_animation_fade_in_out(cfloat(`duration`), int32(`iterations`))
            kryon_component_add_animation(`containerName`, anim)

      of "slideinleft", "slide_in_left":
        # slideInLeft(duration)
        if animCallNode.len == 2:
          let duration = animCallNode[1]
          initStmts.add quote do:
            let anim = kryon_animation_slide_in_left(cfloat(`duration`))
            kryon_component_add_animation(`containerName`, anim)

      else:
        discard  # Unknown animation function

  # Add child components
  for child in childNodes:
    # For-loops: distinguish static vs dynamic collections
    if child.kind == nnkForStmt:
      let loopCollection = child[1]

      # Static collections (array literals, seq literals, ranges) get compile-time unrolled
      if isStaticCollection(loopCollection):
        proc buildStaticForLoop(forNode: NimNode): NimNode =
          ## Generate a compile-time for loop that directly adds children to container
          let loopVar = forNode[0]
          let collection = forNode[1]
          let loopBody = forNode[2]

          # Use immediately-invoked closure to capture loop variable value
          let capturedParam = genSym(nskParam, "captured")

          var innerBody = newTree(nnkStmtList)
          for bodyNode in loopBody.children:
            let transformed = transformLoopVariableReferences(bodyNode, loopVar, capturedParam)
            case transformed.kind
            of nnkLetSection, nnkVarSection, nnkConstSection:
              innerBody.add(transformed)
            of nnkDiscardStmt:
              # Discarded component - still add to container
              if transformed.len > 0:
                let childSym = genSym(nskLet, "child")
                innerBody.add(newLetStmt(childSym, transformed[0]))
                innerBody.add quote do:
                  discard kryon_component_add_child(`containerName`, `childSym`)
              else:
                innerBody.add(transformed)
            else:
              # Component expression - add to container
              let childSym = genSym(nskLet, "child")
              innerBody.add(newLetStmt(childSym, transformed))
              innerBody.add quote do:
                discard kryon_component_add_child(`containerName`, `childSym`)

          # Create closure and call it with loop variable
          let closureProc = newProc(
            name = newEmptyNode(),
            params = @[newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
            body = innerBody,
            procType = nnkLambda
          )
          closureProc.addPragma(ident("closure"))
          let closureCall = newCall(closureProc, loopVar)

          # Return the for loop with closure call as body
          result = newTree(nnkForStmt, loopVar, collection, newStmtList(closureCall))

        initStmts.add(buildStaticForLoop(child))
        continue

      # Dynamic collections use reactive for-loop system
      proc buildReactiveForLoop(forNode: NimNode): NimNode =
        let loopVar = forNode[0]
        let collection = forNode[1]
        let loopBody = forNode[2]

        # Use a parameter for captured value in the item proc
        let itemParam = genSym(nskParam, "item")
        let indexParam = genSym(nskParam, "idx")

        # Build the item creation proc body - must return a component
        var itemBody = newTree(nnkStmtList)
        var lastComponentExpr: NimNode = nil

        for bodyNode in loopBody.children:
          # Transform the body node to replace loop variable references with item parameter
          let transformedBodyNode = transformLoopVariableReferences(bodyNode, loopVar, itemParam)

          case transformedBodyNode.kind
          of nnkLetSection, nnkVarSection, nnkConstSection:
            itemBody.add(transformedBodyNode)
          of nnkDiscardStmt:
            if transformedBodyNode.len > 0:
              lastComponentExpr = transformedBodyNode[0]
            else:
              itemBody.add(transformedBodyNode)
          else:
            # This should be a component expression - save it as the return value
            lastComponentExpr = transformedBodyNode

        # Add the return expression (the component)
        if lastComponentExpr != nil:
          itemBody.add(lastComponentExpr)
        else:
          # Fallback - return nil if no component found
          itemBody.add(newNilLit())

        # Generate the createReactiveForLoop call
        # collectionProc: returns the current collection
        # itemProc: creates a component from an item
        let collectionProcSym = genSym(nskProc, "collectionProc")
        let itemProcSym = genSym(nskProc, "itemProc")

        # Determine element type from collection
        let elemType = newCall(ident("typeof"), newTree(nnkBracketExpr, collection, newLit(0)))

        result = quote do:
          block:
            proc `collectionProcSym`(): seq[`elemType`] {.closure.} =
              result = `collection`
            proc `itemProcSym`(`itemParam`: `elemType`, `indexParam`: int): ptr IRComponent {.closure.} =
              `itemBody`
            discard createReactiveForLoop(`containerName`, `collectionProcSym`, `itemProcSym`)

      initStmts.add(buildReactiveForLoop(child))
      continue

    # Handle both discarded and non-discarded children
    if child.kind == nnkDiscardStmt:
      # Child already has discard - just execute it directly
      # The user took care of discarding, so we just need to add it to container
      # But first, we need to extract the actual component from the discard
      if child.len > 0:
        let childExpr = child[0]
        let childSym = genSym(nskLet, "child")
        initStmts.add(newLetStmt(childSym, childExpr))
        initStmts.add quote do:
          discard kryon_component_add_child(`containerName`, `childSym`)
      else:
        # Discard without expression - just execute as-is
        initStmts.add(child)
    else:
      # Regular child component - create symbol and add to container
      let childSym = genSym(nskLet, "child")
      initStmts.add quote do:
        let `childSym` = block:
          `child`
        discard kryon_component_add_child(`containerName`, `childSym`)

  # Process deferred if statements - create reactive conditionals with parent context
  for ifStmt in deferredIfStmts:
    # Extract condition and branches from if statement
    # nnkIfStmt structure: [condition, thenBranch, (elifBranch | elseBranch)...]
    var conditionExpr: NimNode
    var thenBody: NimNode
    var elseBody: NimNode = nil

    # Process the if statement branches
    for i in 0..<ifStmt.len:
      let branch = ifStmt[i]
      if branch.kind == nnkElifBranch:
        if i == 0:
          # First elif is the main if condition
          conditionExpr = branch[0]
          thenBody = branch[1]
        else:
          # Subsequent elif branches - chain into else
          if elseBody == nil:
            elseBody = newTree(nnkIfStmt)
          elseBody.add(branch)
      elif branch.kind == nnkElse:
        if elseBody != nil and elseBody.kind == nnkIfStmt:
          elseBody.add(branch)
        else:
          elseBody = branch[0]

    # Build closures for condition and branches
    # CRITICAL: Use nskLet since we create lambda expressions with let bindings
    let condProc = genSym(nskLet, "conditionProc")
    let thenProc = genSym(nskLet, "thenProc")
    let elseProc = genSym(nskLet, "elseProc")

    # Helper to build component array from branch body
    proc buildComponentSeq(body: NimNode): NimNode =
      var elements = newNimNode(nnkBracket)
      if body.kind == nnkStmtList:
        for child in body:
          if child.kind notin {nnkCommentStmt, nnkDiscardStmt}:
            elements.add(quote do:
              block:
                `child`)
      else:
        elements.add(quote do:
          block:
            `body`)
      newCall(ident("@"), elements)

    # CRITICAL FIX: Use let bindings with lambdas instead of proc definitions
    # This ensures proper closure capture of loop variables in reactive conditionals
    # With nnkProcDef, Nim may not properly capture the closure environment
    # when variables like capturedLoopVar are referenced inside the proc body

    let thenBodySeq = buildComponentSeq(thenBody)

    # Generate condition and then procs as lambda expressions
    initStmts.add quote do:
      let `condProc` = proc(): bool {.closure.} = `conditionExpr`
      let `thenProc` = proc(): seq[KryonComponent] {.closure.} = `thenBodySeq`

    # Generate else proc (may be nil)
    if elseBody != nil:
      let elseBodySeq = if elseBody.kind == nnkIfStmt:
        # Nested if-elif chain - wrap in closure
        quote do:
          @[block:
            `elseBody`]
      else:
        buildComponentSeq(elseBody)

      initStmts.add quote do:
        let `elseProc` = proc(): seq[KryonComponent] {.closure.} = `elseBodySeq`

      # Register reactive conditional with else
      initStmts.add quote do:
        createReactiveConditional(`containerName`, `condProc`, `thenProc`, `elseProc`)
    else:
      # Register reactive conditional without else
      initStmts.add quote do:
        createReactiveConditional(`containerName`, `condProc`, `thenProc`, nil)

  # Check if this container has any static blocks, and if so, finalize the subtree
  # This must happen AFTER all properties (including animations) have been set
  # This ensures all post-construction propagation steps are performed (e.g., animation flags)
  var hasStaticBlocks = false
  for node in props.children:
    if node.kind == nnkStaticStmt:
      hasStaticBlocks = true
      break

  if hasStaticBlocks:
    # Finalize the subtree to ensure all propagation happens
    initStmts.add quote do:
      ir_component_finalize_subtree(`containerName`)

  # Mark container as dirty
  initStmts.add quote do:
    kryon_component_mark_dirty(`containerName`)

  # Generate the final result
  # Always return the component - the calling context should handle it properly
  # Use IR_COMPONENT_ROW or IR_COMPONENT_COLUMN instead of CONTAINER based on layoutDirection
  let componentCreation =
    if layoutDirectionVal.kind == nnkIntLit:
      if layoutDirectionVal.intVal == 0:
        # Column layout (vertical)
        quote do: newKryonColumn()
      elif layoutDirectionVal.intVal == 1:
        # Row layout (horizontal)
        quote do: newKryonRow()
      else:
        # Default to container
        quote do: newKryonContainer()
    else:
      # Runtime value - default to container
      quote do: newKryonContainer()

  result = quote do:
    block:
      let `containerName` = `componentCreation`
      `initStmts`
      `containerName`

macro Grid*(props: untyped): untyped =
  ## Grid container component using CSS Grid layout
  var
    gridName = genSym(nskLet, "grid")
    childNodes: seq[NimNode] = @[]
    sideEffectStmts: seq[NimNode] = @[]
    deferredIfStmts: seq[NimNode] = @[]
    # Grid-specific properties
    gridTemplateRowsVal: NimNode = nil
    gridTemplateColumnsVal: NimNode = nil
    gridGapVal: NimNode = nil
    gridRowGapVal: NimNode = nil
    gridColumnGapVal: NimNode = nil
    gridAutoFlowVal: NimNode = nil
    gridJustifyItemsVal: NimNode = nil
    gridAlignItemsVal: NimNode = nil
    gridJustifyContentVal: NimNode = nil
    gridAlignContentVal: NimNode = nil
    # Common properties
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    borderRadiusVal: NimNode = nil
    boxShadowVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    minWidthVal: NimNode = nil
    maxWidthVal: NimNode = nil
    minHeightVal: NimNode = nil
    maxHeightVal: NimNode = nil
    zIndexVal: NimNode = nil
    paddingAll: NimNode = nil
    paddingTopVal: NimNode = nil
    paddingRightVal: NimNode = nil
    paddingBottomVal: NimNode = nil
    paddingLeftVal: NimNode = nil
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil

  # Parse properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      # Evaluate const expressions (e.g., config.field) to their literal values
      var value = evalConstExpr(node[1])

      case propName.toLowerAscii():
      # Grid-specific properties
      of "gridtemplaterows", "templaterows", "rows":
        gridTemplateRowsVal = parseGridTemplate(value)
      of "gridtemplatecolumns", "templatecolumns", "columns":
        gridTemplateColumnsVal = parseGridTemplate(value)
      of "gridgap":
        gridGapVal = value
      of "gridrowgap", "rowgap":
        gridRowGapVal = value
      of "gridcolumngap", "columngap":
        gridColumnGapVal = value
      of "gridautoflow", "autoflow":
        gridAutoFlowVal = value
      of "gridjustifyitems", "justifyitems":
        gridJustifyItemsVal = parseAlignmentValue(value)
      of "gridalignitems", "alignitems":
        gridAlignItemsVal = parseAlignmentValue(value)
      of "gridjustifycontent", "justifycontent":
        gridJustifyContentVal = parseAlignmentValue(value)
      of "gridaligncontent", "aligncontent":
        gridAlignContentVal = parseAlignmentValue(value)
      of "gap":
        # Support both single value and tuple (rowGap, columnGap)
        if value.kind == nnkTupleConstr or value.kind == nnkPar:
          gridRowGapVal = value[0]
          gridColumnGapVal = if value.len > 1: value[1] else: value[0]
        else:
          gridGapVal = value
      # Common properties
      of "backgroundcolor", "bg", "background":
        bgColorVal = colorNode(value)
      of "color", "textcolor":
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "borderradius":
        borderRadiusVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "width", "w":
        widthVal = value
      of "height", "h":
        heightVal = value
      of "minwidth":
        minWidthVal = value
      of "maxwidth":
        maxWidthVal = value
      of "minheight":
        minHeightVal = value
      of "maxheight":
        maxHeightVal = value
      of "zindex", "z":
        zIndexVal = value
      of "padding", "p":
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "margin", "m":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "boxshadow":
        boxShadowVal = value
      else:
        discard
    else:
      if isEchoStatement(node):
        sideEffectStmts.add(node)
      elif node.kind in {nnkLetSection, nnkVarSection, nnkConstSection}:
        sideEffectStmts.add(node)
      elif node.kind == nnkIfStmt:
        deferredIfStmts.add(node)
      else:
        childNodes.add(node)

  # Generate initialization statements
  var initStmts = newTree(nnkStmtList)

  # Add side-effect statements
  for stmt in sideEffectStmts:
    initStmts.add(stmt)

  # Note: ir_set_layout_mode not yet implemented in C core
  # The layout mode should be inferred when grid properties are set

  # Set grid template rows
  if gridTemplateRowsVal != nil:
    initStmts.add quote do:
      kryon_component_set_grid_template_rows(`gridName`, `gridTemplateRowsVal`)

  # Set grid template columns
  if gridTemplateColumnsVal != nil:
    initStmts.add quote do:
      kryon_component_set_grid_template_columns(`gridName`, `gridTemplateColumnsVal`)

  # Set grid gap
  if gridGapVal != nil:
    initStmts.add quote do:
      kryon_component_set_grid_gap(`gridName`, cfloat(`gridGapVal`), cfloat(`gridGapVal`))
  elif gridRowGapVal != nil or gridColumnGapVal != nil:
    let rowGap = if gridRowGapVal != nil: gridRowGapVal else: newLit(0)
    let colGap = if gridColumnGapVal != nil: gridColumnGapVal else: newLit(0)
    initStmts.add quote do:
      kryon_component_set_grid_gap(`gridName`, cfloat(`rowGap`), cfloat(`colGap`))

  # Set grid auto-flow
  if gridAutoFlowVal != nil:
    # Parse "row", "column", "row dense", "column dense"
    if gridAutoFlowVal.kind == nnkStrLit:
      let flowStr = gridAutoFlowVal.strVal.toLowerAscii()
      let isRow = flowStr.contains("row") or not flowStr.contains("column")
      let isDense = flowStr.contains("dense")
      initStmts.add quote do:
        kryon_component_set_grid_auto_flow(`gridName`, `isRow`, `isDense`)
    else:
      # Assume boolean for row direction
      initStmts.add quote do:
        kryon_component_set_grid_auto_flow(`gridName`, `gridAutoFlowVal`, false)

  # Set grid alignment
  let justifyItems = if gridJustifyItemsVal != nil: gridJustifyItemsVal else: alignmentNode("stretch")
  let alignItems = if gridAlignItemsVal != nil: gridAlignItemsVal else: alignmentNode("stretch")
  let justifyContent = if gridJustifyContentVal != nil: gridJustifyContentVal else: alignmentNode("start")
  let alignContent = if gridAlignContentVal != nil: gridAlignContentVal else: alignmentNode("start")

  initStmts.add quote do:
    kryon_component_set_grid_alignment(`gridName`, `justifyItems`, `alignItems`, `justifyContent`, `alignContent`)

  # Set position and size
  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)

  let widthIsPercent = widthVal != nil and isPercentExpression(widthVal)
  let heightIsPercent = heightVal != nil and isPercentExpression(heightVal)

  let wExpr = if widthVal != nil:
    if widthIsPercent: extractPercentValue(widthVal) else: widthVal
  else:
    newIntLitNode(0)
  let hExpr = if heightVal != nil:
    if heightIsPercent: extractPercentValue(heightVal) else: heightVal
  else:
    newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08

  if widthIsPercent or heightIsPercent:
    let widthType = if widthIsPercent: bindSym("IR_DIMENSION_PERCENT") else: bindSym("IR_DIMENSION_PX")
    let heightType = if heightIsPercent: bindSym("IR_DIMENSION_PERCENT") else: bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      kryon_component_set_bounds_with_types(`gridName`,
        cfloat(`xExpr`), cfloat(`yExpr`), cfloat(`wExpr`), cfloat(`hExpr`),
        `widthType`, `heightType`, uint8(`maskVal`))
  else:
    initStmts.add quote do:
      kryon_component_set_bounds_mask(`gridName`,
        cfloat(`xExpr`), cfloat(`yExpr`), cfloat(`wExpr`), cfloat(`hExpr`), uint8(`maskVal`))

  # Set padding
  let padTop = if paddingTopVal != nil: paddingTopVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)
  let padRight = if paddingRightVal != nil: paddingRightVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)
  let padBottom = if paddingBottomVal != nil: paddingBottomVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)
  let padLeft = if paddingLeftVal != nil: paddingLeftVal elif paddingAll != nil: paddingAll else: newIntLitNode(0)

  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_padding(`gridName`,
        uint8(`padTop`), uint8(`padRight`), uint8(`padBottom`), uint8(`padLeft`))

  # Set margin
  let marginTop = if marginTopVal != nil: marginTopVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginRight = if marginRightVal != nil: marginRightVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginBottom = if marginBottomVal != nil: marginBottomVal elif marginAll != nil: marginAll else: newIntLitNode(0)
  let marginLeft = if marginLeftVal != nil: marginLeftVal elif marginAll != nil: marginAll else: newIntLitNode(0)

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`gridName`,
        uint8(`marginTop`), uint8(`marginRight`), uint8(`marginBottom`), uint8(`marginLeft`))

  # Set visual properties
  if bgColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`gridName`, `bgColorVal`)

  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`gridName`, `textColorVal`)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`gridName`, `borderColorVal`)

  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`gridName`, uint8(`borderWidthVal`))

  if borderRadiusVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_radius(`gridName`, uint8(`borderRadiusVal`))

  if boxShadowVal != nil:
    # Support both named parameter syntax and positional tuple syntax
    let isNamedTuple = (boxShadowVal.kind == nnkTableConstr) or
                       ((boxShadowVal.kind == nnkTupleConstr or boxShadowVal.kind == nnkPar) and
                        boxShadowVal.len > 0 and boxShadowVal[0].kind == nnkExprColonExpr)

    if isNamedTuple:
      # Named parameter syntax: (offsetY: 8.0, blur: 24.0, color: "#000")
      let spec = parseBoxShadowSpec(boxShadowVal)
      let shadowCall = buildBoxShadowCreation(gridName, spec)
      initStmts.add(shadowCall)
    elif boxShadowVal.kind == nnkTupleConstr or boxShadowVal.kind == nnkPar:
      # Positional tuple syntax (backward compatible)
      let offsetX = boxShadowVal[0]
      let offsetY = boxShadowVal[1]
      let blurRadius = boxShadowVal[2]
      let spreadRadius = boxShadowVal[3]
      let shadowColor = colorNode(boxShadowVal[4])
      let inset = if boxShadowVal.len >= 6: boxShadowVal[5] else: newLit(false)
      initStmts.add quote do:
        kryon_component_set_box_shadow(`gridName`, cfloat(`offsetX`), cfloat(`offsetY`), cfloat(`blurRadius`), cfloat(`spreadRadius`), `shadowColor`, `inset`)

  if zIndexVal != nil:
    initStmts.add quote do:
      kryon_component_set_z_index(`gridName`, uint16(`zIndexVal`))

  # Apply min/max width/height constraints
  if minWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`gridName`)
      if layout != nil:
        ir_set_min_width(layout, `dimType`, cfloat(`minWidthVal`))

  if maxWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`gridName`)
      if layout != nil:
        ir_set_max_width(layout, `dimType`, cfloat(`maxWidthVal`))

  if minHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`gridName`)
      if layout != nil:
        ir_set_min_height(layout, `dimType`, cfloat(`minHeightVal`))

  if maxHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`gridName`)
      if layout != nil:
        ir_set_max_height(layout, `dimType`, cfloat(`maxHeightVal`))

  # Add child components
  for child in childNodes:
    let childSym = genSym(nskLet, "gridChild")
    initStmts.add quote do:
      let `childSym` = `child`
      discard kryon_component_add_child(`gridName`, `childSym`)

  # Process deferred if statements
  for ifStmt in deferredIfStmts:
    # Similar reactive conditional handling as Container
    # (simplified for now - can be expanded later)
    initStmts.add(ifStmt)

  # Mark grid as dirty
  initStmts.add quote do:
    kryon_component_mark_dirty(`gridName`)

  # Return the grid component
  result = quote do:
    block:
      let `gridName` = newKryonContainer()  # Create as container, then set grid mode
      `initStmts`
      `gridName`

macro Row*(props: untyped): untyped =
  ## Convenience macro for a row layout container
  ## Preserves user-specified properties and sets defaults for missing ones
  var body = newTree(nnkStmtList)

  var hasWidth = false
  var hasHeight = false
  var hasMainAxisAlignment = false
  var hasAlignItems = false
  var hasBackgroundColor = false

  # First pass: detect which properties user has set
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      if propName == "width": hasWidth = true
      if propName == "height": hasHeight = true
      if propName == "mainAxisAlignment": hasMainAxisAlignment = true
      if propName == "alignItems": hasAlignItems = true
      if propName == "backgroundColor": hasBackgroundColor = true

  # Add defaults first (only if not specified by user)
  # Row should have transparent background by default to avoid double-rendering inherited colors
  if not hasBackgroundColor:
    body.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("transparent"))
  # Note: Don't set default width or height for containers - let them size based on content
  # if not hasWidth:
  #   body.add newTree(nnkAsgn, ident("width"), newIntLitNode(800))  # Default width
  # if not hasHeight:
  #   body.add newTree(nnkAsgn, ident("height"), newIntLitNode(500)) # Default height
  if not hasMainAxisAlignment:
    body.add newTree(nnkAsgn, ident("mainAxisAlignment"), newStrLitNode("start"))
  if not hasAlignItems:
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))

  # Then add user properties (so they override defaults if there are duplicates)
  for node in props.children:
    body.add(node)


  # Always set layout direction to row (at the end so it always overrides)
  body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(1))  # 1 = KRYON_LAYOUT_ROW

  result = newTree(nnkCall, ident("Container"), body)

macro Column*(props: untyped): untyped =
  ## Convenience macro for a column layout container
  ## Preserves user-specified properties and sets defaults for missing ones
  var body = newTree(nnkStmtList)

  # Track which properties are set by user
  var hasWidth = false
  var hasHeight = false
  var hasMainAxisAlignment = false
  var hasAlignItems = false
  var hasFlexGrow = false
  var hasLayoutDirection = false
  var hasBackgroundColor = false

  # First pass: detect which properties user has set
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      if propName == "width": hasWidth = true
      if propName == "height": hasHeight = true
      if propName == "mainAxisAlignment": hasMainAxisAlignment = true
      if propName == "alignItems": hasAlignItems = true
      if propName == "flexGrow": hasFlexGrow = true
      if propName == "layoutDirection": hasLayoutDirection = true
      if propName == "backgroundColor": hasBackgroundColor = true

  # Add defaults first (only if not specified by user)
  # Column should have transparent background by default to avoid double-rendering inherited colors
  if not hasBackgroundColor:
    body.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("transparent"))
  # Column should fill available height by default
  if not hasFlexGrow:
    body.add newTree(nnkAsgn, ident("flexGrow"), newIntLitNode(1))  # Fill available height
  # Don't set default width - let Column fill available width naturally
  if not hasMainAxisAlignment:
    body.add newTree(nnkAsgn, ident("mainAxisAlignment"), newStrLitNode("start"))
  if not hasAlignItems:
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("stretch"))

  # Then add user properties (so they override defaults if there are duplicates)
  for node in props.children:
    body.add(node)

  
  # Always set layout direction to column (at the end so it always overrides)
  body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))  # 0 = KRYON_LAYOUT_COLUMN

  result = newTree(nnkCall, ident("Container"), body)

macro Center*(props: untyped): untyped =
  ## Convenience macro that centers its children both horizontally and vertically.
  ## Creates an IR_COMPONENT_CENTER which handles centering in the renderer.
  let centerSym = genSym(nskLet, "center")
  let newCenterSym = bindSym("newKryonCenter")
  let addChildSym = bindSym("kryon_component_add_child")


  var initStmts = newStmtList()
  var childNodes: seq[NimNode] = @[]

  # Process properties and children
  for node in props.children:
    if node.kind in {nnkCall, nnkCommand, nnkIdent}:
      # This is a child component
      childNodes.add(node)
    elif node.kind == nnkAsgn:
      # Property assignment - apply to center component
      let propName = $node[0]
      let propValue = node[1]
      initStmts.add quote do:
        when compiles(`centerSym`.`propName` = `propValue`):
          `centerSym`.`propName` = `propValue`

  # Add children to center component
  for child in childNodes:
    if isEchoStatement(child):
      initStmts.add(child)
    else:
      initStmts.add quote do:
        let centerChild = block:
          `child`
        discard `addChildSym`(`centerSym`, centerChild)

  result = quote do:
    block:
      let `centerSym` = `newCenterSym`()
      `initStmts`
      `centerSym`

# ============================================================================
# Spacer / Gap - HTML-like spacing elements
# ============================================================================

macro Spacer*(height: int = 10, backgroundColor: string = "transparent"): untyped =
  ## Creates a spacing element (like HTML <br>)
  ## Usage: Spacer() for default 10px spacing, or Spacer(height = 20) for custom
  ## Can also be used as divider: Spacer(height = 1, backgroundColor = "#cccccc")
  let widthExpr = quote do:
    100.pct


  result = quote do:
    Container:
      height = `height`
      width = `widthExpr`
      backgroundColor = `backgroundColor`

macro Gap*(height: int = 10, backgroundColor: string = "transparent"): untyped =
  ## Alias for Spacer - creates a spacing element
  ## Usage: Gap() or Gap(height = 30)
  result = quote do:
    Spacer(`height`, `backgroundColor`)

# ============================================================================
# Tabs
# ============================================================================

macro TabGroup*(props: untyped): untyped =
  let createSym = bindSym("createTabGroupState")
  let finalizeSym = bindSym("finalizeTabGroup")
  let addChildSym = bindSym("kryon_component_add_child")
  let ctxSym = ident("__kryonCurrentTabGroup")

  var propertyNodes: seq[NimNode] = @[]
