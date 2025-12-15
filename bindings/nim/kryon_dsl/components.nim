## UI Component Macros
##
## This module provides all UI component macros:
## - Text: Text display component
## - Button: Interactive button component
## - Dropdown: Dropdown/select component
## - Checkbox: Checkbox input component
## - Input: Text input component
## - Canvas: Drawing canvas component
## - Markdown: Markdown rendering component
## - TabGroup, TabBar, TabContent, TabPanel, Tab: Tab navigation components
## - RadioGroup, RadioButton: Radio button components
## - Slider: Slider input component
## - TextArea: Multi-line text input component
## - ColorPicker: Color selection component
## - FileInput: File upload component
## - staticFor: Compile-time loop unrolling macro

import macros, strutils, sequtils, tables
import ./properties
import ./reactive
import ./core
import ./layout
import ./helpers  # For colorNode and other helper functions
import ./logic_emitter

export properties, reactive, core, layout, helpers, logic_emitter

# Re-export runtime symbols needed by component macros
import ../runtime
import ../ir_core
import ../ir_logic

export runtime, ir_core, ir_logic


macro Text*(props: untyped): untyped =
  ## Text component macro
  var
    textContent = newStrLitNode("Hello")
    colorVal: NimNode = nil
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    fontSizeVal: NimNode = nil
    fontWeightVal: NimNode = nil
    fontFamilyVal: NimNode = nil
    letterSpacingVal: NimNode = nil
    wordSpacingVal: NimNode = nil
    lineHeightVal: NimNode = nil
    textAlignVal: NimNode = nil
    textDecorationVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    zIndexVal: NimNode = nil
    isReactive = false
    reactiveVars: seq[NimNode] = @[]
  let textSym = genSym(nskLet, "text")

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "content", "text":
        textContent = value
        # Check if the text content contains reactive expressions
        isReactive = isReactiveExpression(value)
        if isReactive:
          reactiveVars = extractReactiveVars(value)
          # Text generated inside runtime loops uses loopValCopy_*; treat those as static
          if value.repr.contains("loopValCopy"):
            isReactive = false
            reactiveVars = @[]
      of "color", "colour":  # Added 'colour' alias
        colorVal = colorNode(value)
      of "fontsize":
        fontSizeVal = value
      of "fontweight":
        fontWeightVal = parseFontWeight(value)
      of "fontfamily":
        fontFamilyVal = value
      of "letterspacing":
        letterSpacingVal = value
      of "wordspacing":
        wordSpacingVal = value
      of "lineheight":
        lineHeightVal = value
      of "textalign":
        textAlignVal = parseTextAlign(value)
      of "textdecoration":
        textDecorationVal = parseTextDecoration(value)
      of "margin":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "width", "w":
        widthVal = value
      of "height", "h":
        heightVal = value
      of "zindex", "z":
        zIndexVal = value
      else:
        discard

  let marginTopExpr = if marginTopVal != nil: marginTopVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginRightExpr = if marginRightVal != nil: marginRightVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginBottomExpr = if marginBottomVal != nil: marginBottomVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginLeftExpr = if marginLeftVal != nil: marginLeftVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)

  var initStmts = newTree(nnkStmtList)

  # Handle font family (load font if specified)
  if fontFamilyVal != nil:
    initStmts.add quote do:
      let fontId = getFontId(`fontFamilyVal`, 16)
      if fontId > 0:
        # Set font ID on the text component
        let state = cast[ptr KryonTextState](kryon_component_get_state(`textSym`))
        if not state.isNil:
          state.font_id = fontId

  if colorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`textSym`, `colorVal`)

  if fontSizeVal != nil:
    initStmts.add quote do:
      setFontSize(`textSym`, `fontSizeVal`)

  if fontWeightVal != nil:
    initStmts.add quote do:
      kryon_component_set_font_weight(`textSym`, uint16(`fontWeightVal`))

  if letterSpacingVal != nil:
    initStmts.add quote do:
      kryon_component_set_letter_spacing(`textSym`, cfloat(`letterSpacingVal`))

  if wordSpacingVal != nil:
    initStmts.add quote do:
      kryon_component_set_word_spacing(`textSym`, cfloat(`wordSpacingVal`))

  if lineHeightVal != nil:
    initStmts.add quote do:
      kryon_component_set_line_height(`textSym`, cfloat(`lineHeightVal`))

  if textAlignVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_align(`textSym`, `textAlignVal`)

  if textDecorationVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_decoration(`textSym`, `textDecorationVal`)

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`textSym`,
        uint8(`marginTopExpr`),
        uint8(`marginRightExpr`),
        uint8(`marginBottomExpr`),
        uint8(`marginLeftExpr`))

  # Set bounds if specified
  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let widthExpr = if widthVal != nil: widthVal else: newIntLitNode(0)
  let heightExpr = if heightVal != nil: heightVal else: newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil:
    maskVal = maskVal or 0x01
  if posYVal != nil:
    maskVal = maskVal or 0x02
  if widthVal != nil:
    maskVal = maskVal or 0x04
  if heightVal != nil:
    maskVal = maskVal or 0x08

  if maskVal != 0:
    let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))
    initStmts.add quote do:
      kryon_component_set_bounds_mask(`textSym`,
        toFixed(`xExpr`),
        toFixed(`yExpr`),
        toFixed(`widthExpr`),
        toFixed(`heightExpr`),
        `maskCast`)

  if zIndexVal != nil:
    initStmts.add quote do:
      kryon_component_set_z_index(`textSym`, uint16(`zIndexVal`))

  # Add reactive bindings if detected
  if isReactive and reactiveVars.len > 0:
    # Create live expression evaluation instead of static reactive binding
    let varNames = newLit(reactiveVars.map(proc(x: NimNode): string = x.strVal))
    let expressionStr = newLit(textContent.repr)

    # Convert the expression to a serializable format for .kir files
    # Transform "$value" or "$value.value" to "{{value}}"
    let varNamesSeq = reactiveVars.map(proc(x: NimNode): string = x.strVal)
    var bindingExpr = "{{" & varNamesSeq[0] & "}}"
    let bindingExprLit = newLit(bindingExpr)

    # Add code to create reactive text expression
    # CRITICAL: Create the closure at RUNTIME, not at compile-time, so Nim's
    # closure mechanism properly captures variables and heap-allocates them
    let createExprSym = bindSym("createReactiveTextExpression")
    let setTextExprSym = bindSym("ir_set_text_expression")
    # FIX: Embed the expression AST directly in the closure body (not pre-evaluated)
    # This allows the closure to re-evaluate the expression each time it's called,
    # reading the current variable values (not capturing evaluated results)
    initStmts.add quote do:
      # Create reactive expression for text
      # The closure body contains the expression AST, which will be evaluated
      # each time the closure is called, capturing variables by reference
      let evalProc = proc (): string {.closure.} =
        `textContent`  # Re-evaluate expression each call
      `createExprSym`(`textSym`, `expressionStr`, evalProc, `varNames`)
      # Set the text_expression field for serialization to .kir files
      `setTextExprSym`(`textSym`, `bindingExprLit`)
      echo "[kryon][reactive] Created live reactive expression: ", `expressionStr`, " -> ", `bindingExprLit`

  result = quote do:
    block:
      let `textSym` = newKryonText(`textContent`)
      `initStmts`
      `textSym`

