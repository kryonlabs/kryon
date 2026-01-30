package com.kryon

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import android.widget.TextView
import android.widget.LinearLayout
import android.widget.Button
import android.view.Gravity
import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.util.DisplayMetrics

abstract class KryonActivity : AppCompatActivity() {

    private lateinit var rootLayout: LinearLayout
    var density: Float = 1.0f
        private set

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Get display density
        val metrics = DisplayMetrics()
        windowManager.defaultDisplay.getMetrics(metrics)
        density = metrics.density

        rootLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setBackgroundColor(Color.parseColor("#1a1a2e"))
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
        }

        buildUI()

        setContentView(rootLayout)
    }

    abstract fun buildUI()

    // Convert dp to pixels
    fun dpToPx(dp: Float): Int {
        return (dp * density).toInt()
    }
    
    fun container(block: ContainerBuilder.() -> Unit = {}) {
        val view = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )
        }
        
        val builder = ContainerBuilder(view, this)
        builder.block()
        
        rootLayout.addView(view)
    }
}

class ContainerBuilder(private val view: LinearLayout, private val activity: KryonActivity) {
    private var bgColor: String? = null
    private var cornerRadius: Float = 0f
    private var position: String? = null
    private var leftPos: Float = 0f
    private var topPos: Float = 0f
    private var borderWidth: Float = 0f
    private var borderColor: String? = null
    
    fun background(color: String) {
        bgColor = color
        applyBackground()
    }
    
    fun width(w: Float) {
        updateLayoutParams {
            width = (w * activity.density).toInt()
        }
    }

    fun height(h: Float) {
        updateLayoutParams {
            height = (h * activity.density).toInt()
        }
    }

    fun padding(top: Float, right: Float, bottom: Float, left: Float) {
        view.setPadding(
            (left * activity.density).toInt(),
            (top * activity.density).toInt(),
            (right * activity.density).toInt(),
            (bottom * activity.density).toInt()
        )
    }

    fun margin(top: Float, right: Float, bottom: Float, left: Float) {
        updateLayoutParams {
            setMargins(
                (left * activity.density).toInt(),
                (top * activity.density).toInt(),
                (right * activity.density).toInt(),
                (bottom * activity.density).toInt()
            )
        }
    }

    fun position(pos: String) {
        position = pos
    }

    fun left(left: Float) {
        leftPos = left
        if (position == "absolute") {
            val pos = (leftPos * activity.density).toInt()
            updateLayoutParams {
                leftMargin = pos
            }
        }
    }

    fun top(top: Float) {
        topPos = top
        if (position == "absolute") {
            val pos = (topPos * activity.density).toInt()
            updateLayoutParams {
                topMargin = pos
            }
        }
    }
    
    fun gap(g: Float) {
        updateLayoutParams {
            bottomMargin = g.toInt()
        }
    }

    fun alignItems(align: String) {
        view.gravity = when(align) {
            "center" -> Gravity.CENTER_HORIZONTAL
            else -> Gravity.START
        }
    }

    fun justifyContent(justify: String) {
        when(justify) {
            "center" -> view.gravity = view.gravity or Gravity.CENTER_VERTICAL
            "flex-start" -> view.gravity = view.gravity or Gravity.TOP
            "flex-end" -> view.gravity = view.gravity or Gravity.BOTTOM
        }
    }

    fun cornerRadius(radius: Float) {
        cornerRadius = radius * activity.density
        applyBackground()
    }

    fun border(width: Float, color: String) {
        borderWidth = width * activity.density
        borderColor = color
        applyBackground()
    }
    
    fun text(content: String, block: TextBuilder.() -> Unit = {}) {
        val textView = TextView(view.context).apply {
            text = content
            textSize = 24f * activity.density  // Scale text size
            setTextColor(Color.parseColor("#ffff00"))
            setPadding(
                (16 * activity.density).toInt(),
                (16 * activity.density).toInt(),
                (16 * activity.density).toInt(),
                (16 * activity.density).toInt()
            )
        }
        view.addView(textView)

        val builder = TextBuilder(textView, activity)
        builder.block()
    }

    fun button(label: String, block: ButtonBuilder.() -> Unit = {}) {
        val buttonView = Button(view.context).apply {
            text = label
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#4CAF50"))
            setPadding(
                (32 * activity.density).toInt(),
                (16 * activity.density).toInt(),
                (32 * activity.density).toInt(),
                (16 * activity.density).toInt()
            )
        }
        view.addView(buttonView)

        val builder = ButtonBuilder(buttonView, activity)
        builder.block()
    }
    
    fun column(block: ColumnBuilder.() -> Unit = {}) {
        val columnView = LinearLayout(view.context).apply {
            orientation = LinearLayout.VERTICAL
        }
        view.addView(columnView)
        
        val builder = ColumnBuilder(columnView, activity)
        builder.block()
    }
    
