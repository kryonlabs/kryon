import std/[strutils, tables, json, os, sequtils]

type
  KytEventType* = enum
    kevWait = "wait"
    kevClick = "click"
    kevKeyPress = "key_press"
    kevTextInput = "text_input"
    kevMouseMove = "mouse_move"
    kevScreenshot = "screenshot"

  KytEvent* = object
    eventType*: KytEventType
    frames*: int
    componentId*: int
    x*, y*: int
    key*: string
    text*: string
    path*: string
    comment*: string

  KytTestSection* = object
    events*: seq[KytEvent]

  KytTest* = object
    name*: string
    target*: string
    description*: string
    setup*: KytTestSection
    scenario*: KytTestSection
    teardown*: KytTestSection

proc parseKytFile*(filepath: string): KytTest =
  ## Parse a .kyt file into a KytTest structure

  if not fileExists(filepath):
    raise newException(IOError, "File not found: " & filepath)

  let content = readFile(filepath)
  var lines = content.splitLines()

  result = KytTest()
  var currentSection = ""
  var currentEvent: KytEvent
  var inEvent = false
  var eventType = ""

  for lineNum, rawLine in lines:
    let line = rawLine.strip()

    # Skip empty lines and comments
    if line.len == 0 or line.startsWith("#"):
      continue

    # Check for sections
    if line == "Test:":
      currentSection = "test"
      continue
    elif line == "Setup:":
      currentSection = "setup"
      continue
    elif line == "Scenario:":
      currentSection = "scenario"
      continue
    elif line == "Teardown:":
      currentSection = "teardown"
      continue

    # Parse properties
    if "=" in line and not line.endsWith(":"):
      let parts = line.split("=", maxsplit=1)
      if parts.len == 2:
        let key = parts[0].strip()
        let value = parts[1].strip().strip(chars={'\"', ' '})

        case currentSection
        of "test":
          case key
          of "name": result.name = value
          of "target": result.target = value
          of "description": result.description = value
          else: discard
        else:
          if inEvent:
            case key
            of "frames": currentEvent.frames = parseInt(value)
            of "component_id": currentEvent.componentId = parseInt(value)
            of "x": currentEvent.x = parseInt(value)
            of "y": currentEvent.y = parseInt(value)
            of "key": currentEvent.key = value
            of "text": currentEvent.text = value
            of "path": currentEvent.path = value
            of "comment": currentEvent.comment = value
            else: discard
      continue

    # Check for event blocks
    if line.endsWith(":"):
      # Save previous event if exists
      if inEvent:
        case currentSection
        of "setup": result.setup.events.add(currentEvent)
        of "scenario": result.scenario.events.add(currentEvent)
        of "teardown": result.teardown.events.add(currentEvent)
        else: discard

      # Start new event
      eventType = line[0..^2].strip()
      if eventType in ["wait", "click", "key_press", "text_input", "mouse_move", "screenshot"]:
        inEvent = true
        currentEvent = KytEvent()

        case eventType
        of "wait": currentEvent.eventType = kevWait
        of "click": currentEvent.eventType = kevClick
        of "key_press": currentEvent.eventType = kevKeyPress
        of "text_input": currentEvent.eventType = kevTextInput
        of "mouse_move": currentEvent.eventType = kevMouseMove
        of "screenshot": currentEvent.eventType = kevScreenshot
        else: inEvent = false

  # Save last event if exists
  if inEvent:
    case currentSection
    of "setup": result.setup.events.add(currentEvent)
    of "scenario": result.scenario.events.add(currentEvent)
    of "teardown": result.teardown.events.add(currentEvent)
    else: discard

proc kytToJson*(test: KytTest): JsonNode =
  ## Convert KytTest to JSON format for desktop_test_events.c

  var events = newJArray()

  # Add setup events
  for event in test.setup.events:
    var eventJson = newJObject()
    eventJson["type"] = %($event.eventType)

    case event.eventType
    of kevWait:
      eventJson["frames"] = %event.frames
    of kevClick:
      if event.componentId > 0:
        eventJson["component_id"] = %event.componentId
      else:
        eventJson["x"] = %event.x
        eventJson["y"] = %event.y
    of kevKeyPress:
      eventJson["key"] = %event.key
    of kevTextInput:
      eventJson["text"] = %event.text
    of kevMouseMove:
      eventJson["x"] = %event.x
      eventJson["y"] = %event.y
    of kevScreenshot:
      eventJson["path"] = %event.path

    if event.comment.len > 0:
      eventJson["comment"] = %event.comment

    events.add(eventJson)

  # Add scenario events
  for event in test.scenario.events:
    var eventJson = newJObject()
    eventJson["type"] = %($event.eventType)

    case event.eventType
    of kevWait:
      eventJson["frames"] = %event.frames
    of kevClick:
      if event.componentId > 0:
        eventJson["component_id"] = %event.componentId
      else:
        eventJson["x"] = %event.x
        eventJson["y"] = %event.y
    of kevKeyPress:
      eventJson["key"] = %event.key
    of kevTextInput:
      eventJson["text"] = %event.text
    of kevMouseMove:
      eventJson["x"] = %event.x
      eventJson["y"] = %event.y
    of kevScreenshot:
      eventJson["path"] = %event.path

    if event.comment.len > 0:
      eventJson["comment"] = %event.comment

    events.add(eventJson)

  # Add teardown events
  for event in test.teardown.events:
    var eventJson = newJObject()
    eventJson["type"] = %($event.eventType)

    case event.eventType
    of kevWait:
      eventJson["frames"] = %event.frames
    of kevClick:
      if event.componentId > 0:
        eventJson["component_id"] = %event.componentId
      else:
        eventJson["x"] = %event.x
        eventJson["y"] = %event.y
    of kevKeyPress:
      eventJson["key"] = %event.key
    of kevTextInput:
      eventJson["text"] = %event.text
    of kevMouseMove:
      eventJson["x"] = %event.x
      eventJson["y"] = %event.y
    of kevScreenshot:
      eventJson["path"] = %event.path

    if event.comment.len > 0:
      eventJson["comment"] = %event.comment

    events.add(eventJson)

  result = newJObject()
  result["events"] = events

when isMainModule:
  # Test the parser
  if paramCount() < 1:
    echo "Usage: kyt_parser <file.kyt>"
    quit(1)

  let filepath = paramStr(1)
  let test = parseKytFile(filepath)

  echo "Test: ", test.name
  echo "Target: ", test.target
  echo "Setup events: ", test.setup.events.len
  echo "Scenario events: ", test.scenario.events.len
  echo "Teardown events: ", test.teardown.events.len
  echo ""
  echo "JSON output:"
  echo pretty(kytToJson(test))