macro Button*(props: untyped): untyped =
  ## Button component macro
  var
    buttonName = genSym(nskLet, "button")
    buttonText = newStrLitNode("Button")
    childNodes: seq[NimNode] = @[]
    clickHandler = newNilLit()
    clickHandlerName: NimNode = nil  # String name for round-trip serialization
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    paddingAll: NimNode = nil
    paddingTopVal: NimNode = nil
    paddingRightVal: NimNode = nil
    paddingBottomVal: NimNode = nil
    paddingLeftVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    minWidthVal: NimNode = nil
    maxWidthVal: NimNode = nil
    minHeightVal: NimNode = nil
    maxHeightVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    borderRadiusVal: NimNode = nil
    boxShadowVal: NimNode = nil
    styleName: NimNode = nil
    styleData: NimNode = nil
    textFadeVal: NimNode = nil
    textOverflowVal: NimNode = nil
    opacityVal: NimNode = nil
    fontSizeVal: NimNode = nil
    fontWeightVal: NimNode = nil
    letterSpacingVal: NimNode = nil
    wordSpacingVal: NimNode = nil
    lineHeightVal: NimNode = nil
    textAlignVal: NimNode = nil
    textDecorationVal: NimNode = nil

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "text":
        buttonText = value
      of "minwidth":
        minWidthVal = value
      of "maxwidth":
        maxWidthVal = value
      of "minheight":
        minHeightVal = value
      of "maxheight":
        maxHeightVal = value
      of "onclick":
        # Wrap the click handler to automatically trigger reactive updates
        # For Counter demo, we need to intercept variable changes in increment/decrement
        clickHandler = value
      of "onclickname":
        # String name of handler for round-trip serialization
        clickHandlerName = value
      of "margin":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "padding":
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "width":
        widthVal = value
      of "height":
        heightVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "background", "backgroundcolor":  # Allow "background" as alias
        bgColorVal = colorNode(value)
      of "color", "textcolor":  # Allow "color" as alias for "textcolor"
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "style":
        # Parse style assignment: style = calendarDayStyle(day)
        if value.kind == nnkCall:
          # Function call: calendarDayStyle(day)
          styleName = value[0]  # The function identifier
          if value.len > 1:
            styleData = value[1]
        elif value.kind == nnkIdent:
          # Simple identifier: myStyle
          styleName = value
        else:
          # Other expression, try to convert to identifier
          styleName = value
      of "textfade":
        textFadeVal = value
      of "textoverflow":
        textOverflowVal = value
      of "opacity":
        opacityVal = value
      of "fontsize":
        fontSizeVal = value
      of "fontweight":
        fontWeightVal = parseFontWeight(value)
      of "letterspacing":
        letterSpacingVal = value
      of "wordspacing":
        wordSpacingVal = value
      of "lineheight":
        lineHeightVal = value
      of "textalign":
        textAlignVal = parseTextAlign(value)
      of "textdecoration":
        textDecorationVal = parseTextDecoration(value)
      of "boxshadow":
        boxShadowVal = value
      else:
        discard
    else:
      # Handle static blocks specially - don't add them to childNodes
      if node.kind == nnkStaticStmt:
        discard  # Static blocks will be handled by the Container macros they contain
      else:
        childNodes.add(node)

  let marginTopExpr = if marginTopVal != nil: marginTopVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginRightExpr = if marginRightVal != nil: marginRightVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginBottomExpr = if marginBottomVal != nil: marginBottomVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginLeftExpr = if marginLeftVal != nil: marginLeftVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)

  # Set padding (support string format, tuple with .px, and individual values)
  var padTopExpr, padRightExpr, padBottomExpr, padLeftExpr: NimNode

  # Check if paddingAll is a string like "20px, 20px, 20px, 20px"
  if paddingAll != nil and paddingAll.kind == nnkStrLit:
    let parsed = parseQuadString(paddingAll)
    padTopExpr = parsed.top
    padRightExpr = parsed.right
    padBottomExpr = parsed.bottom
    padLeftExpr = parsed.left
  # Check if paddingAll is a tuple like (12.px, 24.px, 12.px, 24.px)
  elif paddingAll != nil and (paddingAll.kind == nnkTupleConstr or paddingAll.kind == nnkPar):
    # Extract values from tuple and handle .px suffixes
    let top = if paddingAll.len >= 1: paddingAll[0] else: newIntLitNode(0)
    let right = if paddingAll.len >= 2: paddingAll[1] else: newIntLitNode(0)
    let bottom = if paddingAll.len >= 3: paddingAll[2] else: newIntLitNode(0)
    let left = if paddingAll.len >= 4: paddingAll[3] else: newIntLitNode(0)
    padTopExpr = if isPxExpression(top): extractPxValue(top) else: top
    padRightExpr = if isPxExpression(right): extractPxValue(right) else: right
    padBottomExpr = if isPxExpression(bottom): extractPxValue(bottom) else: bottom
    padLeftExpr = if isPxExpression(left): extractPxValue(left) else: left
  else:
    # Individual values or single paddingAll value
    let allVal = if paddingAll != nil:
      (if isPxExpression(paddingAll): extractPxValue(paddingAll) else: paddingAll)
    else: newIntLitNode(0)

    padTopExpr = if paddingTopVal != nil:
      (if isPxExpression(paddingTopVal): extractPxValue(paddingTopVal) else: paddingTopVal)
    else: allVal
    padRightExpr = if paddingRightVal != nil:
      (if isPxExpression(paddingRightVal): extractPxValue(paddingRightVal) else: paddingRightVal)
    else: allVal
    padBottomExpr = if paddingBottomVal != nil:
      (if isPxExpression(paddingBottomVal): extractPxValue(paddingBottomVal) else: paddingBottomVal)
    else: allVal
    padLeftExpr = if paddingLeftVal != nil:
      (if isPxExpression(paddingLeftVal): extractPxValue(paddingLeftVal) else: paddingLeftVal)
    else: allVal

  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let widthExpr = if widthVal != nil: widthVal else: newIntLitNode(0)
  let heightExpr = if heightVal != nil: heightVal else: newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08
  let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))

  # Normalize click handlers: allow plain calls to be lifted into procs
  if clickHandler.kind notin {nnkNilLit, nnkProcDef, nnkLambda} and clickHandler.kind != nnkIdent:
    clickHandler = quote do:
      proc() =
        `clickHandler`

  var initStmts = newTree(nnkStmtList)

  # Apply style first (before individual property overrides)
  if styleName != nil:
    if styleData != nil:
      initStmts.add quote do:
        `styleName`(`buttonName`, `styleData`)
    else:
      initStmts.add quote do:
        `styleName`(`buttonName`)

  initStmts.add quote do:
    kryon_component_set_bounds_mask(`buttonName`,
      toFixed(`xExpr`),
      toFixed(`yExpr`),
      toFixed(`widthExpr`),
      toFixed(`heightExpr`),
      `maskCast`)

  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_padding(`buttonName`,
        uint8(`padTopExpr`),
        uint8(`padRightExpr`),
        uint8(`padBottomExpr`),
        uint8(`padLeftExpr`))

  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`buttonName`,
        uint8(`marginTopExpr`),
        uint8(`marginRightExpr`),
        uint8(`marginBottomExpr`),
        uint8(`marginLeftExpr`))

  if bgColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`buttonName`, `bgColorVal`)

  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`buttonName`, `textColorVal`)

  if fontSizeVal != nil:
    initStmts.add quote do:
      setFontSize(`buttonName`, `fontSizeVal`)

  if fontWeightVal != nil:
    initStmts.add quote do:
      kryon_component_set_font_weight(`buttonName`, uint16(`fontWeightVal`))

  if letterSpacingVal != nil:
    initStmts.add quote do:
      kryon_component_set_letter_spacing(`buttonName`, cfloat(`letterSpacingVal`))

  if wordSpacingVal != nil:
    initStmts.add quote do:
      kryon_component_set_word_spacing(`buttonName`, cfloat(`wordSpacingVal`))

  if lineHeightVal != nil:
    initStmts.add quote do:
      kryon_component_set_line_height(`buttonName`, cfloat(`lineHeightVal`))

  if textAlignVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_align(`buttonName`, `textAlignVal`)

  if textDecorationVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_decoration(`buttonName`, `textDecorationVal`)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`buttonName`, `borderColorVal`)

  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`buttonName`, uint8(`borderWidthVal`))
  else:
    initStmts.add quote do:
      kryon_component_set_border_width(`buttonName`, 1)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`buttonName`, `borderColorVal`)
  else:
    initStmts.add quote do:
      # Default border: white for good contrast with dark button backgrounds
      kryon_component_set_border_color(`buttonName`, rgba(200, 200, 200, 255))

  if borderRadiusVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_radius(`buttonName`, uint8(`borderRadiusVal`))

  if boxShadowVal != nil:
    # Support both named parameter syntax and positional tuple syntax
    let isNamedTuple = (boxShadowVal.kind == nnkTableConstr) or
                       ((boxShadowVal.kind == nnkTupleConstr or boxShadowVal.kind == nnkPar) and
                        boxShadowVal.len > 0 and boxShadowVal[0].kind == nnkExprColonExpr)

    if isNamedTuple:
      # Named parameter syntax: (offsetY: 8.0, blur: 24.0, color: "#000")
      let spec = parseBoxShadowSpec(boxShadowVal)
      let shadowCall = buildBoxShadowCreation(buttonName, spec)
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
        kryon_component_set_box_shadow(`buttonName`, cfloat(`offsetX`), cfloat(`offsetY`), cfloat(`blurRadius`), cfloat(`spreadRadius`), `shadowColor`, `inset`)

  # Text effect properties
  if textFadeVal != nil:
    # Parse: textFade = horizontal(15) or textFade = none
    if textFadeVal.kind == nnkCall:
      let fadeTypeStr = ($textFadeVal[0]).toLowerAscii()
      var fadeType = ident("IR_TEXT_FADE_NONE")
      var fadeLength = newFloatLitNode(0.0)
      case fadeTypeStr:
      of "horizontal":
        fadeType = ident("IR_TEXT_FADE_HORIZONTAL")
        if textFadeVal.len > 1:
          fadeLength = textFadeVal[1]
      of "vertical":
        fadeType = ident("IR_TEXT_FADE_VERTICAL")
        if textFadeVal.len > 1:
          fadeLength = textFadeVal[1]
      of "radial":
        fadeType = ident("IR_TEXT_FADE_RADIAL")
        if textFadeVal.len > 1:
          fadeLength = textFadeVal[1]
      of "none":
        fadeType = ident("IR_TEXT_FADE_NONE")
      else:
        discard
      initStmts.add quote do:
        kryon_component_set_text_fade(`buttonName`, `fadeType`, float(`fadeLength`))
    elif textFadeVal.kind == nnkIdent and ($textFadeVal).toLowerAscii() == "none":
      let fadeNone = ident("IR_TEXT_FADE_NONE")
      initStmts.add quote do:
        kryon_component_set_text_fade(`buttonName`, `fadeNone`, 0.0)

  if textOverflowVal != nil:
    let overflowStr = if textOverflowVal.kind == nnkIdent: ($textOverflowVal).toLowerAscii() else: ""
    var overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
    case overflowStr:
    of "visible": overflowType = ident("IR_TEXT_OVERFLOW_VISIBLE")
    of "clip": overflowType = ident("IR_TEXT_OVERFLOW_CLIP")
    of "ellipsis": overflowType = ident("IR_TEXT_OVERFLOW_ELLIPSIS")
    of "fade": overflowType = ident("IR_TEXT_OVERFLOW_FADE")
    else: discard
    initStmts.add quote do:
      kryon_component_set_text_overflow(`buttonName`, `overflowType`)

  if opacityVal != nil:
    initStmts.add quote do:
      kryon_component_set_opacity(`buttonName`, float(`opacityVal`))

  # Apply min/max width/height constraints via layout
  if minWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_min_width(layout, `dimType`, cfloat(`minWidthVal`))

  if maxWidthVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_max_width(layout, `dimType`, cfloat(`maxWidthVal`))

  if minHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_min_height(layout, `dimType`, cfloat(`minHeightVal`))

  if maxHeightVal != nil:
    let dimType = bindSym("IR_DIMENSION_PX")
    initStmts.add quote do:
      let layout = ir_get_layout(`buttonName`)
      if layout != nil:
        ir_set_max_height(layout, `dimType`, cfloat(`maxHeightVal`))

  let newButtonSym = bindSym("newKryonButton")
  let buttonBridgeSym = bindSym("nimButtonBridge")
  let registerHandlerSym = bindSym("registerButtonHandler")

  # Create button (IR system handles events separately via registerButtonHandler)
  var buttonCall = newCall(newButtonSym, buttonText)

  # Register onClick handler if provided
  if clickHandler.kind != nnkNilLit:
    # Analyze handler for universal expression conversion
    let analysis = analyzeHandler(clickHandler)

    if analysis.isUniversal:
      # Generate logic function name based on variable and operation
      let funcName = "Button:" & analysis.targetVar & "_" & analysis.operation
      let funcNameLit = newLit(funcName)
      let varNameLit = newLit(analysis.targetVar)
      let operationLit = newLit(analysis.operation)

      # Register universal logic function (only if not already registered) and event binding
      initStmts.add quote do:
        # Register logic function to global logic block (deduped)
        if registerLogicFunction(`funcNameLit`, `varNameLit`, `operationLit`):
          echo "[kryon][logic] Registered universal handler: ", `funcNameLit`, " -> ", `operationLit`, "(", `varNameLit`, ")"
        # Always register event binding (each component instance needs its own)
        registerEventBinding(uint32(`buttonName`.id), "click", `funcNameLit`)

    else:
      # Complex handler - register embedded Nim source
      let lineInfo = clickHandler.lineInfoObj
      let funcName = "Button:handler_" & $lineInfo.line
      let funcNameLit = newLit(funcName)
      let nimSourceLit = newLit(analysis.nimSource)

      initStmts.add quote do:
        # Register embedded Nim source as fallback (deduped)
        if registerLogicFunctionWithSource(`funcNameLit`, "nim", `nimSourceLit`):
          echo "[kryon][logic] Registered embedded handler: ", `funcNameLit`
        # Always register event binding (each component instance needs its own)
        registerEventBinding(uint32(`buttonName`.id), "click", `funcNameLit`)

    # Use Nim proc handler (via handler bridge system) for runtime execution
    initStmts.add quote do:
      `registerHandlerSym`(`buttonName`, `clickHandler`)

    # Register handler name and source for round-trip serialization if provided
    if clickHandlerName != nil:
      let registerRoundTripSym = bindSym("registerHandler")
      let registerSourceSym = bindSym("registerSource")

      # For named proc references, try to extract source from the file
      var nimSourceLit: NimNode
      if clickHandler.kind in {nnkIdent, nnkSym} and analysis.nimSource == $clickHandler:
        # The analysis only got the name, try to extract full source from file
        # Use the props node's lineInfo to get the caller's file (props comes from user code)
        let callerInfo = props.lineInfoObj
        let handlerNameStr = $clickHandler
        let extractedSource = findProcSourceInCallerFile(callerInfo, handlerNameStr)
        nimSourceLit = newLit(extractedSource)
      else:
        nimSourceLit = newLit(analysis.nimSource)

      initStmts.add quote do:
        # Get the logic_id that was created for this button
        let logicId = "nim_button_" & $`buttonName`.id
        `registerRoundTripSym`(uint32(`buttonName`.id), "click", logicId, `clickHandlerName`)
        # Auto-register source for round-trip (from the actual handler definition)
        `registerSourceSym`("nim", `nimSourceLit`)

  initStmts.add quote do:
    kryon_component_mark_dirty(`buttonName`)

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComp = block:
        `child`
      if childComp != nil:
        discard kryon_component_add_child(`buttonName`, childComp)

  result = quote do:
    block:
      let `buttonName` = `buttonCall`
      `initStmts`
      `childStmtList`
      `buttonName`

