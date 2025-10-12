import ../src/kryon

var todos = @["Test todo", "Another item"]
var newTodo = ""

proc addTodoHandler() =
  echo "🔥🔥🔥 addTodo called!"
  echo "=== addTodo called ==="
  echo "newTodo before: '" & newTodo & "'"
  echo "todos before: " & $todos
  echo "todos length: " & $todos.len

  if newTodo != "":
    echo "Adding todo: " & newTodo
    todos.add(newTodo)
    echo "Added! New todos length: " & $todos.len
    echo "todos after add: " & $todos
    newTodo = ""
    echo "newTodo after clearing: '" & newTodo & "'"

    # The reactive system will automatically invalidate dependent elements
    # through the createReactiveEventHandler wrapper
  else:
    echo "newTodo is empty, not adding"

  echo "=== end addTodo ==="

# Create a reactive version that automatically invalidates "todos" and "newTodo" variables
let addTodo = createReactiveEventHandler(addTodoHandler, @["todos", "newTodo"])

let app = kryonApp:
  Header:
    title = "Simple Todo App"
    description = "A simple todo application demonstrating for directive with reactive variables"

  Body:
    background = "#f0f0f0"

    Column:
      gap = 20
      padding = 30

      Text:
        text = "Simple Todo App"
        fontSize = 28
        color = "#333333"

      Row:
        gap = 10

        Input:
          placeholder = "Enter a new todo..."
          width = 400
          height = 40
          backgroundColor = "#ffffff"
          borderColor = "#cccccc"
          borderWidth = 1
          padding = 10
          value = newTodo

        Button:
          text = "Add"
          width = 80
          height = 40
          background = "#007bff"
          color = "#ffffff"
          onClick = addTodo

      Column:
        gap = 5

        Text:
          text = "Todo List:"
          fontSize = 18
          color = "#333333"

        for todo in todos:
          Text:
            text = "- " & todo & " (len=" & $todos.len & ")"
            fontSize = 16
            color = "#333333"

