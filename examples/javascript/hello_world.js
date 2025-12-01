/**
 * Hello World Example - JavaScript
 *
 * A simple Kryon application demonstrating basic components and layout.
 *
 * To run:
 *   bun run hello_world.js
 */

import { run, c } from '../../bindings/javascript/index.js';

const app = c.app({
  windowTitle: 'Hello Kryon!',
  windowWidth: 800,
  windowHeight: 600,
  backgroundColor: '#1a1a2e'
}, [
  c.column({
    width: '100%',
    height: '100%',
    gap: 20,
    align: 'center',
    justify: 'center'
  }, [
    c.text({
      content: 'Hello from Kryon!',
      fontSize: 32,
      fontWeight: 'bold',
      color: '#ffffff'
    }),
    c.text({
      content: 'Plain JavaScript + Bun',
      fontSize: 18,
      color: '#888888'
    }),
    c.button({
      title: 'Click Me',
      backgroundColor: '#4a4a6a',
      color: '#ffffff',
      padding: '12px 24px',
      borderRadius: 6,
      fontSize: 16
    })
  ])
]);

run(app, {
  title: 'Kryon Hello World',
  width: 800,
  height: 600
});