macro Dropdown*(props: untyped): untyped =
  ## Dropdown component macro
  var
    dropdownName = genSym(nskLet, "dropdown")
    placeholderVal = newStrLitNode("Select...")
    optionsVal: NimNode = nil
    selectedIndexVal: NimNode = newIntLitNode(-1)
    clickHandler = newNilLit()
    marginAll: NimNode = nil
    marginTopVal: NimNode = nil
    marginRightVal: NimNode = nil
    marginBottomVal: NimNode = nil
    marginLeftVal: NimNode = nil
    paddingAll: NimNode = nil
    paddingTopVal: NimNode = nil
    paddingRightVal: NimNode = nil
    paddingBottomVal: NimNode = nil
    paddingLeftVal: NimNode = nil
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    bgColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    hoverColorVal: NimNode = nil
    fontSizeVal: NimNode = nil

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      let value = node[1]

      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = value
      of "options":
        optionsVal = value
      of "selectedindex":
        selectedIndexVal = value
      of "onchange":
        clickHandler = value
      of "margin":
        marginAll = value
      of "margintop":
        marginTopVal = value
      of "marginright":
        marginRightVal = value
      of "marginbottom":
        marginBottomVal = value
      of "marginleft":
        marginLeftVal = value
      of "padding":
        paddingAll = value
      of "paddingtop":
        paddingTopVal = value
      of "paddingright":
        paddingRightVal = value
      of "paddingbottom":
        paddingBottomVal = value
      of "paddingleft":
        paddingLeftVal = value
      of "width":
        widthVal = value
      of "height":
        heightVal = value
      of "posx", "x":
        posXVal = value
      of "posy", "y":
        posYVal = value
      of "background", "backgroundcolor":
        bgColorVal = colorNode(value)
      of "color", "textcolor":
        textColorVal = colorNode(value)
      of "bordercolor":
        borderColorVal = colorNode(value)
      of "borderwidth":
        borderWidthVal = value
      of "hovercolor":
        hoverColorVal = colorNode(value)
      of "fontsize":
        fontSizeVal = value
      else:
        discard

  let marginTopExpr = if marginTopVal != nil: marginTopVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginRightExpr = if marginRightVal != nil: marginRightVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginBottomExpr = if marginBottomVal != nil: marginBottomVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)
  let marginLeftExpr = if marginLeftVal != nil: marginLeftVal
    elif marginAll != nil: marginAll
    else: newIntLitNode(0)

  # Set padding (support string format, tuple with .px, and individual values)
  var padTopExpr, padRightExpr, padBottomExpr, padLeftExpr: NimNode

  # Check if paddingAll is a string like "20px, 20px, 20px, 20px"
  if paddingAll != nil and paddingAll.kind == nnkStrLit:
    let parsed = parseQuadString(paddingAll)
    padTopExpr = parsed.top
    padRightExpr = parsed.right
    padBottomExpr = parsed.bottom
    padLeftExpr = parsed.left
  # Check if paddingAll is a tuple like (12.px, 24.px, 12.px, 24.px)
  elif paddingAll != nil and (paddingAll.kind == nnkTupleConstr or paddingAll.kind == nnkPar):
    # Extract values from tuple and handle .px suffixes
    let top = if paddingAll.len >= 1: paddingAll[0] else: newIntLitNode(0)
    let right = if paddingAll.len >= 2: paddingAll[1] else: newIntLitNode(0)
    let bottom = if paddingAll.len >= 3: paddingAll[2] else: newIntLitNode(0)
    let left = if paddingAll.len >= 4: paddingAll[3] else: newIntLitNode(0)
    padTopExpr = if isPxExpression(top): extractPxValue(top) else: top
    padRightExpr = if isPxExpression(right): extractPxValue(right) else: right
    padBottomExpr = if isPxExpression(bottom): extractPxValue(bottom) else: bottom
    padLeftExpr = if isPxExpression(left): extractPxValue(left) else: left
  else:
    # Individual values or single paddingAll value
    let allVal = if paddingAll != nil:
      (if isPxExpression(paddingAll): extractPxValue(paddingAll) else: paddingAll)
    else: newIntLitNode(0)

    padTopExpr = if paddingTopVal != nil:
      (if isPxExpression(paddingTopVal): extractPxValue(paddingTopVal) else: paddingTopVal)
    else: allVal
    padRightExpr = if paddingRightVal != nil:
      (if isPxExpression(paddingRightVal): extractPxValue(paddingRightVal) else: paddingRightVal)
    else: allVal
    padBottomExpr = if paddingBottomVal != nil:
      (if isPxExpression(paddingBottomVal): extractPxValue(paddingBottomVal) else: paddingBottomVal)
    else: allVal
    padLeftExpr = if paddingLeftVal != nil:
      (if isPxExpression(paddingLeftVal): extractPxValue(paddingLeftVal) else: paddingLeftVal)
    else: allVal

  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let widthExpr = if widthVal != nil: widthVal else: newIntLitNode(0)
  let heightExpr = if heightVal != nil: heightVal else: newIntLitNode(0)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08
  let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))

  # Build initialization statements
  var initStmts = newTree(nnkStmtList)

  # Set bounds
  initStmts.add quote do:
    kryon_component_set_bounds_mask(`dropdownName`,
      toFixed(`xExpr`),
      toFixed(`yExpr`),
      toFixed(`widthExpr`),
      toFixed(`heightExpr`),
      `maskCast`)

  # Set padding if specified
  if paddingAll != nil or paddingTopVal != nil or paddingRightVal != nil or paddingBottomVal != nil or paddingLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_padding(`dropdownName`,
        uint8(`padTopExpr`),
        uint8(`padRightExpr`),
        uint8(`padBottomExpr`),
        uint8(`padLeftExpr`))

  # Set margin if specified
  if marginAll != nil or marginTopVal != nil or marginRightVal != nil or marginBottomVal != nil or marginLeftVal != nil:
    initStmts.add quote do:
      kryon_component_set_margin(`dropdownName`,
        uint8(`marginTopExpr`),
        uint8(`marginRightExpr`),
        uint8(`marginBottomExpr`),
        uint8(`marginLeftExpr`))

  # Note: Colors and styles are set through the state during creation,
  # so they don't need to be set after component creation

  initStmts.add quote do:
    kryon_component_mark_dirty(`dropdownName`)

  # Build the dropdown creation call with all parameters
  let newDropdownSym = bindSym("newKryonDropdown")
  let optionsExpr = if optionsVal != nil: optionsVal
    else: newNimNode(nnkPrefix).add(ident("@"), newNimNode(nnkBracket))

  # Build named arguments for the creation function
  var creationStmt = newTree(nnkCall, newDropdownSym)
  creationStmt.add(newTree(nnkExprEqExpr, ident("placeholder"), placeholderVal))
  creationStmt.add(newTree(nnkExprEqExpr, ident("options"), optionsExpr))
  creationStmt.add(newTree(nnkExprEqExpr, ident("selectedIndex"), selectedIndexVal))

  # Add optional parameters if they were specified
  if fontSizeVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("fontSize"), newCall(ident("uint8"), fontSizeVal)))
  if textColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("textColor"), textColorVal))
  if bgColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("backgroundColor"), bgColorVal))
  if borderColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("borderColor"), borderColorVal))
  if borderWidthVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("borderWidth"), newCall(ident("uint8"), borderWidthVal)))
  if hoverColorVal != nil:
    creationStmt.add(newTree(nnkExprEqExpr, ident("hoverColor"), hoverColorVal))
  if clickHandler.kind != nnkNilLit:
    creationStmt.add(newTree(nnkExprEqExpr, ident("onChangeCallback"), clickHandler))

  result = quote do:
    block:
      let `dropdownName` = `creationStmt`
      `initStmts`
      `dropdownName`

