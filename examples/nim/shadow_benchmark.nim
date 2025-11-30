import ../../bindings/nim/kryon_dsl

# Shadow Benchmark - 100 shadowed text elements
# Before optimization: ~10 FPS
# After optimization: 60 FPS

var app = createApp:
  Container:
    width = 100.percent
    height = 100.percent
    layoutDirection = 1  # Column
    gap = 5.px
    padding = (10.px, 10.px, 10.px, 10.px)
    backgroundColor = rgb(240, 240, 240)

    # Title
    Text:
      content = "Shadow Benchmark - 100 Text Elements with Shadows"
      fontSize = 24.px
      color = rgb(50, 50, 50)
      marginBottom = 20.px

    # Grid container for shadow elements
    Container:
      width = 100.percent
      height = auto
      layoutDirection = 0  # Row wrapping handled by renderer
      gap = 10.px

      # Generate 100 shadowed text elements
      for i in 0..<100:
        Container:
          width = 180.px
          height = 60.px
          backgroundColor = rgb(255, 255, 255)
          borderRadius = 8.px
          padding = (10.px, 10.px, 10.px, 10.px)
          boxShadow = shadow(2.px, 2.px, 8.px, rgba(0, 0, 0, 0.3))

          Text:
            content = "Shadow Text #" & $(i + 1)
            fontSize = 14.px
            color = rgb(60, 60, 60)
            textShadow = textShadow(1.px, 1.px, 4.px, rgba(0, 0, 0, 0.4))

runApp(app, width = 1200, height = 800, title = "Shadow Benchmark")
