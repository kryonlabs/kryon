// Simple Todo App - TypeScript/JSX
//
// A simple todo application demonstrating for directive with reactive variables

import { run } from '../../bindings/typescript/src/index.ts';

// State
let todos = ["Test todo", "Another item"];
let newTodo = "";

function addTodoHandler() {
  if (newTodo !== "") {
    todos.push(newTodo);
    newTodo = "";
    console.log("Added todo. Total:", todos.length);
  }
}

function App() {
  return (
    <column width="100%" height="100%" backgroundColor="#f0f0f0" gap={20} padding={30}>
      <text content="Simple Todo App" fontSize={28} color="#333333" />

      <row gap={10}>
        <input
          placeholder="Enter a new todo..."
          width={400}
          height={40}
          backgroundColor="#ffffff"
          padding={10}
        />
        <button
          title="Add"
          width={80}
          height={40}
          backgroundColor="#007bff"
          onClick={addTodoHandler}
        />
      </row>

      <column gap={5}>
        <text content="Todo List:" fontSize={18} color="#333333" />
        {todos.map((todo) => (
          <text
            key={todo}
            content={`- ${todo} (len=${todos.length})`}
            fontSize={16}
            color="#333333"
          />
        ))}
      </column>
    </column>
  );
}

run(<App />, {
  title: 'Simple Todo App',
  width: 600,
  height: 500,
});