macro Checkbox*(props: untyped): untyped =
  ## Checkbox component macro
  var
    checkboxName = genSym(nskLet, "checkbox")
    clickHandler = newNilLit()
    labelVal: NimNode = newStrLitNode("")
    checkedVal: NimNode = newLit(false)
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    fontSizeVal: NimNode = nil
    textColorVal: NimNode = nil
    backgroundColorVal: NimNode = nil

  # Parse Checkbox properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "label":
        if node[1].kind == nnkStrLit:
          labelVal = node[1]
      of "checked":
        if node[1].kind == nnkIdent or node[1].kind in {nnkIntLit, nnkIdent}:
          checkedVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "fontsize":
        fontSizeVal = node[1]
      of "textcolor":
        textColorVal = colorNode(node[1])
      of "background", "backgroundcolor":
        backgroundColorVal = colorNode(node[1])
      of "onclick", "onchange":
        if node[1].kind == nnkIdent:
          clickHandler = node[1]
      else:
        discard

  var initStmts = newTree(nnkStmtList)

  # Set width and height using IR style system
  if widthVal != nil:
    initStmts.add quote do:
      setWidth(`checkboxName`, `widthVal`)
  if heightVal != nil:
    initStmts.add quote do:
      setHeight(`checkboxName`, `heightVal`)

  # Set font size if provided
  if fontSizeVal != nil:
    initStmts.add quote do:
      setFontSize(`checkboxName`, `fontSizeVal`)

  # Set text color if provided (for the label text)
  if textColorVal != nil:
    initStmts.add quote do:
      setTextColor(`checkboxName`, runtime.parseColor(`textColorVal`))

  # Set background color if provided
  if backgroundColorVal != nil:
    initStmts.add quote do:
      setBgColor(`checkboxName`, runtime.parseColor(`backgroundColorVal`))

  # Store checked state in custom_data
  initStmts.add quote do:
    setCheckboxState(`checkboxName`, `checkedVal`)

  # Register click handler if provided
  if clickHandler.kind != nnkNilLit:
    # Analyze handler for universal expression conversion
    let analysis = analyzeHandler(clickHandler)

    if analysis.isUniversal:
      # Generate logic function name based on variable and operation
      let funcName = "Checkbox:" & analysis.targetVar & "_" & analysis.operation
      let funcNameLit = newLit(funcName)
      let varNameLit = newLit(analysis.targetVar)
      let operationLit = newLit(analysis.operation)

      # Register universal logic function and event binding
      initStmts.add quote do:
        if registerLogicFunction(`funcNameLit`, `varNameLit`, `operationLit`):
          echo "[kryon][logic] Registered universal checkbox handler: ", `funcNameLit`
        registerEventBinding(uint32(`checkboxName`.id), "click", `funcNameLit`)
        # Use logic_id-aware handler registration
        registerCheckboxHandlerWithLogicId(`checkboxName`, proc() = `clickHandler`(), `funcNameLit`)

    else:
      # Complex handler - register embedded Nim source
      let lineInfo = clickHandler.lineInfoObj
      let funcName = "nim_checkbox_" & $lineInfo.line
      let funcNameLit = newLit(funcName)
      let nimSourceLit = newLit(analysis.nimSource)

      initStmts.add quote do:
        if registerLogicFunctionWithSource(`funcNameLit`, "nim", `nimSourceLit`):
          echo "[kryon][logic] Registered embedded checkbox handler: ", `funcNameLit`
        registerEventBinding(uint32(`checkboxName`.id), "click", `funcNameLit`)
        # Use logic_id-aware handler registration
        registerCheckboxHandlerWithLogicId(`checkboxName`, proc() = `clickHandler`(), `funcNameLit`)

  result = quote do:
    block:
      let `checkboxName` = newKryonCheckbox(`labelVal`)
      `initStmts`
      `checkboxName`

macro Input*(props: untyped): untyped =
  ## Input component macro
  ## Note: Text editing not yet supported by C core, shows as styled container
  var
    inputName = genSym(nskLet, "input")
    placeholderVal: NimNode = newStrLitNode("")
    valueVal: NimNode = newStrLitNode("")
    widthVal: NimNode = nil
    heightVal: NimNode = nil
    posXVal: NimNode = nil
    posYVal: NimNode = nil
    backgroundColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    borderRadiusVal: NimNode = nil
    fontSizeVal: NimNode = newIntLitNode(14)
    fontWeightVal: NimNode = nil
    letterSpacingVal: NimNode = nil
    wordSpacingVal: NimNode = nil
    lineHeightVal: NimNode = nil
    textAlignVal: NimNode = nil
    textDecorationVal: NimNode = nil
    onTextChangeVal: NimNode = nil
    childNodes: seq[NimNode] = @[]

  # Parse Input properties
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = node[1]
      of "value":
        valueVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "posx", "x":
        posXVal = node[1]
      of "posy", "y":
        posYVal = node[1]
      of "background", "backgroundcolor":
        backgroundColorVal = colorNode(node[1])
      of "textcolor", "color":
        textColorVal = colorNode(node[1])
      of "bordercolor":
        borderColorVal = colorNode(node[1])
      of "borderwidth":
        borderWidthVal = node[1]
      of "borderradius":
        borderRadiusVal = node[1]
      of "fontsize":
        fontSizeVal = node[1]
      of "fontweight":
        fontWeightVal = parseFontWeight(node[1])
      of "letterspacing":
        letterSpacingVal = node[1]
      of "wordspacing":
        wordSpacingVal = node[1]
      of "lineheight":
        lineHeightVal = node[1]
      of "textalign":
        textAlignVal = parseTextAlign(node[1])
      of "textdecoration":
        textDecorationVal = parseTextDecoration(node[1])
      of "ontextchange":
        onTextChangeVal = node[1]
      of "inputtype", "min", "max", "step", "accept":
        # Store these for future implementation
        discard
      else:
        discard
    else:
      childNodes.add(node)

  var initStmts = newTree(nnkStmtList)

  # Use bounds_mask to allow layout system to position the input
  let xExpr = if posXVal != nil: posXVal else: newIntLitNode(0)
  let yExpr = if posYVal != nil: posYVal else: newIntLitNode(0)
  let wExpr = if widthVal != nil: widthVal else: newIntLitNode(200)
  let hExpr = if heightVal != nil: heightVal else: newIntLitNode(40)

  var maskVal = 0
  if posXVal != nil: maskVal = maskVal or 0x01
  if posYVal != nil: maskVal = maskVal or 0x02
  if widthVal != nil: maskVal = maskVal or 0x04
  if heightVal != nil: maskVal = maskVal or 0x08
  let maskCast = newCall(ident("uint8"), newIntLitNode(maskVal))

  initStmts.add quote do:
    kryon_component_set_bounds_mask(`inputName`,
      toFixed(`xExpr`),
      toFixed(`yExpr`),
      toFixed(`wExpr`),
      toFixed(`hExpr`),
      `maskCast`)

  # Add padding
  initStmts.add quote do:
    kryon_component_set_padding(`inputName`, 8, 8, 8, 8)

  # Apply colors if provided (accept string or uint32)
  if backgroundColorVal != nil:
    let bgExpr =
      if backgroundColorVal.kind == nnkStrLit: quote do: runtime.parseColor(`backgroundColorVal`)
      else: backgroundColorVal
    initStmts.add quote do:
      setBackgroundColor(`inputName`, `bgExpr`)

  if textColorVal != nil:
    let tcExpr =
      if textColorVal.kind == nnkStrLit: quote do: runtime.parseColor(`textColorVal`)
      else: textColorVal
    initStmts.add quote do:
      setTextColor(`inputName`, `tcExpr`)

  if fontSizeVal != nil:
    initStmts.add quote do:
      setFontSize(`inputName`, `fontSizeVal`)

  if fontWeightVal != nil:
    initStmts.add quote do:
      kryon_component_set_font_weight(`inputName`, uint16(`fontWeightVal`))

  if letterSpacingVal != nil:
    initStmts.add quote do:
      kryon_component_set_letter_spacing(`inputName`, cfloat(`letterSpacingVal`))

  if wordSpacingVal != nil:
    initStmts.add quote do:
      kryon_component_set_word_spacing(`inputName`, cfloat(`wordSpacingVal`))

  if lineHeightVal != nil:
    initStmts.add quote do:
      kryon_component_set_line_height(`inputName`, cfloat(`lineHeightVal`))

  if textAlignVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_align(`inputName`, `textAlignVal`)

  if textDecorationVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_decoration(`inputName`, `textDecorationVal`)

  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`inputName`, `borderColorVal`)

  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`inputName`, uint8(`borderWidthVal`))

  if borderRadiusVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_radius(`inputName`, uint8(`borderRadiusVal`))

  # Store placeholder separately; only set real value as text content
  initStmts.add quote do:
    setCustomData(`inputName`, `placeholderVal`)
    if `valueVal`.len > 0:
      setText(`inputName`, `valueVal`)
    else:
      setText(`inputName`, "")

  # Wire onChange/value binding if provided
  let hasValueBinding = valueVal.kind == nnkIdent
  if hasValueBinding and onTextChangeVal != nil:
    initStmts.add quote do:
      registerInputHandler(`inputName`, proc(t: string) =
        `valueVal` = t
        `onTextChangeVal`(t)
      )
  elif hasValueBinding:
    initStmts.add quote do:
      registerInputHandler(`inputName`, proc(t: string) =
        `valueVal` = t
      )
  elif onTextChangeVal != nil:
    initStmts.add quote do:
      registerInputHandler(`inputName`, proc(t: string) =
        `onTextChangeVal`(t)
      )

  # Keep the component value in sync with the bound variable (e.g., when cleared in code)
  if hasValueBinding:
    let syncProcSym = genSym(nskProc, "syncInputValue")
    initStmts.add quote do:
      proc `syncProcSym`() =
        setText(`inputName`, `valueVal`)
      registerReactiveRebuild(`syncProcSym`)
      `syncProcSym`()

  # Add any child components (though Input usually doesn't have children)
  for child in childNodes:
    if isEchoStatement(child):
      initStmts.add(child)
    else:
      initStmts.add quote do:
        let childComponent = block:
          `child`
        if childComponent != nil:
          discard kryon_component_add_child(`inputName`, childComponent)

  let onTextChangeExpr = if onTextChangeVal != nil: onTextChangeVal else: newNilLit()
  
  # Prepare color expressions
  let textColorExpr = if textColorVal != nil:
    if textColorVal.kind == nnkStrLit: newCall(ident("parseColor"), textColorVal) else: textColorVal
  else: newIntLitNode(0)
  
  let bgColorExpr = if backgroundColorVal != nil:
    if backgroundColorVal.kind == nnkStrLit: newCall(ident("parseColor"), backgroundColorVal) else: backgroundColorVal
  else: newIntLitNode(0)
  
  let borderColorExpr = if borderColorVal != nil:
    if borderColorVal.kind == nnkStrLit: newCall(ident("parseColor"), borderColorVal) else: borderColorVal
  else: newIntLitNode(0)

  result = quote do:
    block:
      # Use the new IR-based Input component (placeholder/value currently unused)
      let `inputName` = newKryonInput()
      `initStmts`
      kryon_component_mark_dirty(`inputName`)
      `inputName`

