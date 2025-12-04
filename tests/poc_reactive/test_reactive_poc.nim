## Reactive Manifest POC Test
## Simple counter example demonstrating .kir v2.1 format with reactive state

import std/strformat
import ../../bindings/nim/kryon_dsl/impl
import ../../bindings/nim/runtime
import ../../bindings/nim/reactive_system
import ../../bindings/nim/ir_serialization

# Create reactive state
var counter = namedReactiveVar("counter", 0)
var message = namedReactiveVar("message", "Hello POC!")

proc main() =
  # Build simple UI with reactive state
  var root = Container:
    width = 400.px
    height = 200.px
    layoutDirection = 1  # Column
    padding = 20

    Text:
      content = "Reactive Manifest POC Test"
      fontSize = 18
      fontWeight = 700

    Text:
      content = fmt"Count: {counter.value}"
      fontSize = 16

    Button:
      title = "Increment"
      onClick = proc() =
        counter.value += 1

    Text:
      content = fmt"{message.value}"
      fontSize = 14

  echo "\n=== Reactive Manifest POC Test ==="
  echo "This test demonstrates .kir v2.1 serialization with reactive state\n"

  # Export reactive manifest
  let manifest = exportReactiveManifest()
  if manifest != nil:
    echo "\n=== Reactive Manifest Contents ==="
    ir_reactive_manifest_print(manifest)

    # Serialize to .kir v2.1 format with manifest
    echo "\n=== Serializing to v2.1 format ==="
    if ir_write_json_v2_with_manifest_file(root, manifest, "/tmp/test_reactive_poc.kir"):
      echo "✓ Successfully wrote /tmp/test_reactive_poc.kir"
      echo "\nYou can inspect the file with:"
      echo "  cat /tmp/test_reactive_poc.kir | jq ."
      echo "  jq .reactive_manifest /tmp/test_reactive_poc.kir"
    else:
      echo "✗ Failed to write .kir file"

    ir_reactive_manifest_destroy(manifest)
  else:
    echo "✗ Failed to export reactive manifest"

  # Cleanup
  ir_destroy_component(root)
  echo "\n=== Test Complete ==="

main()
