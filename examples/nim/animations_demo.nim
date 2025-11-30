## Animations Demo - CSS-Like Animation Configs

import kryon_dsl

# Animation configurations with CSS-like string specs
const animConfigs = [
  (name: "Pulse", desc: "Scale animation", color: "#6366f1", icon: "*",
   anim: "pulse(2.0, -1)"),
  (name: "Fade In/Out", desc: "Opacity animation", color: "#a78bfa", icon: "~",
   anim: "fadeInOut(3.0, -1)"),
  (name: "Slide In", desc: "Translate animation", color: "#f472b6", icon: ">",
   anim: "slideInLeft(2.0)")
]

let app = kryonApp:
  Header:
    windowWidth = 1000
    windowHeight = 600
    windowTitle = "Animations Demo"

  Body:
    backgroundColor = "#0a0e27"
    padding = 40

    # Title
    Text:
      text = "Preset Animations"
      fontSize = 36
      fontWeight = 700
      color = "#ffffff"

    Text:
      text = "CSS-like animation configs"
      fontSize = 16
      color = "#a5b4fc"

    Spacer()

    # Animation boxes - direct string usage!
    Row:
      gap = 30
      justifyContent = "center"

      # Compile-time loop over animation configs
      static:
        for config in animConfigs:
          Container:
            width = 200
            backgroundColor = "#1e1b4b"
            padding = 24
            borderRadius = 12
            boxShadow = (offsetY: 8.0, spread: 4.0, color: "#00000080")

            Container:
              width = 80
              height = 80
              backgroundColor = config.color
              borderRadius = 40
              animation = config.anim
              
              Center:
                Text:
                  text = config.icon
                  fontSize = 40
                  color = "#ffffff"

            Text:
              text = config.name
              fontSize = 18
              fontWeight = 600
              color = "#e0e7ff"

            Text:
              text = config.desc
              fontSize = 13
              color = "#a5b4fc"

    # Info box
    Container:
      width = 100.pct
      maxWidth = 600
      backgroundColor = "#1e1b4b"
      padding = 20
      borderRadius = 12
      boxShadow = (offsetY: 6.0, spread: 3.0, color: "#00000060")

      Text:
        text = "Available Preset Animations"
        fontSize = 16
        fontWeight = 600
        color = "#e0e7ff"

      Text:
        text = "pulse(duration, iterations) - Scale in/out effect"
        fontSize = 13
        color = "#c7d2fe"

      Text:
        text = "fadeInOut(duration, iterations) - Opacity 0 to 1 to 0"
        fontSize = 13
        color = "#c7d2fe"

      Text:
        text = "slideInLeft(duration) - Slide in from left edge"
        fontSize = 13
        color = "#c7d2fe"

      Text:
        text = "Use -1 for infinite loop, 1 for once, N for N times"
        fontSize = 12
        color = "#a5b4fc"