# Canvas macro removed - now available as a plugin
# Import from kryon-plugin-canvas to use Canvas component

macro Spacer*(props: untyped): untyped =
  ## Spacer component macro
  var
    spacerName = genSym(nskLet, "spacer")

  discard props

  result = quote do:
    block:
      let `spacerName` = newKryonSpacer()
      `spacerName`

# ============================================================================
# Tabs
# ============================================================================

macro TabGroup*(props: untyped): untyped =
  let createSym = bindSym("createTabGroupState")
  let finalizeSym = bindSym("finalizeTabGroup")
  let addChildSym = bindSym("kryon_component_add_child")
  let ctxSym = ident("__kryonCurrentTabGroup")

  var propertyNodes: seq[NimNode] = @[]
  var childNodes: seq[NimNode] = @[]
  var selectedIndexVal: NimNode = newIntLitNode(0)
  var reorderableVal: NimNode = newLit(false)
  var hasLayout = false
  var hasGap = false
  var hasAlign = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "selectedindex":
        selectedIndexVal = node[1]
      of "reorderable":
        reorderableVal = node[1]
      of "layoutdirection":
        hasLayout = true
        propertyNodes.add(node)
      of "gap":
        hasGap = true
        propertyNodes.add(node)
      of "alignitems":
        hasAlign = true
        propertyNodes.add(node)
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))
  if not hasAlign:
    # Stretch children to fill TabGroup width - enables flex_grow on tabs
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("stretch"))
  if not hasGap:
    body.add newTree(nnkAsgn, ident("gap"), newIntLitNode(8))
  for node in propertyNodes:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let groupSym = genSym(nskLet, "tabGroup")
  let stateSym = genSym(nskLet, "tabGroupState")

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`groupSym`, childComponent)

  let panelIndexSym = ident("__kryonTabPanelIndex")

  result = quote do:
    block:
      let `groupSym` = `containerCall`
      # TabGroup state needs group, tab bar, tab content, selected index, reorderable flag
      let `stateSym` = `createSym`(`groupSym`, nil, nil, `selectedIndexVal`, `reorderableVal`)
      let `ctxSym` = `stateSym`
      var `panelIndexSym` = 0  # Counter for panel indices
      block:
        `childStmtList`
      `finalizeSym`(`stateSym`)
      # Register state for runtime access by reactive system
      registerTabGroupState(`groupSym`, `stateSym`)
      `groupSym`


macro TabBar*(props: untyped): untyped =
  let registerSym = bindSym("registerTabBar")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let ctxBarSym = ident("__kryonCurrentTabBar")

  var propertyNodes: seq[NimNode] = @[]
  var childNodes: seq[NimNode] = @[]
  var reorderableVal: NimNode = newLit(false)
  var hasLayout = false
  var hasGap = false
  var hasAlign = false
  var hasPadding = false
  var hasBackground = false
  var hasBorderColor = false
  var hasBorderWidth = false
  var hasFlex = false
  var hasWidth = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "reorderable":
        reorderableVal = node[1]
      of "layoutdirection":
        hasLayout = true
        propertyNodes.add(node)
      of "gap":
        hasGap = true
        propertyNodes.add(node)
      of "alignitems", "crossaxisalignment":
        hasAlign = true
        propertyNodes.add(node)
      of "padding":
        hasPadding = true
        propertyNodes.add(node)
      of "background", "backgroundcolor":
        hasBackground = true
        propertyNodes.add(node)
      of "bordercolor":
        hasBorderColor = true
        propertyNodes.add(node)
      of "borderwidth":
        hasBorderWidth = true
        propertyNodes.add(node)
      of "flexgrow":
        hasFlex = true
        propertyNodes.add(node)
      of "width":
        hasWidth = true
        propertyNodes.add(node)
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(1))
  if not hasAlign:
    body.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))
  if not hasGap:
    # Chrome-like tabs: minimal gap between tabs
    body.add newTree(nnkAsgn, ident("gap"), newIntLitNode(2))
  if not hasPadding:
    body.add newTree(nnkAsgn, ident("paddingTop"), newIntLitNode(4))
    body.add newTree(nnkAsgn, ident("paddingBottom"), newIntLitNode(4))
    body.add newTree(nnkAsgn, ident("paddingLeft"), newIntLitNode(8))
    body.add newTree(nnkAsgn, ident("paddingRight"), newIntLitNode(8))
  if not hasBackground:
    body.add newTree(nnkAsgn, ident("backgroundColor"), newStrLitNode("#202124"))
  if not hasBorderWidth:
    body.add newTree(nnkAsgn, ident("borderWidth"), newIntLitNode(1))
  if not hasBorderColor:
    body.add newTree(nnkAsgn, ident("borderColor"), newStrLitNode("#2b2f33"))
  # Add explicit height to prevent TabBar visibility issues
  var hasHeight = false
  var otherProperties: seq[NimNode] = @[]

  for node in propertyNodes:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      if propName.toLowerAscii() == "height":
        hasHeight = true
        body.add(node)
      else:
        # Only add properties we haven't already handled
        otherProperties.add(node)

  # Add remaining properties
  for node in otherProperties:
    body.add(node)

  # Set default height if not provided
  if not hasHeight:
    body.add newTree(nnkAsgn, ident("height"), newIntLitNode(40))

  # Remove default flex grow - TabBar should maintain its fixed height
  # TabBar expands horizontally due to width=100% in its parent, not flex grow

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let barSym = genSym(nskLet, "tabBar")

  var childStmtList = newStmtList()
  for node in childNodes:
    if isEchoStatement(node):
      childStmtList.add(node)
      continue
    if node.kind == nnkForStmt:
      # Handle runtime for loops with Tab components
      # Structure: nnkForStmt[loopVar, collection, body]
      let loopVar = node[0]
      let collection = node[1]
      let body = node[2]

      # CRITICAL FIX: Use immediately-invoked closure to force value capture
      # Without this, all closures share the same loop variable and see the final value
      let capturedParam = genSym(nskParam, "capturedIdx")
      let transformedBody = transformLoopVariableReferences(body, loopVar, capturedParam)

      # Create immediately-invoked closure: (proc(capturedIdx: typeof(loopVar)) = body)(loopVar)
      let closureProc = newProc(
        name = newEmptyNode(),
        params = [newEmptyNode(), newIdentDefs(capturedParam, newCall(ident("typeof"), loopVar))],
        body = transformedBody,
        procType = nnkLambda
      )
      closureProc.addPragma(ident("closure"))
      let closureCall = newCall(closureProc, loopVar)

      # Generate code that iterates at runtime and processes Tab components
      childStmtList.add newTree(nnkForStmt, loopVar, collection, newStmtList(closureCall))
    else:
      # Handle regular child nodes
      if node.kind == nnkCall and node[0].kind == nnkIdent and node[0].eqIdent("Tab"):
        # Tab components are statements that self-register, add them directly
        childStmtList.add(node)
      else:
        # Other components (like Button) return values - add them as children to TabBar
        let componentSym = genSym(nskLet, "tabBarChild")
        let addChildSym = bindSym("kryon_component_add_child")
        childStmtList.add quote do:
          let `componentSym` = `node`
          discard `addChildSym`(`ctxBarSym`, `componentSym`)

  result = quote do:
    block:
      let `barSym` = `containerCall`
      `registerSym`(`barSym`, `ctxSym`, `reorderableVal`)
      let `ctxBarSym` = `barSym`
      block:
        `childStmtList`
      `barSym`

macro TabContent*(props: untyped): untyped =
  let registerSym = bindSym("registerTabContent")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let addChildSym = bindSym("kryon_component_add_child")

  var propertyNodes: seq[NimNode] = @[]
  var childNodes: seq[NimNode] = @[]
  var hasLayout = false
  var hasFlex = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "layoutdirection":
        hasLayout = true
        propertyNodes.add(node)
      of "flexgrow":
        hasFlex = true
        propertyNodes.add(node)
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  var body = newTree(nnkStmtList)
  if not hasLayout:
    body.add newTree(nnkAsgn, ident("layoutDirection"), newIntLitNode(0))
  if not hasFlex:
    body.add newTree(nnkAsgn, ident("flexGrow"), newIntLitNode(1))
  for node in propertyNodes:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let contentSym = genSym(nskLet, "tabContent")

  var childStmtList = newStmtList()
  for node in childNodes:
    if isEchoStatement(node):
      childStmtList.add(node)
      continue
    if node.kind == nnkForStmt:
      # Handle runtime for loops with TabPanel components
      # Structure: nnkForStmt[loopVar, collection, body]
      let loopVar = node[0]
      let collection = node[1]
      let body = node[2]

      # CRITICAL FIX: Use immediately-invoked closure to force value capture
      # Without this, all closures share the same loop variable and see the final value
      let capturedParam = genSym(nskParam, "capturedIdx")
      let transformedBody = transformLoopVariableReferences(body, loopVar, capturedParam)

      # Generate code that iterates at runtime and processes TabPanel components
      # TabPanel components return values that need to be added as children
      # Use immediately-invoked closure to capture the loop variable value
      childStmtList.add quote do:
        for `loopVar` in `collection`:
          (proc(`capturedParam`: typeof(`loopVar`)) {.closure.} =
            let panelChild = block:
              `transformedBody`
            if panelChild != nil:
              discard `addChildSym`(`contentSym`, panelChild)
          )(`loopVar`)
    else:
      # Handle regular child nodes
      if node.kind == nnkCall and node[0].kind == nnkIdent and node[0].eqIdent("TabPanel"):
        # TabPanel components self-register and are added as children, so we need to capture the result
        childStmtList.add quote do:
          let panelChild = block:
            `node`
          if panelChild != nil:
            discard `addChildSym`(`contentSym`, panelChild)
      else:
        # Other components
        childStmtList.add quote do:
          let panelChild = block:
            `node`
          if panelChild != nil:
            discard `addChildSym`(`contentSym`, panelChild)

  result = quote do:
    block:
      let `contentSym` = `containerCall`
      `registerSym`(`contentSym`, `ctxSym`)
      block:
        `childStmtList`
      `contentSym`

