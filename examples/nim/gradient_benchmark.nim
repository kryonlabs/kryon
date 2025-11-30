import ../../bindings/nim/kryon_dsl

# Gradient Benchmark - 20 conic gradients
# Before optimization: ~20 FPS
# After optimization: 60 FPS

var app = createApp:
  Container:
    width = 100.percent
    height = 100.percent
    layoutDirection = 1  # Column
    gap = 10.px
    padding = (20.px, 20.px, 20.px, 20.px)
    backgroundColor = rgb(30, 30, 30)

    # Title
    Text:
      content = "Gradient Benchmark - 20 Conic Gradients"
      fontSize = 24.px
      color = rgb(255, 255, 255)
      marginBottom = 20.px

    # Grid container for gradients
    Container:
      width = 100.percent
      height = auto
      layoutDirection = 0  # Row
      gap = 15.px
      flexWrap = 1  # Wrap

      # Generate 20 conic gradient boxes
      for i in 0..<20:
        Container:
          width = 180.px
          height = 180.px
          borderRadius = 16.px
          background = conicGradient(
            0.0,  # Start angle
            [
              stop(0.0, rgb(255, 0, 0)),
              stop(0.166, rgb(255, 255, 0)),
              stop(0.333, rgb(0, 255, 0)),
              stop(0.5, rgb(0, 255, 255)),
              stop(0.666, rgb(0, 0, 255)),
              stop(0.833, rgb(255, 0, 255)),
              stop(1.0, rgb(255, 0, 0))
            ]
          )
          boxShadow = shadow(0.px, 4.px, 12.px, rgba(0, 0, 0, 0.5))

          # Label
          Container:
            width = 100.percent
            height = 100.percent
            justifyContent = 4  # Center
            alignItems = 4      # Center

            Text:
              content = "#" & $(i + 1)
              fontSize = 20.px
              color = rgb(255, 255, 255)
              textShadow = textShadow(1.px, 1.px, 3.px, rgba(0, 0, 0, 0.8))

runApp(app, width = 1000, height = 800, title = "Gradient Benchmark")
