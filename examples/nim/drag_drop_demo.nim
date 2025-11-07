## Drag and Drop Demo
##
## Demonstrates attaching Draggable and DropTarget behaviors to elements using 'with' syntax
## Shows how to actually move UI elements from source to drop zone

import kryon_dsl

# Define a DragDropComponent with local state (like counters demo)
proc DragDropComponent*(): Element =
  # Local state variables - these are tracked by Kryon's automatic reactivity
  var sourceItems = @["Item 1", "Item 2", "Item 3"]
  var droppedItems: seq[string] = @[]

  # Event handlers that modify this component's state
  proc handleDrop(data: string) =
    echo "[DROP] Dropped: ", data
    # Move the item from sourceItems to droppedItems
    let index = sourceItems.find(data)
    if index != -1:
      sourceItems.delete(index)
      droppedItems.add(data)

  proc handleReset() =
    echo "[RESET] Reset drop zone"
    # Return all items back to sourceItems
    for item in droppedItems:
      sourceItems.add(item)
    droppedItems.setLen(0)

  # Return the UI
  result = Column:
    gap = 30
    padding = 40

    # Title
    Text:
      text = "Drag & Drop Demo"
      fontSize = 36
      color = "#ffffff"

    # Instructions
    Text:
      text = "Click and drag containers from the source area to the drop zone below"
      fontSize = 16
      color = "#aaaaaa"

    # Source area with draggable items
    Column:
      gap = 15

      Text:
        text = "Draggable Containers:"
        fontSize = 20
        color = "#4a90e2"

      Row:
        gap = 20

        for i in 0..<sourceItems.len:
          Container:
            width = 150
            height = 100
            backgroundColor = "#2a2a2a"
            borderWidth = 2
            borderColor = "#4a4a4a"
            padding = 10

            # This Container becomes draggable with the 'with' syntax
            with Draggable:
              itemType = "demo-item"
              data = sourceItems[i]

            # Content
            Center:
              Column:
                gap = 5

                Text:
                  text = sourceItems[i]
                  color = "#ffffff"
                  fontSize = 18

                Text:
                  text = "Drag me"
                  color = "#888888"
                  fontSize = 12

    # Drop zone area
    Column:
      gap = 15

      Text:
        text = "Drop Zone:"
        fontSize = 20
        color = "#52c41a"

      Container:
        width = 500
        height = 200
        backgroundColor = "#1e1e1e"
        borderWidth = 3
        borderColor = "#52c41a"
        padding = 20

        # This Container becomes a drop target with the 'with' syntax
        with DropTarget:
          itemType = "demo-item"
          onDrop = handleDrop

        # Content shows dropped items or placeholder
        if droppedItems.len > 0:
          Column:
            gap = 15

            Row:
              gap = 10

              for item in droppedItems:
                Container:
                  width = 120
                  height = 60
                  backgroundColor = "#2a4a2a"
                  borderWidth = 1
                  borderColor = "#52c41a"
                  padding = 8

                  Center:
                    Text:
                      text = item
                      color = "#52c41a"
                      fontSize = 14

            Button:
              text = "Reset All"
              backgroundColor = "#ff4d4f"
              color = "#ffffff"
              onClick = handleReset
              width = 120
              height = 40
        else:
          Center:
            Column:
              gap = 5

              Text:
                text = "Drop containers here"
                color = "#666666"
                fontSize = 20

              Text:
                text = "v"
                color = "#444444"
                fontSize = 32

    # Legend
    Column:
      gap = 10

      Text:
        text = "How it works:"
        fontSize = 16
        color = "#888888"

      Text:
        text = "• Containers use 'with Draggable' to become draggable"
        fontSize = 14
        color = "#666666"

      Text:
        text = "• Drop zone uses 'with DropTarget' to accept drops"
        fontSize = 14
        color = "#666666"

      Text:
        text = "• UI elements actually move from source to drop zone"
        fontSize = 14
        color = "#666666"

# Define the UI using the component
let app = kryonApp:
  Header:
    windowWidth = 900
    windowHeight = 700
    windowTitle = "Drag & Drop Demo - Kryon DSL"

  Body:
    backgroundColor = "#1a1a1a"

    DragDropComponent()