macro TabPanel*(props: untyped): untyped =
  let registerSym = bindSym("registerTabPanel")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let panelIndexSym = ident("__kryonTabPanelIndex")

  var body = newTree(nnkStmtList)
  for node in props.children:
    body.add(node)

  let containerCall = newTree(nnkCall, ident("Container"), body)
  let panelSym = genSym(nskLet, "tabPanel")

  result = quote do:
    block:
      let `panelSym` = `containerCall`
      `registerSym`(`panelSym`, `ctxSym`, `panelIndexSym`)
      `panelIndexSym` += 1  # Increment for next panel
      `panelSym`

macro Tab*(props: untyped): untyped =
  let registerSym = bindSym("registerTabComponent")
  let visualType = bindSym("TabVisualState")
  let ctxSym = ident("__kryonCurrentTabGroup")
  let ctxBarSym = ident("__kryonCurrentTabBar")

  var buttonProps = newTree(nnkStmtList)
  var extraChildren: seq[NimNode] = @[]
  var titleVal: NimNode = newStrLitNode("Tab")
  var backgroundColorExpr: NimNode = newStrLitNode("#292C30")
  var activeBackgroundExpr: NimNode = newStrLitNode("#3C4047")
  var textColorExpr: NimNode = newStrLitNode("#C7C9CC")
  var activeTextExpr: NimNode = newStrLitNode("#FFFFFF")
  var onClickExpr: NimNode = nil
  var hasPadding = false
  var hasMargin = false
  var hasBackground = false
  var hasTextColor = false
  var hasBorderColor = false
  var hasBorderWidth = false
  var hasWidth = false

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "title":
        titleVal = node[1]
      of "background", "backgroundcolor":
        backgroundColorExpr = node[1]
        buttonProps.add(node)
        hasBackground = true
      of "activebackgroundcolor":
        activeBackgroundExpr = node[1]
      of "textcolor":
        textColorExpr = node[1]
        buttonProps.add(node)
        hasTextColor = true
      of "activetextcolor":
        activeTextExpr = node[1]
      of "bordercolor":
        hasBorderColor = true
        buttonProps.add(node)
      of "borderwidth":
        hasBorderWidth = true
        buttonProps.add(node)
      of "padding":
        hasPadding = true
        buttonProps.add(node)
      of "margin":
        hasMargin = true
        buttonProps.add(node)
      of "width":
        hasWidth = true
        buttonProps.add(node)
      of "onclick":
        onClickExpr = node[1]
      else:
        buttonProps.add(node)
    else:
        extraChildren.add(node)

  if not hasBackground:
    buttonProps.add newTree(nnkAsgn, ident("backgroundColor"), copyNimTree(backgroundColorExpr))
  if not hasTextColor:
    buttonProps.add newTree(nnkAsgn, ident("color"), copyNimTree(textColorExpr))
  if not hasBorderColor:
    buttonProps.add newTree(nnkAsgn, ident("borderColor"), newStrLitNode("#4C5057"))
  if not hasBorderWidth:
    buttonProps.add newTree(nnkAsgn, ident("borderWidth"), newIntLitNode(1))
  # Don't set fixed width - tabs will fill available space via flex_grow
  buttonProps.add newTree(nnkAsgn, ident("height"), newIntLitNode(32))
  buttonProps.add newTree(nnkAsgn, ident("alignItems"), newStrLitNode("center"))
  buttonProps.add newTree(nnkAsgn, ident("justifyContent"), newStrLitNode("center"))
  # Chrome-like tabs: min/max width for responsive behavior
  # minWidth=16: allows tabs to shrink very small (text fades out with ellipsis)
  # maxWidth=180: prevents excessive expansion (Chrome-like behavior)
  buttonProps.add newTree(nnkAsgn, ident("minWidth"), newIntLitNode(16))
  buttonProps.add newTree(nnkAsgn, ident("maxWidth"), newIntLitNode(180))
  buttonProps.add newTree(nnkAsgn, ident("minHeight"), newIntLitNode(28))

  # Tab default: text fades when overflowing (Chrome-like behavior)
  buttonProps.add newTree(nnkAsgn, ident("textFade"), newCall(ident("horizontal"), newIntLitNode(15)))
  buttonProps.add newTree(nnkAsgn, ident("textOverflow"), ident("fade"))

  buttonProps.add newTree(nnkAsgn, ident("text"), titleVal)

  # ALWAYS add onClick handler to ensure IR_EVENT_CLICK is created
  # This is required for automatic tab switching to work
  # The C renderer detects tab clicks via IR_EVENT_CLICK with "nim_button_" logic_id
  if onClickExpr != nil:
    # User provided onClick - use it
    buttonProps.add newTree(nnkAsgn, ident("onClick"), onClickExpr)
  else:
    # No onClick - add dummy handler to ensure event is created
    # The C layer handles tab switching, this just triggers event creation
    let dummyHandler = quote do:
      proc() = discard
    buttonProps.add newTree(nnkAsgn, ident("onClick"), dummyHandler)

  for child in extraChildren:
    buttonProps.add(child)

  let buttonCall = newTree(nnkCall, ident("Button"), buttonProps)
  let tabSym = genSym(nskLet, "tabComponent")
  let centerSetter = bindSym("kryon_button_set_center_text")
  let ellipsizeSetter = bindSym("kryon_button_set_ellipsize")
  var afterCreate = newStmtList()

  if not hasPadding:
    # Compact padding for tabs: 6px vertical, 10px horizontal
    afterCreate.add quote do:
      kryon_component_set_padding(`tabSym`, 6'u8, 10'u8, 6'u8, 10'u8)

  if not hasMargin:
    # Chrome-like tabs: minimal margin between tabs
    afterCreate.add quote do:
      kryon_component_set_margin(`tabSym`, 2'u8, 1'u8, 0'u8, 1'u8)

  # Flex behavior: tabs with explicit width should NOT shrink (flexShrink=0)
  # Tabs without explicit width fill available space (flexGrow=1, flexShrink=1)
  if hasWidth:
    afterCreate.add quote do:
      kryon_component_set_layout_alignment(`tabSym`, kaCenter, kaCenter)
      # Tab has fixed width - don't allow shrinking
      kryon_component_set_flex(`tabSym`, 0'u8, 0'u8)
      kryon_component_set_layout_direction(`tabSym`, 1)
      `centerSetter`(`tabSym`, false)
      `ellipsizeSetter`(`tabSym`, true)
  else:
    afterCreate.add quote do:
      kryon_component_set_layout_alignment(`tabSym`, kaCenter, kaCenter)
      # Tabs fill available space evenly (flexGrow=1, flexShrink=1)
      kryon_component_set_flex(`tabSym`, 1'u8, 1'u8)
      kryon_component_set_layout_direction(`tabSym`, 1)
      `centerSetter`(`tabSym`, false)
      `ellipsizeSetter`(`tabSym`, true)

  let visualNode = newTree(nnkObjConstr, visualType,
    newTree(nnkExprColonExpr, ident("backgroundColor"), colorNode(copyNimTree(backgroundColorExpr))),
    newTree(nnkExprColonExpr, ident("activeBackgroundColor"), colorNode(copyNimTree(activeBackgroundExpr))),
    newTree(nnkExprColonExpr, ident("textColor"), colorNode(copyNimTree(textColorExpr))),
    newTree(nnkExprColonExpr, ident("activeTextColor"), colorNode(copyNimTree(activeTextExpr)))
  )

  result = quote do:
    block:
      # Check if TabGroup context is available
      if `ctxSym` == nil:
        echo "[kryon][error] Tab component created outside of TabGroup context: ", `titleVal`
        discard ()

      let `tabSym` = `buttonCall`
      if `tabSym` == nil:
        echo "[kryon][error] Failed to create tab button: ", `titleVal`
        discard ()

      `afterCreate`
      # Ensure the tab component is visible by marking dirty
      kryon_component_mark_dirty(`tabSym`)
      # Register tab with current tab group (index handled in backend; use 0 placeholder)
      `registerSym`(`tabSym`, `ctxSym`, `visualNode`, 0)
      # Attach tab button into current TabBar so it renders
      if `ctxBarSym` != nil:
        discard kryon_component_add_child(`ctxBarSym`, `tabSym`)
      echo "[kryon][debug] Created tab button: ", `titleVal`, " component: ", cast[uint](`tabSym`)
      # Tab components are registered with the tab group, explicitly return void
      discard ()

# ============================================================================
# Static For Loop Macro
# ============================================================================

macro staticFor*(loopStmt, bodyStmt: untyped): untyped =
  ## Static for loop macro that expands at compile time
  ## Usage: staticFor(item in items):
  ##           Component:
  ##             property = item.value
  ## This expands the loop at compile time, generating multiple components
  ##
  ## NOTE: The collection must be a compile-time constant (const or literal)

  # Parse the loop statement (e.g., "item in items")
  expectKind loopStmt, nnkInfix

  let loopVar = loopStmt[1]      # "item" - the loop variable
  let collection = loopStmt[2]   # "items" - the collection to iterate over

  # Create a result statement list that will hold all expanded components
  result = newStmtList()

  # The strategy: use untyped template parameters and macro.getType
  # to evaluate the collection at compile time, then substitute values

  # Helper proc to replace all occurrences of loopVar with a value in the AST
  proc replaceIdent(node: NimNode, oldIdent: NimNode, newNode: NimNode): NimNode =
    case node.kind
    of nnkIdent:
      if node.eqIdent(oldIdent):
        return copyNimTree(newNode)
      else:
        return node
    of nnkEmpty, nnkNilLit:
      return node
    of nnkCharLit..nnkUInt64Lit, nnkFloatLit..nnkFloat128Lit, nnkStrLit..nnkTripleStrLit:
      return node
    else:
      result = copyNimNode(node)
      for child in node:
        result.add replaceIdent(child, oldIdent, newNode)

  # Try to evaluate the collection at compile time
  # This works if collection is a const or can be resolved at compile time
  let collectionValue = collection

  # Generate a unique symbol for storing collection length
  let lenSym = genSym(nskConst, "staticForLen")

  # Build the unrolled loop by generating index-based iteration
  # We create compile-time when branches for each possible index
  result = quote do:
    const `lenSym` = len(`collectionValue`)

  # Add each iteration as a separate when branch
  # This is a workaround since we can't truly iterate at macro expansion time
  # We'll generate code for indices 0..99 (should be enough for most cases)
  for i in 0 ..< 100:  # Maximum 100 iterations supported
    let iLit = newLit(i)
    let itemAccess = quote do:
      `collectionValue`[`iLit`]

    # Replace the loop variable with the item access in a copy of the body
    let expandedBody = replaceIdent(bodyStmt, loopVar, itemAccess)

    # CRITICAL FIX: Apply the same symbol isolation as the static: for construct
    # Use unique symbols and block wrapping to prevent AST reuse
    let resultSym = genSym(nskLet, "staticForResult")
    let iterationBlock = ident("iteration")

    result.add quote do:
      when `iLit` < `lenSym`:
        block `iterationBlock`:
          let `resultSym` = `expandedBody`
          # Add to parent container if this is a component
          when compiles(`resultSym`):
            when compiles(kryon_component_add_child):
              when compiles(staticForParent):
                discard kryon_component_add_child(staticForParent, `resultSym`)

