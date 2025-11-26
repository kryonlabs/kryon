// Interactive Button Demo - TypeScript/JSX
import { run } from '../../bindings/typescript/src/index.ts';

function handleButtonClick() {
  console.log("Button clicked! Hello from Kryon-TypeScript!");
}

function App() {
  return (
    <center width="100%" height="100%" backgroundColor="#191919">
      <button
        title="Click Me!"
        width={150}
        height={50}
        backgroundColor="#404080"
        onClick={handleButtonClick}
      />
    </center>
  );
}

run(<App />, {
  title: 'Interactive Button Demo',
  width: 600,
  height: 400,
});
