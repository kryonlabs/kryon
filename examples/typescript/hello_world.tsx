// Kryon Hello World - TypeScript/JSX Example
import { run } from '../../bindings/typescript/src/index.ts';

function App() {
  return (
    <column width="100%" height="100%" gap={20} align="center" justify="center" backgroundColor="#1a1a2e">
      <text content="Hello from Kryon!" fontSize={32} color="#ffffff" />
      <text content="TypeScript + JSX + Bun FFI" fontSize={18} color="#888888" />
      <row gap={10}>
        <button title="Click Me" backgroundColor="#4a4a6a" />
        <button title="Another Button" backgroundColor="#6a4a4a" />
      </row>
    </column>
  );
}

run(<App />, {
  title: 'Kryon Hello World',
  width: 800,
  height: 600,
});