# ============================================================================
# DSL Implementation Complete
# ============================================================================
# New input components to add to impl.nim after the Spacer macro

macro RadioGroup*(props: untyped): untyped =
  ## Radio button group component  
  ## Groups radio buttons and manages single selection
  var
    groupName = genSym(nskLet, "radioGroup")
    nameVal = newStrLitNode("radiogroup")
    selectedIndexVal = newIntLitNode(0)
    childNodes: seq[NimNode] = @[]
    gapVal = newIntLitNode(8)
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "name":
        nameVal = node[1]
      of "selectedindex":
        selectedIndexVal = node[1]
      of "gap":
        gapVal = node[1]
      else:
        discard
    else:
      childNodes.add(node)
  
  result = quote do:
    block:
      let `groupName` = Column:
        gap = `gapVal`
      `groupName`


macro RadioButton*(props: untyped): untyped =
  ## Individual radio button component
  var
    labelVal = newStrLitNode("")
    valueVal = newIntLitNode(0)
    clickHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "label":
        labelVal = node[1]
      of "value":
        valueVal = node[1]
      of "onclick":
        clickHandler = node[1]
      else:
        discard
  
  # For now, render as checkbox until we have proper radio support
  result = quote do:
    Checkbox:
      label = `labelVal`
      checked = false
      onClick = `clickHandler`

macro Slider*(props: untyped): untyped =
  ## Slider/Range input component
  var
    valueVal = newIntLitNode(50)
    minVal = newIntLitNode(0)
    maxVal = newIntLitNode(100)
    stepVal = newIntLitNode(1)
    widthVal = newIntLitNode(200)
    heightVal = newIntLitNode(30)
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "value":
        valueVal = node[1]
      of "min":
        minVal = node[1]
      of "max":
        maxVal = node[1]
      of "step":
        stepVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "onchange":
        changeHandler = node[1]
      else:
        discard
  
  # Placeholder: render as a container with text showing value
  result = quote do:
    Container:
      width = `widthVal`
      height = `heightVal`
      backgroundColor = "#ddd"
      layoutDirection = 1  # Row
      
      # Progress bar
      Container:
        width = (`valueVal` * `widthVal`) div `maxVal`
        height = `heightVal`
        backgroundColor = "#3498db"

macro TextArea*(props: untyped): untyped =
  ## Multi-line text input component
  var
    textareaName = genSym(nskLet, "textarea")
    placeholderVal = newStrLitNode("")
    valueVal = newStrLitNode("")

    widthVal = newIntLitNode(300)
    heightVal = newIntLitNode(100)
    backgroundColorVal: NimNode = nil
    textColorVal: NimNode = nil
    borderColorVal: NimNode = nil
    borderWidthVal: NimNode = nil
    borderRadiusVal: NimNode = nil
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = node[1]
      of "value":
        valueVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "background", "backgroundcolor":
        backgroundColorVal = colorNode(node[1])
      of "textcolor", "color":
        textColorVal = colorNode(node[1])
      of "bordercolor":
        borderColorVal = colorNode(node[1])
      of "borderwidth":
        borderWidthVal = node[1]
      of "borderradius":
        borderRadiusVal = node[1]
      of "ontextchange":
        changeHandler = node[1]
      else:
        discard
  
  var initStmts = newTree(nnkStmtList)
  
  if backgroundColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_background_color(`textareaName`, `backgroundColorVal`)
  
  if textColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_text_color(`textareaName`, `textColorVal`)
  
  if borderColorVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_color(`textareaName`, `borderColorVal`)
  
  if borderWidthVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_width(`textareaName`, uint8(`borderWidthVal`))

  if borderRadiusVal != nil:
    initStmts.add quote do:
      kryon_component_set_border_radius(`textareaName`, uint8(`borderRadiusVal`))

  result = quote do:
    block:
      let `textareaName` = newKryonContainer()
      kryon_component_set_bounds(`textareaName`,
        toFixed(0), toFixed(0), toFixed(`widthVal`), toFixed(`heightVal`))
      `initStmts`
      # TODO: Implement actual multiline text editing
      # For now, just a container
      `textareaName`

macro ColorPicker*(props: untyped): untyped =
  ## Color picker component
  var
    valueVal = newStrLitNode("#000000")
    widthVal = newIntLitNode(60)
    heightVal = newIntLitNode(35)
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "value":
        valueVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "onchange":
        changeHandler = node[1]
      else:
        discard
  
  result = quote do:
    Container:
      width = `widthVal`
      height = `heightVal`
      backgroundColor = `valueVal`
      borderColor = "#333"
      borderWidth = 1

macro FileInput*(props: untyped): untyped =
  ## File upload input component
  var
    placeholderVal = newStrLitNode("Choose file...")
    acceptVal = newStrLitNode("*")
    widthVal = newIntLitNode(300)
    heightVal = newIntLitNode(35)
    changeHandler = newNilLit()
    
  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii():
      of "placeholder":
        placeholderVal = node[1]
      of "accept":
        acceptVal = node[1]
      of "width":
        widthVal = node[1]
      of "height":
        heightVal = node[1]
      of "onchange":
        changeHandler = node[1]
      else:
        discard
  
  result = quote do:
    Button:
      text = `placeholderVal`
      width = `widthVal`
      height = `heightVal`
      backgroundColor = "#ecf0f1"
      textColor = "#7f8c8d"
      onClick = proc() =
        echo "File input clicked - not yet implemented"

# ============================================================================
# Tables
# ============================================================================

macro Table*(props: untyped): untyped =
  ## Table component macro - creates an HTML-like table structure
  ## Supports styling: borderColor, cellPadding, striped, showBorders, headerBackground
  let createStateSym = bindSym("ir_table_create_state")
  let finalizeSym = bindSym("ir_table_finalize")
  let addChildSym = bindSym("kryon_component_add_child")
  let ir_tableSym = bindSym("ir_table")
  let ctxSym = ident("__kryonCurrentTable")
  let ctxStateSym = ident("__kryonCurrentTableState")

  var propertyNodes: seq[NimNode] = @[]
  var childNodes: seq[NimNode] = @[]

  # Table-specific properties
  var borderColorVal: NimNode = nil
  var headerBackgroundVal: NimNode = nil
  var evenRowBackgroundVal: NimNode = nil
  var oddRowBackgroundVal: NimNode = nil
  var borderWidthVal: NimNode = nil
  var cellPaddingVal: NimNode = nil
  var showBordersVal: NimNode = nil
  var stripedVal: NimNode = nil

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "bordercolor":
        borderColorVal = node[1]
      of "headerbackground", "headerbackgroundcolor":
        headerBackgroundVal = node[1]
      of "evenrowbackground", "evenrowcolor":
        evenRowBackgroundVal = node[1]
      of "oddrowbackground", "oddrowcolor":
        oddRowBackgroundVal = node[1]
      of "borderwidth":
        borderWidthVal = node[1]
      of "cellpadding":
        cellPaddingVal = node[1]
      of "showborders":
        showBordersVal = node[1]
      of "striped":
        stripedVal = node[1]
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  let tableSym = genSym(nskLet, "table")
  let stateSym = genSym(nskLet, "tableState")

  # Build state initialization statements
  var stateInitStmts = newStmtList()

  if borderColorVal != nil:
    let colorExpr = if borderColorVal.kind == nnkStrLit:
      newCall(ident("parseColor"), borderColorVal)
    else:
      borderColorVal
    let setBorderColorSym = bindSym("ir_table_set_border_color")
    stateInitStmts.add quote do:
      let bc = `colorExpr`
      `setBorderColorSym`(`stateSym`, bc.r, bc.g, bc.b, bc.a)

  if headerBackgroundVal != nil:
    let colorExpr = if headerBackgroundVal.kind == nnkStrLit:
      newCall(ident("parseColor"), headerBackgroundVal)
    else:
      headerBackgroundVal
    let setHeaderBgSym = bindSym("ir_table_set_header_background")
    stateInitStmts.add quote do:
      let hbg = `colorExpr`
      `setHeaderBgSym`(`stateSym`, hbg.r, hbg.g, hbg.b, hbg.a)

  if evenRowBackgroundVal != nil and oddRowBackgroundVal != nil:
    let evenExpr = if evenRowBackgroundVal.kind == nnkStrLit:
      newCall(ident("parseColor"), evenRowBackgroundVal)
    else:
      evenRowBackgroundVal
    let oddExpr = if oddRowBackgroundVal.kind == nnkStrLit:
      newCall(ident("parseColor"), oddRowBackgroundVal)
    else:
      oddRowBackgroundVal
    let setRowBgSym = bindSym("ir_table_set_row_backgrounds")
    stateInitStmts.add quote do:
      let erbg = `evenExpr`
      let orbg = `oddExpr`
      `setRowBgSym`(`stateSym`, erbg.r, erbg.g, erbg.b, erbg.a, orbg.r, orbg.g, orbg.b, orbg.a)

  if borderWidthVal != nil:
    let setBorderWidthSym = bindSym("ir_table_set_border_width")
    stateInitStmts.add quote do:
      `setBorderWidthSym`(`stateSym`, cfloat(`borderWidthVal`))

  if cellPaddingVal != nil:
    let setCellPaddingSym = bindSym("ir_table_set_cell_padding")
    stateInitStmts.add quote do:
      `setCellPaddingSym`(`stateSym`, cfloat(`cellPaddingVal`))

  if showBordersVal != nil:
    let setShowBordersSym = bindSym("ir_table_set_show_borders")
    stateInitStmts.add quote do:
      `setShowBordersSym`(`stateSym`, `showBordersVal`)

  if stripedVal != nil:
    let setStripedSym = bindSym("ir_table_set_striped")
    stateInitStmts.add quote do:
      `setStripedSym`(`stateSym`, `stripedVal`)

  # Apply general properties to table component
  var propInitStmts = newStmtList()
  for node in propertyNodes:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "width":
        let val = node[1]
        propInitStmts.add quote do:
          kryon_component_set_width(`tableSym`, toFixed(`val`))
      of "height":
        let val = node[1]
        propInitStmts.add quote do:
          kryon_component_set_height(`tableSym`, toFixed(`val`))
      of "background", "backgroundcolor":
        let colorExpr = if node[1].kind == nnkStrLit:
          newCall(ident("parseColor"), node[1])
        else:
          node[1]
        propInitStmts.add quote do:
          kryon_component_set_background_color(`tableSym`, `colorExpr`)
      else:
        discard

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`tableSym`, childComponent)

  result = quote do:
    block:
      let `tableSym` = `ir_tableSym`()
      let `stateSym` = `createStateSym`()
      let `ctxSym` = `tableSym`
      let `ctxStateSym` = `stateSym`
      `stateInitStmts`
      `propInitStmts`
      # Attach state to table component
      cast[ptr IRComponent](`tableSym`).custom_data = cast[pointer](`stateSym`)
      block:
        `childStmtList`
      `finalizeSym`(`tableSym`)
      `tableSym`

