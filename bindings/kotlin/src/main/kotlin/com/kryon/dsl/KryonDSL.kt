package com.kryon.dsl

/**
 * DSL marker to prevent DSL builder scope leakage
 */
@DslMarker
annotation class KryonDslMarker

/**
 * Base class for all Kryon component builders.
 * Provides common styling and layout methods that all components support.
 */
@KryonDslMarker
abstract class ComponentBuilder(protected val handle: Long) {
    protected var nativeComponentId: Int = 0

    // Layout properties
    fun width(value: Float) {
        nativeSetWidth(handle, nativeComponentId, value)
    }

    fun height(value: Float) {
        nativeSetHeight(handle, nativeComponentId, value)
    }

    fun padding(value: Float) {
        nativeSetPadding(handle, nativeComponentId, value, value, value, value)
    }

    fun padding(top: Float, right: Float, bottom: Float, left: Float) {
        nativeSetPadding(handle, nativeComponentId, top, right, bottom, left)
    }

    fun margin(value: Float) {
        nativeSetMargin(handle, nativeComponentId, value, value, value, value)
    }

    fun margin(top: Float, right: Float, bottom: Float, left: Float) {
        nativeSetMargin(handle, nativeComponentId, top, right, bottom, left)
    }

    fun gap(value: Float) {
        nativeSetGap(handle, nativeComponentId, value)
    }

    // Color properties
    fun background(color: String) {
        nativeSetBackground(handle, nativeComponentId, color)
    }

    fun color(color: String) {
        nativeSetColor(handle, nativeComponentId, color)
    }

    // Flexbox layout
    fun justifyContent(value: String) {
        nativeSetJustifyContent(handle, nativeComponentId, value)
    }

    fun alignItems(value: String) {
        nativeSetAlignItems(handle, nativeComponentId, value)
    }

    fun flexDirection(value: String) {
        nativeSetFlexDirection(handle, nativeComponentId, value)
    }

    // Typography
    fun fontSize(size: Float) {
        nativeSetFontSize(handle, nativeComponentId, size)
    }

    fun fontWeight(weight: Int) {
        nativeSetFontWeight(handle, nativeComponentId, weight)
    }

    fun fontBold() {
        nativeSetFontWeight(handle, nativeComponentId, 700)
    }

    // Positioning
    fun position(value: String) {
        nativeSetPosition(handle, nativeComponentId, value)
    }

    fun left(value: Float) {
        nativeSetLeft(handle, nativeComponentId, value)
    }

    fun top(value: Float) {
        nativeSetTop(handle, nativeComponentId, value)
    }

    // Border
    fun border(width: Float, color: String, radius: Float = 0f) {
        nativeSetBorder(handle, nativeComponentId, width, color, radius)
    }

    // Native method declarations (implemented in kryon_dsl_jni.c)
    private external fun nativeSetWidth(handle: Long, componentId: Int, value: Float)
    private external fun nativeSetHeight(handle: Long, componentId: Int, value: Float)
    private external fun nativeSetPadding(handle: Long, componentId: Int, top: Float, right: Float, bottom: Float, left: Float)
    private external fun nativeSetMargin(handle: Long, componentId: Int, top: Float, right: Float, bottom: Float, left: Float)
    private external fun nativeSetGap(handle: Long, componentId: Int, value: Float)
    private external fun nativeSetBackground(handle: Long, componentId: Int, color: String)
    private external fun nativeSetColor(handle: Long, componentId: Int, color: String)
    private external fun nativeSetJustifyContent(handle: Long, componentId: Int, value: String)
    private external fun nativeSetAlignItems(handle: Long, componentId: Int, value: String)
    private external fun nativeSetFlexDirection(handle: Long, componentId: Int, value: String)
    private external fun nativeSetFontSize(handle: Long, componentId: Int, size: Float)
    private external fun nativeSetFontWeight(handle: Long, componentId: Int, weight: Int)
    private external fun nativeSetPosition(handle: Long, componentId: Int, value: String)
    private external fun nativeSetLeft(handle: Long, componentId: Int, value: Float)
    private external fun nativeSetTop(handle: Long, componentId: Int, value: Float)
    private external fun nativeSetBorder(handle: Long, componentId: Int, width: Float, color: String, radius: Float)

    // Stack management for nested containers
    protected external fun nativePopParent(handle: Long)

    companion object {
        init {
            System.loadLibrary("kryon_android")
        }
    }
}
