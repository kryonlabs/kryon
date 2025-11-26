// Counters Demo - TypeScript/JSX
//
// Demonstrates reusable Counter components with independent state
// Each counter maintains its own value and can be incremented/decremented independently

import { run } from '../../bindings/typescript/src/index.ts';

// Define a reusable Counter component
// Each counter gets its own state captured in a closure
function Counter({ initialValue = 0 }: { initialValue?: number }) {
  // Each counter instance has its own state variable
  let value = initialValue;

  // Event handlers that modify this counter's state
  function increment() {
    value += 1;
    console.log("Counter value:", value);
  }

  function decrement() {
    value -= 1;
    console.log("Counter value:", value);
  }

  // Return the counter UI
  return (
    <row align="center" gap={32}>
      <button
        title="-"
        width={60}
        height={50}
        backgroundColor="#E74C3C"
        onClick={decrement}
      />
      <text content={String(value)} fontSize={32} color="#FFFFFF" />
      <button
        title="+"
        width={60}
        height={50}
        backgroundColor="#2ECC71"
        onClick={increment}
      />
    </row>
  );
}

// Main App
function App() {
  return (
    <column
      width="100%"
      height="100%"
      backgroundColor="#1E1E1E"
      justify="center"
      align="center"
      gap={20}
    >
      <Counter initialValue={5} />
      <Counter />
    </column>
  );
}

run(<App />, {
  title: 'Counters Demo',
  width: 800,
  height: 600,
});