macro TableHead*(props: untyped): untyped =
  ## TableHead section - contains header rows
  let addChildSym = bindSym("kryon_component_add_child")
  let ir_table_headSym = bindSym("ir_table_head")
  let ctxSym = ident("__kryonCurrentTable")
  let ctxHeadSym = ident("__kryonCurrentTableSection")
  let ctxIsHeaderSym = ident("__kryonTableIsHeader")

  var childNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind != nnkAsgn:
      childNodes.add(node)

  let headSym = genSym(nskLet, "tableHead")

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`headSym`, childComponent)

  result = quote do:
    block:
      let `headSym` = `ir_table_headSym`()
      let `ctxHeadSym` = `headSym`
      let `ctxIsHeaderSym` = true
      block:
        `childStmtList`
      `headSym`

macro TableBody*(props: untyped): untyped =
  ## TableBody section - contains data rows
  let addChildSym = bindSym("kryon_component_add_child")
  let ir_table_bodySym = bindSym("ir_table_body")
  let ctxSym = ident("__kryonCurrentTable")
  let ctxBodySym = ident("__kryonCurrentTableSection")
  let ctxIsHeaderSym = ident("__kryonTableIsHeader")

  var childNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind != nnkAsgn:
      childNodes.add(node)

  let bodySym = genSym(nskLet, "tableBody")

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`bodySym`, childComponent)

  result = quote do:
    block:
      let `bodySym` = `ir_table_bodySym`()
      let `ctxBodySym` = `bodySym`
      let `ctxIsHeaderSym` = false
      block:
        `childStmtList`
      `bodySym`

macro TableFoot*(props: untyped): untyped =
  ## TableFoot section - contains footer rows
  let addChildSym = bindSym("kryon_component_add_child")
  let ir_table_footSym = bindSym("ir_table_foot")
  let ctxSym = ident("__kryonCurrentTable")
  let ctxFootSym = ident("__kryonCurrentTableSection")
  let ctxIsHeaderSym = ident("__kryonTableIsHeader")

  var childNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind != nnkAsgn:
      childNodes.add(node)

  let footSym = genSym(nskLet, "tableFoot")

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`footSym`, childComponent)

  result = quote do:
    block:
      let `footSym` = `ir_table_footSym`()
      let `ctxFootSym` = `footSym`
      let `ctxIsHeaderSym` = false
      block:
        `childStmtList`
      `footSym`

macro Tr*(props: untyped): untyped =
  ## Table Row component
  let addChildSym = bindSym("kryon_component_add_child")
  let ir_table_rowSym = bindSym("ir_table_row")
  let ctxSectionSym = ident("__kryonCurrentTableSection")
  let ctxRowSym = ident("__kryonCurrentTableRow")

  var childNodes: seq[NimNode] = @[]
  var propertyNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind == nnkAsgn:
      propertyNodes.add(node)
    else:
      childNodes.add(node)

  let rowSym = genSym(nskLet, "tableRow")

  # Apply row properties
  var propInitStmts = newStmtList()
  for node in propertyNodes:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "background", "backgroundcolor":
        let colorExpr = if node[1].kind == nnkStrLit:
          newCall(ident("parseColor"), node[1])
        else:
          node[1]
        propInitStmts.add quote do:
          kryon_component_set_background_color(`rowSym`, `colorExpr`)
      else:
        discard

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`rowSym`, childComponent)

  result = quote do:
    block:
      let `rowSym` = `ir_table_rowSym`()
      let `ctxRowSym` = `rowSym`
      `propInitStmts`
      block:
        `childStmtList`
      `rowSym`

macro Td*(props: untyped): untyped =
  ## Table Data Cell component
  ## Supports: colspan, rowspan, alignment, text, children
  let addChildSym = bindSym("kryon_component_add_child")
  let ir_table_cellSym = bindSym("ir_table_cell")
  let setAlignSym = bindSym("ir_table_cell_set_alignment")
  let ctxRowSym = ident("__kryonCurrentTableRow")

  var childNodes: seq[NimNode] = @[]
  var colspanVal: NimNode = newIntLitNode(1)
  var rowspanVal: NimNode = newIntLitNode(1)
  var alignmentVal: NimNode = nil
  var textVal: NimNode = nil
  var backgroundVal: NimNode = nil
  var textColorVal: NimNode = nil
  var propertyNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "colspan":
        colspanVal = node[1]
      of "rowspan":
        rowspanVal = node[1]
      of "align", "alignment", "textalign":
        alignmentVal = node[1]
      of "text", "content":
        textVal = node[1]
      of "background", "backgroundcolor":
        backgroundVal = node[1]
      of "textcolor", "color":
        textColorVal = node[1]
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  let cellSym = genSym(nskLet, "tableCell")

  var initStmts = newStmtList()

  # Handle alignment
  if alignmentVal != nil:
    # Convert string alignment to IRAlignment enum
    initStmts.add quote do:
      let alignStr = $(`alignmentVal`)
      let align = case alignStr.toLowerAscii()
        of "left": kaStart
        of "center": kaCenter
        of "right": kaEnd
        else: kaStart
      `setAlignSym`(`cellSym`, align)

  # Handle background color
  if backgroundVal != nil:
    let colorExpr = if backgroundVal.kind == nnkStrLit:
      newCall(ident("parseColor"), backgroundVal)
    else:
      backgroundVal
    initStmts.add quote do:
      kryon_component_set_background_color(`cellSym`, `colorExpr`)

  # Handle text content
  if textVal != nil:
    let colorExpr = if textColorVal != nil:
      if textColorVal.kind == nnkStrLit:
        newCall(ident("parseColor"), textColorVal)
      else:
        textColorVal
    else:
      newCall(ident("parseColor"), newStrLitNode("#000000"))

    let textSym = genSym(nskLet, "cellText")
    initStmts.add quote do:
      let `textSym` = newKryonText($`textVal`)
      kryon_component_set_text_color(`textSym`, `colorExpr`)
      discard `addChildSym`(`cellSym`, `textSym`)

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`cellSym`, childComponent)

  result = quote do:
    block:
      let `cellSym` = `ir_table_cellSym`(uint16(`colspanVal`), uint16(`rowspanVal`))
      `initStmts`
      block:
        `childStmtList`
      `cellSym`

macro Th*(props: untyped): untyped =
  ## Table Header Cell component (like Td but with header styling)
  ## Supports: colspan, rowspan, alignment, text, children
  let addChildSym = bindSym("kryon_component_add_child")
  let ir_table_header_cellSym = bindSym("ir_table_header_cell")
  let setAlignSym = bindSym("ir_table_cell_set_alignment")
  let ctxRowSym = ident("__kryonCurrentTableRow")

  var childNodes: seq[NimNode] = @[]
  var colspanVal: NimNode = newIntLitNode(1)
  var rowspanVal: NimNode = newIntLitNode(1)
  var alignmentVal: NimNode = nil
  var textVal: NimNode = nil
  var backgroundVal: NimNode = nil
  var textColorVal: NimNode = nil
  var propertyNodes: seq[NimNode] = @[]

  for node in props.children:
    if node.kind == nnkAsgn:
      let propName = $node[0]
      case propName.toLowerAscii()
      of "colspan":
        colspanVal = node[1]
      of "rowspan":
        rowspanVal = node[1]
      of "align", "alignment", "textalign":
        alignmentVal = node[1]
      of "text", "content":
        textVal = node[1]
      of "background", "backgroundcolor":
        backgroundVal = node[1]
      of "textcolor", "color":
        textColorVal = node[1]
      else:
        propertyNodes.add(node)
    else:
      childNodes.add(node)

  let cellSym = genSym(nskLet, "tableHeaderCell")

  var initStmts = newStmtList()

  # Handle alignment
  if alignmentVal != nil:
    initStmts.add quote do:
      let alignStr = $(`alignmentVal`)
      let align = case alignStr.toLowerAscii()
        of "left": kaStart
        of "center": kaCenter
        of "right": kaEnd
        else: kaStart
      `setAlignSym`(`cellSym`, align)

  # Handle background color
  if backgroundVal != nil:
    let colorExpr = if backgroundVal.kind == nnkStrLit:
      newCall(ident("parseColor"), backgroundVal)
    else:
      backgroundVal
    initStmts.add quote do:
      kryon_component_set_background_color(`cellSym`, `colorExpr`)

  # Handle text content (default bold for headers)
  if textVal != nil:
    let colorExpr = if textColorVal != nil:
      if textColorVal.kind == nnkStrLit:
        newCall(ident("parseColor"), textColorVal)
      else:
        textColorVal
    else:
      newCall(ident("parseColor"), newStrLitNode("#000000"))

    let textSym = genSym(nskLet, "headerText")
    initStmts.add quote do:
      let `textSym` = newKryonText($`textVal`)
      kryon_component_set_text_color(`textSym`, `colorExpr`)
      kryon_component_set_font_weight(`textSym`, 700)  # Bold for headers
      discard `addChildSym`(`cellSym`, `textSym`)

  var childStmtList = newStmtList()
  for child in childNodes:
    if isEchoStatement(child):
      childStmtList.add(child)
      continue
    childStmtList.add quote do:
      let childComponent = block:
        `child`
      if childComponent != nil:
        discard `addChildSym`(`cellSym`, childComponent)

  result = quote do:
    block:
      let `cellSym` = `ir_table_header_cellSym`(uint16(`colspanVal`), uint16(`rowspanVal`))
      `initStmts`
      block:
        `childStmtList`
      `cellSym`
