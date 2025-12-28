package com.kryon.dsl

/**
 * Container component builder - flexible layout container
 */
@KryonDslMarker
class ContainerBuilder(handle: Long) : ComponentBuilder(handle) {
    init {
        nativeComponentId = nativeCreateContainer(handle)
    }

    // Child component builders
    fun Container(block: ContainerBuilder.() -> Unit) {
        ContainerBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Row(block: RowBuilder.() -> Unit) {
        RowBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Column(block: ColumnBuilder.() -> Unit) {
        ColumnBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Center(block: CenterBuilder.() -> Unit) {
        CenterBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Text(block: TextBuilder.() -> Unit) {
        TextBuilder(handle).apply(block)
    }

    fun Button(block: ButtonBuilder.() -> Unit) {
        ButtonBuilder(handle).apply(block)
    }

    fun Input(block: InputBuilder.() -> Unit) {
        InputBuilder(handle).apply(block)
    }

    private external fun nativeCreateContainer(handle: Long): Int
}

/**
 * Row component builder - horizontal flex layout
 */
@KryonDslMarker
class RowBuilder(handle: Long) : ComponentBuilder(handle) {
    init {
        nativeComponentId = nativeCreateRow(handle)
    }

    fun Container(block: ContainerBuilder.() -> Unit) {
        ContainerBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Row(block: RowBuilder.() -> Unit) {
        RowBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Column(block: ColumnBuilder.() -> Unit) {
        ColumnBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Center(block: CenterBuilder.() -> Unit) {
        CenterBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Text(block: TextBuilder.() -> Unit) {
        TextBuilder(handle).apply(block)
    }

    fun Button(block: ButtonBuilder.() -> Unit) {
        ButtonBuilder(handle).apply(block)
    }

    fun Input(block: InputBuilder.() -> Unit) {
        InputBuilder(handle).apply(block)
    }

    private external fun nativeCreateRow(handle: Long): Int
}

/**
 * Column component builder - vertical flex layout
 */
@KryonDslMarker
class ColumnBuilder(handle: Long) : ComponentBuilder(handle) {
    init {
        nativeComponentId = nativeCreateColumn(handle)
    }

    fun Container(block: ContainerBuilder.() -> Unit) {
        ContainerBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Row(block: RowBuilder.() -> Unit) {
        RowBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Column(block: ColumnBuilder.() -> Unit) {
        ColumnBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Center(block: CenterBuilder.() -> Unit) {
        CenterBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Text(block: TextBuilder.() -> Unit) {
        TextBuilder(handle).apply(block)
    }

    fun Button(block: ButtonBuilder.() -> Unit) {
        ButtonBuilder(handle).apply(block)
    }

    fun Input(block: InputBuilder.() -> Unit) {
        InputBuilder(handle).apply(block)
    }

    private external fun nativeCreateColumn(handle: Long): Int
}

/**
 * Center component builder - centers its child
 */
@KryonDslMarker
class CenterBuilder(handle: Long) : ComponentBuilder(handle) {
    init {
        nativeComponentId = nativeCreateCenter(handle)
    }

    fun Container(block: ContainerBuilder.() -> Unit) {
        ContainerBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Row(block: RowBuilder.() -> Unit) {
        RowBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Column(block: ColumnBuilder.() -> Unit) {
        ColumnBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Center(block: CenterBuilder.() -> Unit) {
        CenterBuilder(handle).apply(block)
        nativePopParent(handle)
    }

    fun Text(block: TextBuilder.() -> Unit) {
        TextBuilder(handle).apply(block)
    }

    fun Button(block: ButtonBuilder.() -> Unit) {
        ButtonBuilder(handle).apply(block)
    }

    fun Input(block: InputBuilder.() -> Unit) {
        InputBuilder(handle).apply(block)
    }

    private external fun nativeCreateCenter(handle: Long): Int
}

/**
 * Text component builder
 */
@KryonDslMarker
class TextBuilder(handle: Long) : ComponentBuilder(handle) {
    init {
        nativeComponentId = nativeCreateText(handle)
    }

    fun text(value: String) {
        nativeSetText(handle, nativeComponentId, value)
    }

    private external fun nativeCreateText(handle: Long): Int
    private external fun nativeSetText(handle: Long, componentId: Int, value: String)
}

/**
 * Button component builder
 */
@KryonDslMarker
class ButtonBuilder(handle: Long) : ComponentBuilder(handle) {
    init {
        nativeComponentId = nativeCreateButton(handle)
    }

    fun text(value: String) {
        nativeSetButtonText(handle, nativeComponentId, value)
    }

    fun onClick(handler: () -> Unit) {
        // Get the activity instance to register callback
        val callbackId = nativeRegisterClickCallback(handle, nativeComponentId, handler)
    }

    private external fun nativeCreateButton(handle: Long): Int
    private external fun nativeSetButtonText(handle: Long, componentId: Int, value: String)
    private external fun nativeRegisterClickCallback(handle: Long, componentId: Int, callback: () -> Unit): Int
}

/**
 * Input component builder
 */
@KryonDslMarker
class InputBuilder(handle: Long) : ComponentBuilder(handle) {
    init {
        nativeComponentId = nativeCreateInput(handle)
    }

    fun placeholder(value: String) {
        nativeSetPlaceholder(handle, nativeComponentId, value)
    }

    fun value(value: String) {
        nativeSetInputValue(handle, nativeComponentId, value)
    }

    fun onChanged(handler: (String) -> Unit) {
        // TODO: Store lambda and call native handler registration
    }

    private external fun nativeCreateInput(handle: Long): Int
    private external fun nativeSetPlaceholder(handle: Long, componentId: Int, value: String)
    private external fun nativeSetInputValue(handle: Long, componentId: Int, value: String)
}