    private inline fun updateLayoutParams(crossinline update: LinearLayout.LayoutParams.() -> Unit) {
        val params = view.layoutParams as? LinearLayout.LayoutParams ?: LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        )
        params.update()
        view.layoutParams = params
    }
    
    private fun applyBackground() {
        val drawable = GradientDrawable()
        bgColor?.let { drawable.setColor(Color.parseColor(it)) }
        
        if (borderWidth > 0 && borderColor != null) {
            drawable.setStroke(borderWidth.toInt(), Color.parseColor(borderColor!!))
        }
        
        if (cornerRadius > 0) {
            drawable.cornerRadius = cornerRadius
        }
        
        view.background = drawable
    }
}

class ColumnBuilder(private val view: LinearLayout, private val activity: KryonActivity) {
    init {
        view.orientation = LinearLayout.VERTICAL
    }
    
    fun background(color: String) {
        view.setBackgroundColor(Color.parseColor(color))
    }
    
    fun width(w: Float) {
        updateLayoutParams { width = (w * activity.density).toInt() }
    }

    fun height(h: Float) {
        updateLayoutParams { height = (h * activity.density).toInt() }
    }

    fun padding(top: Float, right: Float, bottom: Float, left: Float) {
        view.setPadding(
            (left * activity.density).toInt(),
            (top * activity.density).toInt(),
            (right * activity.density).toInt(),
            (bottom * activity.density).toInt()
        )
    }

    fun margin(top: Float, right: Float, bottom: Float, left: Float) {
        updateLayoutParams {
            setMargins(
                (left * activity.density).toInt(),
                (top * activity.density).toInt(),
                (right * activity.density).toInt(),
                (bottom * activity.density).toInt()
            )
        }
    }

    fun gap(g: Float) {
        updateLayoutParams {
            bottomMargin = (g * activity.density).toInt()
        }
    }
    
    fun alignItems(align: String) {
        view.gravity = when(align) {
            "center" -> Gravity.CENTER_HORIZONTAL
            else -> Gravity.START
        }
    }
    
    fun justifyContent(justify: String) {
        when(justify) {
            "center" -> view.gravity = view.gravity or Gravity.CENTER_VERTICAL
            "flex-start" -> view.gravity = view.gravity or Gravity.TOP
            "flex-end" -> view.gravity = view.gravity or Gravity.BOTTOM
        }
    }
    
    fun text(content: String, block: TextBuilder.() -> Unit = {}) {
        val textView = TextView(view.context).apply {
            text = content
            textSize = 16f * activity.density
            setTextColor(Color.WHITE)
            setPadding(
                (8 * activity.density).toInt(),
                (8 * activity.density).toInt(),
                (8 * activity.density).toInt(),
                (8 * activity.density).toInt()
            )
        }
        view.addView(textView)

        val builder = TextBuilder(textView, activity)
        builder.block()
    }

    fun container(block: ContainerBuilder.() -> Unit = {}) {
        val containerView = LinearLayout(view.context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(
                (8 * activity.density).toInt(),
                (8 * activity.density).toInt(),
                (8 * activity.density).toInt(),
                (8 * activity.density).toInt()
            )
        }
        view.addView(containerView)

        val builder = ContainerBuilder(containerView, activity)
        builder.block()
    }
    
    private inline fun updateLayoutParams(crossinline update: LinearLayout.LayoutParams.() -> Unit) {
        val params = view.layoutParams as? LinearLayout.LayoutParams ?: LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        )
        params.update()
        view.layoutParams = params
    }
}

class TextBuilder(private val view: TextView, private val activity: KryonActivity) {
    fun fontSize(size: Float) {
        view.textSize = size * activity.density
    }

    fun color(c: String) {
        view.setTextColor(Color.parseColor(c))
    }
}

class ButtonBuilder(private val view: Button, private val activity: KryonActivity) {
    fun width(w: Float) {
        updateLayoutParams { width = (w * activity.density).toInt() }
    }

    fun height(h: Float) {
        updateLayoutParams { height = (h * activity.density).toInt() }
    }

    fun background(color: String) {
        view.setBackgroundColor(Color.parseColor(color))
    }

    fun fontSize(size: Float) {
        view.textSize = size * activity.density
    }

    fun color(c: String) {
        view.setTextColor(Color.parseColor(c))
    }

    fun cornerRadius(radius: Float) {
        val drawable = GradientDrawable()
        drawable.setColor(view.backgroundTintList?.defaultColor ?: Color.parseColor("#4CAF50"))
        drawable.cornerRadius = radius * activity.density
        view.background = drawable
    }
    
    private inline fun updateLayoutParams(crossinline update: LinearLayout.LayoutParams.() -> Unit) {
        val params = view.layoutParams as? LinearLayout.LayoutParams ?: LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        )
        params.update()
        view.layoutParams = params
    }
}
