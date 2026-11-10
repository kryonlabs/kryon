#!/usr/bin/env python3
"""GTK3 offscreen CSS pixels and native ownership, source and saved IR."""
import argparse
import ctypes as C
import os
from pathlib import Path
import shlex
import subprocess
import sys
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/scratch/gtk-style-linux'
CSS = ROOT / 'tests/gtk_style_linux_test.css'
SCRUB = ('DISPLAY', 'WAYLAND_DISPLAY', 'WAYLAND_SOCKET', 'XAUTHORITY',
         'DBUS_SESSION_BUS_ADDRESS', 'SESSION_MANAGER', 'ICEAUTHORITY', 'GDK_DISPLAY',
         'DESKTOP_SESSION', 'XDG_CURRENT_DESKTOP', 'XDG_SESSION_ID', 'XDG_SESSION_TYPE')


def run(args, **kwargs):
    print(shlex.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True, **kwargs)


def reference(scale, screen_css=None, local_css=None, state=0, backdrop=False):
    """Independent GTK calls, with Cairo's premultiplied ARGB converted by Python."""
    gtk = C.CDLL('libgtk-3.so.0')
    gdk = C.CDLL('libgdk-3.so.0')
    cairo = C.CDLL('libcairo.so.2')
    obj = C.CDLL('libgobject-2.0.so.0')

    def fn(lib, name, result, *args):
        f = getattr(lib, name)
        f.restype, f.argtypes = result, args
        return f

    ptr, int_, uint, text, double = C.c_void_p, C.c_int, C.c_uint, C.c_char_p, C.c_double
    assert fn(gtk, 'gtk_init_check', int_, ptr, ptr)(None, None)
    screen = fn(gdk, 'gdk_screen_get_default', ptr)()
    provider = fn(gtk, 'gtk_css_provider_new', ptr)()
    assert fn(gtk, 'gtk_css_provider_load_from_path', int_, ptr, text, ptr)(provider, os.fsencode(CSS), None)
    fn(gtk, 'gtk_style_context_add_provider_for_screen', None, ptr, ptr, uint)(screen, provider, 800)
    # A real GTK widget hierarchy supplies an independent reference path.
    window = fn(gtk, 'gtk_window_new', ptr, int_)(0)
    button = fn(gtk, 'gtk_button_new', ptr)()
    fn(gtk, 'gtk_container_add', None, ptr, ptr)(window, button)
    context_for = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)
    root_context, context = context_for(window), context_for(button)
    add_class = fn(gtk, 'gtk_style_context_add_class', None, ptr, text)
    add_class(root_context, b'fixture-root')
    add_class(context, b'flat')
    fn(gtk, 'gtk_widget_set_name', None, ptr, text)(button, b'target')
    fn(gtk, 'gtk_style_context_set_scale', None, ptr, int_)(context, scale)
    extra=[]
    for css, local in ((screen_css,False),(local_css,True)):
        if css is None: continue
        new=fn(gtk,'gtk_css_provider_new',ptr)()
        assert fn(gtk,'gtk_css_provider_load_from_data',int_,ptr,text,C.c_ssize_t,ptr)(new,css.encode(),-1,None)
        if local: fn(gtk,'gtk_style_context_add_provider',None,ptr,ptr,C.c_uint)(context,new,900)
        else: fn(gtk,'gtk_style_context_add_provider_for_screen',None,ptr,ptr,C.c_uint)(screen,new,900)
        extra.append((new,local))
    fn(gtk,'gtk_style_context_set_state',None,ptr,C.c_uint)(context,state)
    w, h = 24 * scale, 16 * scale
    surface = fn(cairo, 'cairo_image_surface_create', ptr, int_, int_, int_)(0, w, h)
    draw = fn(cairo, 'cairo_create', ptr, ptr)(surface)
    if backdrop:
        fn(cairo,'cairo_surface_flush',None,ptr)(surface)
        stride=fn(cairo,'cairo_image_surface_get_stride',int_,ptr)(surface)
        data=fn(cairo,'cairo_image_surface_get_data',ptr,ptr)(surface)
        for y in range(h):
            for x in range(w):
                r,g,b,a=12+x%5,14+y%7,20+(x+y)%9,0 if x%5==0 else 128 if x%3==0 else 255
                pixel=(a<<24)|(((r*a+127)//255)<<16)|(((g*a+127)//255)<<8)|((b*a+127)//255)
                C.c_uint32.from_address(data+y*stride+x*4).value=pixel
        fn(cairo,'cairo_surface_mark_dirty',None,ptr)(surface)
        # Independent upload via Cairo SOURCE, rather than the implementation's
        # writable map. Only newly allocated compatible pixmaps are accessed.
        root=fn(gdk,'gdk_get_default_root_window',ptr)()
        template=fn(gdk,'gdk_window_create_similar_surface',ptr,ptr,int_,int_,int_)(root,0x3000,1,1)
        fn(cairo,'cairo_surface_set_device_scale',None,ptr,double,double)(template,1,1)
        native=fn(cairo,'cairo_surface_create_similar',ptr,ptr,int_,int_,int_)(template,0x3000,w,h)
        fn(cairo,'cairo_surface_destroy',None,ptr)(template)
        fn(cairo,'cairo_surface_set_device_scale',None,ptr,double,double)(native,1,1)
        fn(cairo,'cairo_destroy',None,ptr)(draw)
        draw=fn(cairo,'cairo_create',ptr,ptr)(native)
        fn(cairo,'cairo_set_operator',None,ptr,int_)(draw,1)
        fn(cairo,'cairo_set_source_surface',None,ptr,ptr,double,double)(draw,surface,0,0)
        fn(cairo,'cairo_paint',None,ptr)(draw)
        fn(cairo,'cairo_set_operator',None,ptr,int_)(draw,2)
        fn(cairo,'cairo_surface_flush',None,ptr)(native)
        fn(cairo,'cairo_surface_destroy',None,ptr)(surface)
        surface=native
    fn(cairo, 'cairo_scale', None, ptr, double, double)(draw, scale, scale)
    for name in ('gtk_render_background', 'gtk_render_frame'):
        fn(gtk, name, None, ptr, ptr, double, double, double, double)(context, draw, 0, 0, 24, 16)
    fn(cairo, 'cairo_surface_flush', None, ptr)(surface)
    mapped=fn(cairo,'cairo_surface_map_to_image',ptr,ptr,ptr)(surface,None) if backdrop else surface
    assert fn(cairo,'cairo_image_surface_get_width',int_,ptr)(mapped)==w
    assert fn(cairo,'cairo_image_surface_get_height',int_,ptr)(mapped)==h
    stride = fn(cairo, 'cairo_image_surface_get_stride', int_, ptr)(mapped)
    data = fn(cairo, 'cairo_image_surface_get_data', ptr, ptr)(mapped)
    pixels = C.string_at(data, stride * h)
    result = bytearray()
    for y in range(h):
        for x in range(w):
            value = int.from_bytes(pixels[y*stride+x*4:y*stride+x*4+4], sys.byteorder)
            alpha = value >> 24
            for shift in (16, 8, 0):
                component = (value >> shift) & 255
                result.append(min(255, (component*255+alpha//2)//alpha) if alpha else 0)
            result.append(alpha)
    if backdrop: fn(cairo,'cairo_surface_unmap_image',None,ptr,ptr)(surface,mapped)
    fn(cairo, 'cairo_destroy', None, ptr)(draw)
    fn(cairo, 'cairo_surface_destroy', None, ptr)(surface)
    for new,local in extra:
        if not local: fn(gtk,'gtk_style_context_remove_provider_for_screen',None,ptr,ptr)(screen,new)
        fn(obj,'g_object_unref',None,ptr)(new)
    fn(gtk, 'gtk_widget_destroy', None, ptr)(window)
    fn(gtk, 'gtk_style_context_remove_provider_for_screen', None, ptr, ptr)(screen, provider)
    fn(obj, 'g_object_unref', None, ptr)(provider)
    return bytes(result)


def reference_preferred():
    """Minimum/natural requisitions of actual GTK tree headers and entries."""
    gtk, gdk, obj = (C.CDLL(name) for name in ('libgtk-3.so.0', 'libgdk-3.so.0', 'libgobject-2.0.so.0'))
    ptr, int_, text = C.c_void_p, C.c_int, C.c_char_p
    def fn(lib, name, result, *args):
        f = getattr(lib, name); f.restype, f.argtypes = result, args; return f
    class Size(C.Structure):
        _fields_ = [('width', int_), ('height', int_)]
    assert fn(gtk, 'gtk_init_check', int_, ptr, ptr)(None, None)
    provider = fn(gtk, 'gtk_css_provider_new', ptr)()
    assert fn(gtk, 'gtk_css_provider_load_from_path', int_, ptr, text, ptr)(provider, os.fsencode(CSS), None)
    screen = fn(gdk, 'gdk_screen_get_default', ptr)()
    fn(gtk, 'gtk_style_context_add_provider_for_screen', None, ptr, ptr, C.c_uint)(screen, provider, 800)
    rows = []
    for header in (True, False):
        root = fn(gtk, 'gtk_window_new', ptr, int_)(0)
        context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
        for cls in (b'fixture-root', b'background'):
            fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
        if header:
            view = fn(gtk, 'gtk_tree_view_new', ptr)()
            column = fn(gtk, 'gtk_tree_view_column_new', ptr)()
            fn(gtk, 'gtk_tree_view_column_set_title', None, ptr, text)(column, b'Name')
            fn(gtk, 'gtk_tree_view_column_set_sort_indicator', None, ptr, int_)(column, 1)
            fn(gtk, 'gtk_tree_view_append_column', int_, ptr, ptr)(view, column)
            fn(gtk, 'gtk_container_add', None, ptr, ptr)(root, view)
            fn(gtk, 'gtk_widget_show_all', None, ptr)(view)
            widget = fn(gtk, 'gtk_tree_view_column_get_button', ptr, ptr)(column)
        else:
            toolbar = fn(gtk, 'gtk_toolbar_new', ptr)()
            item = fn(gtk, 'gtk_tool_item_new', ptr)()
            widget = fn(gtk, 'gtk_entry_new', ptr)()
            fn(gtk, 'gtk_container_add', None, ptr, ptr)(item, widget)
            fn(gtk, 'gtk_toolbar_insert', None, ptr, ptr, int_)(toolbar, item, -1)
            fn(gtk, 'gtk_container_add', None, ptr, ptr)(root, toolbar)
            fn(gtk, 'gtk_widget_show_all', None, ptr)(toolbar)
        minimum, natural = Size(), Size()
        fn(gtk, 'gtk_widget_show_all', None, ptr)(widget)
        fn(gtk, 'gtk_widget_get_preferred_size', None, ptr, C.POINTER(Size), C.POINTER(Size))(widget, C.byref(minimum), C.byref(natural))
        rows.append((minimum.width, minimum.height, natural.width, natural.height))
        fn(gtk, 'gtk_widget_destroy', None, ptr)(root)
    root = fn(gtk, 'gtk_window_new', ptr, int_)(0)
    context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
    for cls in (b'fixture-root', b'background'):
        fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
    view = fn(gtk, 'gtk_tree_view_new', ptr)()
    fn(gtk, 'gtk_container_add', None, ptr, ptr)(root, view)
    renderer = fn(gtk, 'gtk_cell_renderer_text_new', ptr)()
    fn(obj, 'g_object_ref_sink', ptr, ptr)(renderer)
    measured = []
    for value in (b'156 bytes', b'Plain text document (link to 04 Notes.txt)', b'11/14/23'):
        fn(obj, 'g_object_set', None, ptr, text, text, ptr)(renderer, b'text', value, None)
        minimum, natural = Size(), Size()
        fn(gtk, 'gtk_cell_renderer_get_preferred_size', None, ptr, ptr, C.POINTER(Size), C.POINTER(Size))(
            renderer, view, C.byref(minimum), C.byref(natural))
        measured.append((minimum.width, minimum.height, natural.width, natural.height))
    rows.append(tuple(max(values) for values in zip(*measured)))
    fn(obj, 'g_object_set', None, ptr, text, int_, text, int_, ptr)(renderer,
        b'width-chars', 60, b'ellipsize', 3, None)
    measured = []
    for value in (b'156 bytes', b'Plain text document (link to 04 Notes.txt)', b'11/14/23'):
        fn(obj, 'g_object_set', None, ptr, text, text, ptr)(renderer, b'text', value, None)
        minimum, natural = Size(), Size()
        fn(gtk, 'gtk_cell_renderer_get_preferred_size', None, ptr, ptr, C.POINTER(Size), C.POINTER(Size))(
            renderer, view, C.byref(minimum), C.byref(natural))
        measured.append((minimum.width, minimum.height, natural.width, natural.height))
    rows.append(tuple(max(values) for values in zip(*measured)))
    fn(obj, 'g_object_unref', None, ptr)(renderer)
    fn(gtk, 'gtk_widget_destroy', None, ptr)(root)
    allocations = []
    for toolbar, measure_container in ((False, True), (False, False), (True, True), (True, False)):
        root = fn(gtk, 'gtk_window_new', ptr, int_)(0)
        context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
        for cls in (b'fixture-root', b'background'):
            fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
        if toolbar:
            bar = fn(gtk, 'gtk_toolbar_new', ptr)()
            fn(gtk, 'gtk_toolbar_set_style', None, ptr, int_)(bar, 0)
            fn(gtk, 'gtk_toolbar_set_icon_size', None, ptr, int_)(bar, 3)
            image = fn(gtk, 'gtk_image_new_from_icon_name', ptr, text, int_)(b'fixture-icon', 3)
            item = fn(gtk, 'gtk_tool_button_new', ptr, ptr, text)(image, None)
            fn(gtk, 'gtk_toolbar_insert', None, ptr, ptr, int_)(bar, item, -1)
        else:
            bar = fn(gtk, 'gtk_menu_bar_new', ptr)()
            item = fn(gtk, 'gtk_menu_item_new_with_label', ptr, text)(b'Bookmarks')
            fn(gtk, 'gtk_container_add', None, ptr, ptr)(bar, item)
        fn(gtk, 'gtk_container_add', None, ptr, ptr)(root, bar)
        fn(gtk, 'gtk_widget_show_all', None, ptr)(bar)
        minimum, natural = Size(), Size()
        fn(gtk, 'gtk_widget_get_preferred_size', None, ptr, C.POINTER(Size), C.POINTER(Size))(
            bar if measure_container else item, C.byref(minimum), C.byref(natural))
        rows.append((minimum.width, minimum.height, natural.width, natural.height))
        if measure_container:
            allocation = (int_ * 4)(0, 0, natural.width, natural.height)
            fn(gtk, 'gtk_widget_size_allocate', None, ptr, ptr)(bar, allocation)
            fn(gtk, 'gtk_widget_get_allocation', None, ptr, ptr)(item, allocation)
            allocations.append(tuple(allocation))
        fn(gtk, 'gtk_widget_destroy', None, ptr)(root)
    rows.extend(allocations)
    root = fn(gtk, 'gtk_window_new', ptr, int_)(0)
    context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
    for cls in (b'fixture-root', b'background'):
        fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
    paned = fn(gtk, 'gtk_paned_new', ptr, int_)(0)
    for first in (True, False):
        child = fn(gtk, 'gtk_box_new', ptr, int_, int_)(1, 0)
        fn(gtk, 'gtk_paned_pack1' if first else 'gtk_paned_pack2', None, ptr, ptr, int_, int_)(paned, child, int(not first), 0)
    fn(gtk, 'gtk_container_add', None, ptr, ptr)(root, paned)
    fn(gtk, 'gtk_widget_show_all', None, ptr)(paned)
    minimum, natural = Size(), Size()
    fn(gtk, 'gtk_widget_get_preferred_size', None, ptr, C.POINTER(Size), C.POINTER(Size))(paned, C.byref(minimum), C.byref(natural))
    rows.append((minimum.width, minimum.height, natural.width, natural.height))
    fn(gtk, 'gtk_widget_destroy', None, ptr)(root)
    root = fn(gtk, 'gtk_window_new', ptr, int_)(0)
    context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
    for cls in (b'fixture-root', b'background'):
        fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
    grid = fn(gtk, 'gtk_grid_new', ptr)()
    toolbar = fn(gtk, 'gtk_toolbar_new', ptr)()
    item = fn(gtk, 'gtk_tool_item_new', ptr)()
    alignment = fn(gtk, 'gtk_alignment_new', ptr, C.c_float, C.c_float, C.c_float, C.c_float)(0,0,1,1)
    box = fn(gtk, 'gtk_box_new', ptr, int_, int_)(0,0)
    entry = fn(gtk, 'gtk_entry_new', ptr)()
    fn(gtk, 'gtk_container_add', None, ptr, ptr)(root,grid)
    fn(gtk, 'gtk_container_add', None, ptr, ptr)(grid,toolbar)
    fn(gtk, 'gtk_toolbar_insert', None, ptr, ptr, int_)(toolbar,item,-1)
    for parent, child in ((item,alignment),(alignment,box),(box,entry)):
        fn(gtk, 'gtk_container_add', None, ptr, ptr)(parent,child)
    fn(gtk, 'gtk_widget_show_all', None, ptr)(grid)
    minimum, natural = Size(), Size()
    fn(gtk, 'gtk_widget_get_preferred_size', None, ptr, C.POINTER(Size), C.POINTER(Size))(entry,C.byref(minimum),C.byref(natural))
    rows.append((minimum.width,minimum.height,natural.width,natural.height))
    fn(gtk, 'gtk_widget_destroy', None, ptr)(root)
    class List(C.Structure): pass
    List._fields_=[('data',ptr),('next',C.POINTER(List)),('previous',C.POINTER(List))]
    for sorted_, peer_count, peer_index in ((False,1,0),(True,1,0),(True,3,0),(True,3,1),(True,3,2)):
        root = fn(gtk, 'gtk_window_new', ptr, int_)(0)
        context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
        for cls in (b'fixture-root', b'background'):
            fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
        tree = fn(gtk, 'gtk_tree_view_new', ptr)()
        column=None
        for position in range(peer_count):
            peer=fn(gtk, 'gtk_tree_view_column_new', ptr)()
            if position==peer_index:
                column=peer
                fn(gtk, 'gtk_tree_view_column_set_title', None, ptr, text)(column,b'Name')
                fn(gtk, 'gtk_tree_view_column_set_sort_indicator', None, ptr, int_)(column,int(sorted_))
            fn(gtk, 'gtk_tree_view_append_column', int_, ptr, ptr)(tree,peer)
        fn(gtk, 'gtk_container_add', None, ptr, ptr)(root,tree)
        fn(gtk, 'gtk_widget_show', None, ptr)(tree)
        button=fn(gtk, 'gtk_tree_view_column_get_button', ptr, ptr)(column)
        fn(gtk, 'gtk_widget_show', None, ptr)(button)
        allocation=(int_*4)(0,0,213,37)
        fn(gtk, 'gtk_widget_get_preferred_size', None, ptr, C.POINTER(Size), C.POINTER(Size))(button,C.byref(minimum),C.byref(natural))
        fn(gtk, 'gtk_widget_size_allocate', None, ptr, ptr)(button,allocation)
        box=fn(gtk, 'gtk_bin_get_child', ptr, ptr)(button)
        children=fn(gtk, 'gtk_container_get_children', C.POINTER(List), ptr)(box)
        current=children; label_rect=(0,0,0,0); arrow_rect=(0,0,0,0)
        image_type=fn(gtk, 'gtk_image_get_type', C.c_ulong)()
        while current:
            child=current.contents.data
            image=fn(obj, 'g_type_check_instance_is_a', int_, ptr, C.c_ulong)(child,image_type)
            widget=child if image else fn(gtk, 'gtk_bin_get_child', ptr, ptr)(child)
            fn(gtk, 'gtk_widget_get_allocation', None, ptr, ptr)(widget,allocation)
            if image:
                if sorted_: arrow_rect=tuple(allocation)
            else: label_rect=tuple(allocation)
            current=current.contents.next
        rows.extend((label_rect,arrow_rect))
        fn(C.CDLL('libglib-2.0.so.0'),'g_list_free',None,ptr)(children)
        fn(gtk, 'gtk_widget_destroy', None, ptr)(root)
    for icon in (False, True):
        root = fn(gtk, 'gtk_offscreen_window_new', ptr)()
        context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
        for cls in (b'fixture-root', b'background'):
            fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
        grid = fn(gtk, 'gtk_grid_new', ptr)()
        toolbar = fn(gtk, 'gtk_toolbar_new', ptr)()
        item = fn(gtk, 'gtk_tool_item_new', ptr)()
        alignment = fn(gtk, 'gtk_alignment_new', ptr, C.c_float, C.c_float, C.c_float, C.c_float)(0,0,1,1)
        box = fn(gtk, 'gtk_box_new', ptr, int_, int_)(0,0)
        entry = fn(gtk, 'gtk_entry_new', ptr)()
        fn(gtk, 'gtk_container_add', None, ptr, ptr)(root,grid)
        fn(gtk, 'gtk_container_add', None, ptr, ptr)(grid,toolbar)
        fn(gtk, 'gtk_toolbar_insert', None, ptr, ptr, int_)(toolbar,item,-1)
        for parent, child in ((item,alignment),(alignment,box),(box,entry)):
            fn(gtk, 'gtk_container_add', None, ptr, ptr)(parent,child)
        if icon:
            fn(gtk, 'gtk_entry_set_icon_from_icon_name', None, ptr, int_, text)(entry,0,b'folder')
        fn(gtk, 'gtk_widget_show_all', None, ptr)(grid)
        minimum, natural = Size(), Size()
        fn(gtk, 'gtk_widget_get_preferred_size', None, ptr, C.POINTER(Size), C.POINTER(Size))(entry,C.byref(minimum),C.byref(natural))
        allocation=(int_*4)(0,0,347,37)
        fn(gtk, 'gtk_widget_realize', None, ptr)(entry)
        fn(gtk, 'gtk_widget_size_allocate', None, ptr, ptr)(entry,allocation)
        area=(int_*4)()
        fn(gtk, 'gtk_entry_get_text_area', None, ptr, ptr)(entry,area)
        x,y=int_(),int_()
        fn(gtk, 'gtk_entry_get_layout_offsets', None, ptr, C.POINTER(int_), C.POINTER(int_))(entry,C.byref(x),C.byref(y))
        layout=fn(gtk, 'gtk_entry_get_layout', ptr, ptr)(entry)
        width,height=int_(),int_()
        fn(C.CDLL('libpango-1.0.so.0'),'pango_layout_get_pixel_size',None,ptr,C.POINTER(int_),C.POINTER(int_))(layout,C.byref(width),C.byref(height))
        rows.append((x.value,y.value,max(0,area[0]+area[2]-x.value),height.value))
        area=(int_*4)()
        if icon: fn(gtk, 'gtk_entry_get_icon_area',None,ptr,int_,ptr)(entry,0,area)
        rows.append(tuple(area))
        fn(gtk, 'gtk_widget_destroy', None, ptr)(root)
    for compact in (False, True):
        for message in (b' ', b'17 items', b'17 folders | 4 files: 234 bytes | Free space: 434.3 GiB'):
            root = fn(gtk, 'gtk_window_new', ptr, int_)(0)
            context = fn(gtk, 'gtk_widget_get_style_context', ptr, ptr)(root)
            for cls in (b'fixture-root', b'background'):
                fn(gtk, 'gtk_style_context_add_class', None, ptr, text)(context, cls)
            status=fn(gtk,'gtk_statusbar_new',ptr)()
            if compact:
                fn(gtk,'gtk_widget_set_margin_top',None,ptr,int_)(status,0)
                fn(gtk,'gtk_widget_set_margin_bottom',None,ptr,int_)(status,0)
            fn(gtk,'gtk_statusbar_push',C.c_uint,ptr,C.c_uint,text)(status,0,message)
            fn(gtk,'gtk_container_add',None,ptr,ptr)(root,status)
            fn(gtk,'gtk_widget_show_all',None,ptr)(status)
            minimum,natural=Size(),Size()
            fn(gtk,'gtk_widget_get_preferred_size',None,ptr,C.POINTER(Size),C.POINTER(Size))(status,C.byref(minimum),C.byref(natural))
            rows.append((minimum.width,minimum.height,natural.width,natural.height))
            allocation=(int_*4)(0,0,347,41)
            fn(gtk,'gtk_widget_size_allocate',None,ptr,ptr)(status,allocation)
            area=fn(gtk,'gtk_statusbar_get_message_area',ptr,ptr)(status)
            children=fn(gtk,'gtk_container_get_children',C.POINTER(List),ptr)(area)
            current=children; label_rect=(0,0,0,0)
            while current:
                child=current.contents.data
                if fn(obj,'g_type_check_instance_is_a',int_,ptr,C.c_ulong)(child,fn(gtk,'gtk_label_get_type',C.c_ulong)()):
                    fn(gtk,'gtk_widget_get_allocation',None,ptr,ptr)(child,allocation)
                    label_rect=tuple(allocation)
                    break
                current=current.contents.next
            rows.append(label_rect)
            fn(C.CDLL('libglib-2.0.so.0'),'g_list_free',None,ptr)(children)
            fn(gtk,'gtk_widget_destroy',None,ptr)(root)
    fn(gtk, 'gtk_style_context_remove_provider_for_screen', None, ptr, ptr)(screen, provider)
    fn(obj, 'g_object_unref', None, ptr)(provider)
    return rows


def reference_tree_separators():
    """Read style properties from an actual GtkTreeView, independently of paths."""
    gtk = C.CDLL('libgtk-3.so.0')
    ptr = C.c_void_p
    gtk.gtk_tree_view_new.restype = ptr
    tree = gtk.gtk_tree_view_new()
    vertical, horizontal = C.c_int(), C.c_int()
    gtk.gtk_widget_style_get(C.c_void_p(tree), C.c_char_p(b'vertical-separator'), C.byref(vertical),
                             C.c_char_p(b'horizontal-separator'), C.byref(horizontal), ptr())
    gtk.gtk_widget_destroy.argtypes = [ptr]
    gtk.gtk_widget_destroy(tree)
    return vertical.value, horizontal.value


def reference_image(scale=1, transformed=False):
    """Actual GtkImage uses the large-toolbar category and natural icon artwork."""
    gtk, cairo = C.CDLL('libgtk-3.so.0'), C.CDLL('libcairo.so.2')
    ptr, int_, text = C.c_void_p, C.c_int, C.c_char_p
    def fn(lib, name, result, *args):
        f = getattr(lib, name); f.restype, f.argtypes = result, args; return f
    assert fn(gtk, 'gtk_init_check', int_, ptr, ptr)(None, None)
    theme = fn(gtk, 'gtk_icon_theme_get_default', ptr)()
    paths = (text * 1)(os.fsencode(os.environ['GTK_STYLE_ICON_PATH']))
    fn(gtk, 'gtk_icon_theme_set_search_path', None, ptr, C.POINTER(text), int_)(theme, paths, 1)
    settings = fn(gtk, 'gtk_settings_get_default', ptr)()
    obj = C.CDLL('libgobject-2.0.so.0')
    fn(obj, 'g_object_set', None, ptr, text, text, ptr)(settings, b'gtk-icon-theme-name', b'FixtureMain', None)
    window = fn(gtk, 'gtk_offscreen_window_new', ptr)()
    provider = None
    if transformed:
        provider = fn(gtk, 'gtk_css_provider_new', ptr)()
        fn(gtk, 'gtk_css_provider_load_from_data', int_, ptr, text, C.c_ssize_t, ptr)(provider,
            b'image { -gtk-icon-transform:scale(0.63); -gtk-icon-shadow:1px 2px 1px rgba(20,40,80,0.5); }', -1, None)
        screen = fn(C.CDLL('libgdk-3.so.0'), 'gdk_screen_get_default', ptr)()
        fn(gtk, 'gtk_style_context_add_provider_for_screen', None, ptr, ptr, C.c_uint)(screen, provider, 1200)
    image = fn(gtk, 'gtk_image_new_from_icon_name', ptr, text, int_)(b'fixture-icon', 3)
    fn(gtk, 'gtk_widget_set_size_request', None, ptr, int_, int_)(image, 24, 24)
    fn(gtk, 'gtk_container_add', None, ptr, ptr)(window, image)
    fn(gtk, 'gtk_widget_show_all', None, ptr)(window)
    surface = fn(cairo, 'cairo_image_surface_create', ptr, int_, int_, int_)(0, 24*scale, 24*scale)
    draw = fn(cairo, 'cairo_create', ptr, ptr)(surface)
    fn(cairo, 'cairo_scale', None, ptr, C.c_double, C.c_double)(draw, scale, scale)
    fn(gtk, 'gtk_widget_draw', None, ptr, ptr)(image, draw)
    fn(cairo, 'cairo_surface_flush', None, ptr)(surface)
    stride = fn(cairo, 'cairo_image_surface_get_stride', int_, ptr)(surface)
    pixels = C.string_at(fn(cairo, 'cairo_image_surface_get_data', ptr, ptr)(surface), stride * 24*scale)
    rgba = bytearray()
    for y in range(24*scale):
        for x in range(24*scale):
            value = int.from_bytes(pixels[y*stride+x*4:y*stride+x*4+4], sys.byteorder)
            alpha = value >> 24
            rgba.extend(min(255, (((value >> shift) & 255)*255+alpha//2)//alpha) if alpha else 0 for shift in (16, 8, 0))
            rgba.append(alpha)
    fn(cairo, 'cairo_destroy', None, ptr)(draw)
    fn(cairo, 'cairo_surface_destroy', None, ptr)(surface)
    fn(gtk, 'gtk_widget_destroy', None, ptr)(window)
    if provider:
        fn(gtk, 'gtk_style_context_remove_provider_for_screen', None, ptr, ptr)(screen, provider)
        fn(obj, 'g_object_unref', None, ptr)(provider)
    return bytes(rgba)


def make_icon_themes(directory):
    def png(path, color):
        def chunk(kind, data):
            return struct.pack('!I',len(data))+kind+data+struct.pack('!I',zlib.crc32(kind+data))
        raw=b''.join(b'\0'+bytes(color)*16 for _ in range(16))
        path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',16,16,8,6,0,0,0))+
                         chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
    for name in ('FixtureMain','FixtureBase','FixtureOther'):
        root=directory/name
        images=root/'16x16/apps'
        images.mkdir(parents=True,exist_ok=True)
        inherited='Inherits=FixtureBase\n' if name=='FixtureMain' else ''
        (root/'index.theme').write_text(f'[Icon Theme]\nName={name}\n{inherited}Directories=16x16/apps\n\n'
                                      '[16x16/apps]\nSize=16\nType=Scalable\nMinSize=1\nMaxSize=256\nContext=Applications\n')
        if name=='FixtureBase':
            png(images/'fixture-icon.png',(40,80,120,160))
        if name=='FixtureOther':
            png(images/'fixture-icon.png',(180,70,20,255))
        if name=='FixtureMain':
            (images/'fixture-broken.png').write_bytes(b'\x89PNG\r\n\x1a\ntruncated')
            for icon,fill in (('fixture-vector','#c04020'),('fixture-symbolic','#2e3436')):
                (images/(icon+'.svg')).write_text('<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16">'
                                               f'<rect x="2" y="2" width="12" height="12" fill="{fill}"/></svg>')


def reference_icon(name,scale,color='#112233'):
    gtk=C.CDLL('libgtk-3.so.0'); obj=C.CDLL('libgobject-2.0.so.0'); pix=C.CDLL('libgdk_pixbuf-2.0.so.0')
    ptr,text,int_=C.c_void_p,C.c_char_p,C.c_int
    def fn(lib,name,result,*args):
        f=getattr(lib,name); f.restype,f.argtypes=result,args; return f
    theme=fn(gtk,'gtk_icon_theme_get_default',ptr)()
    path=(text*1)(os.fsencode(os.environ['GTK_STYLE_ICON_PATH']))
    fn(gtk,'gtk_icon_theme_set_search_path',None,ptr,C.POINTER(text),int_)(theme,path,1)
    settings=fn(gtk,'gtk_settings_get_default',ptr)()
    fn(obj,'g_object_set',None,ptr,text)(settings,b'gtk-icon-theme-name',text(b'FixtureMain'),None)
    info=fn(gtk,'gtk_icon_theme_lookup_icon_for_scale',ptr,ptr,text,int_,int_,int_)(theme,name.encode(),16,scale,16)
    assert info
    # The original fixture CSS defines this context's symbolic foreground.
    widget=fn(gtk,'gtk_button_new',ptr)()
    context=fn(gtk,'gtk_widget_get_style_context',ptr,ptr)(widget)
    provider=fn(gtk,'gtk_css_provider_new',ptr)()
    css=('* { color: '+color+'; }').encode()
    assert fn(gtk,'gtk_css_provider_load_from_data',int_,ptr,text,C.c_ssize_t,ptr)(provider,css,-1,None)
    fn(gtk,'gtk_style_context_add_provider',None,ptr,ptr,C.c_uint)(context,provider,800)
    image=fn(gtk,'gtk_icon_info_load_symbolic_for_context',ptr,ptr,ptr,ptr,ptr)(info,context,None,None)
    assert image
    get=lambda field:fn(pix,'gdk_pixbuf_get_'+field,int_,ptr)(image)
    w,h,stride,channels=get('width'),get('height'),get('rowstride'),get('n_channels')
    assert (w,h)==(16*scale,16*scale)
    data=fn(pix,'gdk_pixbuf_get_pixels',ptr,ptr)(image)
    pixels=C.string_at(data,(h-1)*stride+w*channels)
    rgba=bytearray()
    for y in range(h):
        for x in range(w):
            at=y*stride+x*channels
            rgba.extend(pixels[at:at+3]); rgba.append(pixels[at+3] if channels==4 else 255)
    unref=fn(obj,'g_object_unref',None,ptr)
    for value in (image,info,provider):unref(value)
    fn(gtk,'gtk_widget_destroy',None,ptr)(widget)
    return bytes(rgba)


def reference_text(variant, scale, screen_css=None, local_css=None, state=0, composite_alpha=None, composite_style=True):
    gtk=C.CDLL('libgtk-3.so.0'); gdk=C.CDLL('libgdk-3.so.0'); obj=C.CDLL('libgobject-2.0.so.0')
    pango=C.CDLL('libpango-1.0.so.0'); cairo=C.CDLL('libcairo.so.2')
    ptr,text,int_,double=C.c_void_p,C.c_char_p,C.c_int,C.c_double
    def fn(lib,name,result,*args):
        f=getattr(lib,name); f.restype,f.argtypes=result,args; return f
    assert fn(gtk,'gtk_init_check',int_,ptr,ptr)(None,None)
    screen=fn(gdk,'gdk_screen_get_default',ptr)()
    provider=fn(gtk,'gtk_css_provider_new',ptr)()
    assert fn(gtk,'gtk_css_provider_load_from_path',int_,ptr,text,ptr)(provider,os.fsencode(CSS),None)
    fn(gtk,'gtk_style_context_add_provider_for_screen',None,ptr,ptr,C.c_uint)(screen,provider,800)
    window=fn(gtk,'gtk_window_new',ptr,int_)(0); button=fn(gtk,'gtk_button_new',ptr)()
    fn(gtk,'gtk_container_add',None,ptr,ptr)(window,button)
    context_for=fn(gtk,'gtk_widget_get_style_context',ptr,ptr)
    root_context,context=context_for(window),context_for(button)
    add_class=fn(gtk,'gtk_style_context_add_class',None,ptr,text)
    add_class(root_context,b'fixture-root'); add_class(context,b'flat')
    add_class(context,('normal' if variant in ('override','markup') else variant).encode())
    fn(gtk,'gtk_widget_set_name',None,ptr,text)(button,b'target')
    fn(gtk,'gtk_style_context_set_scale',None,ptr,int_)(context,scale)
    extra=[]
    for css, local in ((screen_css,False),(local_css,True)):
        if css is None: continue
        new=fn(gtk,'gtk_css_provider_new',ptr)()
        assert fn(gtk,'gtk_css_provider_load_from_data',int_,ptr,text,C.c_ssize_t,ptr)(new,css.encode(),-1,None)
        if local: fn(gtk,'gtk_style_context_add_provider',None,ptr,ptr,C.c_uint)(context,new,900)
        else: fn(gtk,'gtk_style_context_add_provider_for_screen',None,ptr,ptr,C.c_uint)(screen,new,900)
        extra.append((new,local))
    fn(gtk,'gtk_style_context_set_state',None,ptr,C.c_uint)(context,state)
    layout=fn(gtk,'gtk_widget_create_pango_layout',ptr,ptr,text)(button,'Café 世界\nLabel'.encode())
    if variant=='markup':
        fn(pango,'pango_layout_set_markup',None,ptr,text,int_)(layout,
            '<span foreground="#e04020"><b>Café &amp; 世界</b></span>\n&lt;Label&gt;'.encode(),-1)
    if variant=='override':
        font=fn(pango,'pango_font_description_from_string',ptr,text)(b'DejaVu Sans Bold Italic 18')
        fn(pango,'pango_layout_set_font_description',None,ptr,ptr)(layout,font)
        fn(pango,'pango_font_description_free',None,ptr)(font)
    fn(pango,'pango_layout_set_width',None,ptr,int_)(layout,96*1024)
    fn(pango,'pango_layout_set_alignment',None,ptr,int_)(layout,1)
    fn(pango,'pango_layout_set_wrap',None,ptr,int_)(layout,2)
    tw,th=int_(),int_()
    fn(pango,'pango_layout_get_pixel_size',None,ptr,C.POINTER(int_),C.POINTER(int_))(layout,C.byref(tw),C.byref(th))
    w,h=96*scale,64*scale
    surface=fn(cairo,'cairo_image_surface_create',ptr,int_,int_,int_)(0,w,h)
    draw=fn(cairo,'cairo_create',ptr,ptr)(surface)
    fn(cairo,'cairo_scale',None,ptr,double,double)(draw,scale,scale)
    if composite_alpha is not None:
        # Import the test's straight RGBA backdrop as native premultiplied
        # image pixels (a floating-point solid color has different rounding).
        data=fn(cairo,'cairo_image_surface_get_data',ptr,ptr)(surface)
        background=bytes((59,41,31,255) if composite_alpha==255 else (18,12,9,77))
        C.memmove(data,background*(w*h),w*h*4)
        fn(cairo,'cairo_surface_mark_dirty',None,ptr)(surface)
        # The raster API returns native CSS/text layers on transparency;
        # compare their composition with Cairo, without reimplementing OVER.
        if composite_style:
            fn(cairo,'cairo_push_group',None,ptr)(draw)
            fn(gtk,'gtk_render_background',None,ptr,ptr,double,double,double,double)(context,draw,0,0,96,64)
            fn(gtk,'gtk_render_frame',None,ptr,ptr,double,double,double,double)(context,draw,0,0,96,64)
            fn(cairo,'cairo_pop_group_to_source',None,ptr)(draw)
            fn(cairo,'cairo_paint',None,ptr)(draw)
            fn(cairo,'cairo_push_group',None,ptr)(draw)
        fn(pango,'pango_layout_set_ellipsize',None,ptr,int_)(layout,0)
    y=(64-th.value)/2
    if composite_alpha is not None: y=y//1
    fn(gtk,'gtk_render_layout',None,ptr,ptr,double,double,ptr)(context,draw,0,y,layout)
    if composite_alpha is not None and composite_style:
        fn(cairo,'cairo_pop_group_to_source',None,ptr)(draw)
        fn(cairo,'cairo_paint',None,ptr)(draw)
    fn(cairo,'cairo_surface_flush',None,ptr)(surface)
    stride=fn(cairo,'cairo_image_surface_get_stride',int_,ptr)(surface)
    pixels=C.string_at(fn(cairo,'cairo_image_surface_get_data',ptr,ptr)(surface),stride*h)
    rgba=bytearray()
    for y in range(h):
        for x in range(w):
            value=int.from_bytes(pixels[y*stride+x*4:y*stride+x*4+4],sys.byteorder)
            alpha=value>>24
            rgba.extend(min(255,(((value>>shift)&255)*255+alpha//2)//alpha) if alpha else 0 for shift in (16,8,0))
            rgba.append(alpha)
    fn(cairo,'cairo_destroy',None,ptr)(draw); fn(cairo,'cairo_surface_destroy',None,ptr)(surface)
    fn(obj,'g_object_unref',None,ptr)(layout)
    for new,local in extra:
        if not local: fn(gtk,'gtk_style_context_remove_provider_for_screen',None,ptr,ptr)(screen,new)
        fn(obj,'g_object_unref',None,ptr)(new)
    fn(gtk,'gtk_widget_destroy',None,ptr)(window)
    fn(gtk,'gtk_style_context_remove_provider_for_screen',None,ptr,ptr)(screen,provider)
    fn(obj,'g_object_unref',None,ptr)(provider)
    return bytes(rgba)


def reference_native_font_widget(explicit):
    """Capture an actual X11 GtkLabel; image-surface references miss defaults."""
    gtk=C.CDLL('libgtk-3.so.0'); gdk=C.CDLL('libgdk-3.so.0')
    cairo=C.CDLL('libcairo.so.2'); pixbuf=C.CDLL('libgdk_pixbuf-2.0.so.0'); obj=C.CDLL('libgobject-2.0.so.0')
    ptr, int_, text=C.c_void_p, C.c_int, C.c_char_p
    def fn(lib,name,result,*args):
        function=getattr(lib,name); function.restype=result; function.argtypes=args; return function
    assert fn(gtk,'gtk_init_check',int_,ptr,ptr)(None,None)
    screen=fn(gdk,'gdk_screen_get_default',ptr)()
    if explicit:
        settings=fn(gtk,'gtk_settings_get_default',ptr)()
        setter=obj.g_object_set; setter.restype=None; setter.argtypes=(ptr,text)
        setter(settings,b'gtk-xft-antialias',int_(0),b'gtk-xft-hinting',int_(1),
               b'gtk-xft-hintstyle',text(b'hintmedium'),None)
    css=b'window.font-native, label.font-native { font:10pt sans; color:#c3c7d1; background-color:#181b28; background-image:none; border:0; border-radius:0; padding:0; margin:0; text-shadow:none; }'
    provider=fn(gtk,'gtk_css_provider_new',ptr)()
    assert fn(gtk,'gtk_css_provider_load_from_data',int_,ptr,text,C.c_ssize_t,ptr)(provider,css,-1,None)
    fn(gtk,'gtk_style_context_add_provider_for_screen',None,ptr,ptr,C.c_uint)(screen,provider,1000)
    window=fn(gtk,'gtk_window_new',ptr,int_)(0)
    label=fn(gtk,'gtk_label_new',ptr,text)('File 01 Folder Café'.encode())
    try:
        fn(gtk,'gtk_window_set_decorated',None,ptr,int_)(window,0)
        fn(gtk,'gtk_label_set_xalign',None,ptr,C.c_float)(label,0)
        fn(gtk,'gtk_widget_set_size_request',None,ptr,int_,int_)(label,200,28)
        for widget in (window,label):
            context=fn(gtk,'gtk_widget_get_style_context',ptr,ptr)(widget)
            fn(gtk,'gtk_style_context_add_class',None,ptr,text)(context,b'font-native')
        fn(gtk,'gtk_container_add',None,ptr,ptr)(window,label)
        fn(gtk,'gtk_widget_show_all',None,ptr)(window)
        # A mapped window may not have reached its first frame.  Capturing
        # immediately after draining pending events can return the cleared
        # X11 background, particularly for the second window in this test.
        frame=fn(gtk,'gtk_widget_get_frame_clock',ptr,ptr)(window)
        painted=[]
        callback_type=C.CFUNCTYPE(None,ptr,ptr)
        callback=callback_type(lambda clock,data: painted.append(True))
        signal=fn(obj,'g_signal_connect_data',C.c_ulong,ptr,text,callback_type,ptr,ptr,C.c_uint)(
            frame,b'after-paint',callback,None,None,0)
        try:
            import time
            deadline=time.monotonic()+5
            while not painted and time.monotonic()<deadline:
                fn(gtk,'gtk_main_iteration_do',int_,int_)(False)
                time.sleep(.001)
            assert painted, 'Native X11 label did not finish its first frame'
        finally:
            fn(obj,'g_signal_handler_disconnect',None,ptr,C.c_ulong)(frame,signal)
        display=fn(gdk,'gdk_screen_get_display',ptr,ptr)(screen)
        fn(gdk,'gdk_display_sync',None,ptr)(display)
        native=fn(gtk,'gtk_widget_get_window',ptr,ptr)(window)
        image=fn(gdk,'gdk_pixbuf_get_from_window',ptr,ptr,int_,int_,int_,int_)(native,0,0,200,28)
        assert image, 'Native X11 label capture unavailable'
        try:
            channels=fn(pixbuf,'gdk_pixbuf_get_n_channels',int_,ptr)(image)
            stride=fn(pixbuf,'gdk_pixbuf_get_rowstride',int_,ptr)(image)
            pixels=C.string_at(fn(pixbuf,'gdk_pixbuf_get_pixels',ptr,ptr)(image),stride*28)
            rgba=bytearray()
            for y in range(28):
                for x in range(200):
                    at=y*stride+x*channels
                    rgba.extend(pixels[at:at+3]); rgba.append(255)
            return bytes(rgba)
        finally: fn(obj,'g_object_unref',None,ptr)(image)
    finally:
        fn(gtk,'gtk_widget_destroy',None,ptr)(window)
        fn(gtk,'gtk_style_context_remove_provider_for_screen',None,ptr,ptr)(screen,provider)
        fn(obj,'g_object_unref',None,ptr)(provider)


def execute(probe, output):
    # Only this subprocess has the private server's display and private bus.
    run([probe, CSS, output, 'full'])
    run([probe, CSS, output, 'rebind'])
    for name, alpha in (('opaque',255),('translucent',77)):
        actual=(output/f'composite-{name}.rgba').read_bytes()
        expected=reference_text('composite',1,composite_alpha=alpha)
        (output/f'composite-{name}-reference.rgba').write_bytes(expected)
        different=[(i//4%96,i//4//96,list(actual[i:i+4]),list(expected[i:i+4])) for i in range(0,len(actual),4) if actual[i:i+4]!=expected[i:i+4]]
        assert actual==expected, (name,'native premultiplied gradient/text/shadow OVER',len(different),different[:6])
    assert (output/'composite-plain-text.rgba').read_bytes()==reference_text('plain',1,composite_alpha=255,composite_style=False), 'direct native GTK opaque text pixels'
    print('GTK composition: direct opaque glyph pixels; cached gradient/frame/shadow layers match native Cairo OVER on opaque/translucent destinations',flush=True)
    for name, scale in (('normal', 1), ('scale2', 2)):
        actual = (output / (name + '.rgba')).read_bytes()
        expected = reference(scale)
        if actual != expected:
            differences = [(i, a, b) for i, (a, b) in enumerate(zip(actual, expected)) if a != b]
            raise AssertionError(f'{name}: GTK widget reference differs: {len(differences)} bytes; first {differences[:8]}')
        assert len(set(actual[3::4])) > 2, 'Alpha gradient was flattened'
        assert actual[:4] != actual[-4:], 'CSS gradient/frame was flattened'
    updated = (output / 'updated.rgba').read_bytes()
    assert updated == bytes((10, 20, 30, 128)) * (24*16), updated[:16]
    for name in ('fixture-icon','fixture-vector','fixture-symbolic'):
        for scale in (1,2):
            suffix='-2' if scale==2 else ''
            assert (output/(name+suffix+'.rgba')).read_bytes()==reference_icon(name,scale), (name,scale)
    assert (output/'icon-updated.rgba').read_bytes()==bytes((180,70,20,255))*(16*16)
    preferred = [tuple(map(int, row.split())) for row in (output/'preferred-metrics.rgba').read_text().splitlines()]
    assert preferred == reference_preferred(), ('native widget requisitions', preferred, reference_preferred())
    separators = tuple(map(int, (output/'tree-separators.rgba').read_text().split()))
    assert separators == reference_tree_separators(), ('actual GtkTreeView separators', separators, reference_tree_separators())
    assert (output/'native-image.rgba').read_bytes() == reference_image(), 'Actual GtkImage natural icon sizing/centering'
    for scale in (1, 2):
        if scale == 1:
            expected = reference_image(1, True)
        else:
            # A fresh GTK process must actually have a scale-2 widget; scaling
            # a scale-1 GtkImage's completed pixels tests a different path.
            path = output/'native-scaled-image.rgba'
            run([sys.executable, __file__, '--scaled-image-reference', path], env={**os.environ, 'GDK_SCALE': '2'})
            expected = path.read_bytes()
        assert (output/f'styled-icon-{scale}.rgba').read_bytes() == expected, ('Actual GtkImage CSS icon transform/shadow', scale)
    assert (output/'symbolic-updated.rgba').read_bytes()==reference_icon('fixture-symbolic',1,'#abcdef')
    texts=[]
    for variant in ('normal','bold','italic','override','markup'):
        for scale in (1,2):
            actual=(output/f'text-{variant}-{scale}.rgba').read_bytes()
            expected=reference_text(variant,scale)
            assert actual==expected, (variant,scale,sum(a!=b for a,b in zip(actual,expected)))
            assert any(actual[3::4]), 'Text produced no ink'
            texts.append(actual)
    assert len(set(texts))==10, 'Font styles, explicit font, markup or scale were ignored'
    screen_css='* { padding:7px; color:#123456; background-image:none; background-color:#203040; border:0; border-radius:0; font:16px DejaVu Sans; text-shadow:none; } .fixture-root:disabled button { padding:11px; } button:disabled { color:#556677; }'
    local_css='* { padding:1px; color:#abcdef; background-color:#504030; font:20px DejaVu Sans; } *:disabled { color:#987654; }'
    replacement='* { padding:2px; color:#aabbcc; background-color:#304050; font:18px DejaVu Sans; }'
    for name,css,state,color in (('screen',None,0,'#123456'),('local',local_css,0,'#abcdef'),
                                 ('state',local_css,8,'#987654'),('rejected',local_css,0,'#abcdef'),
                                 ('replaced',replacement,0,'#aabbcc'),('released',None,0,'#123456')):
        assert (output/f'provider-{name}.rgba').read_bytes()==reference(1,screen_css,css,state), name
        assert (output/f'provider-{name}-text.rgba').read_bytes()==reference_text('normal',1,screen_css,css,state), (name,'text')
        assert (output/f'provider-{name}-icon.rgba').read_bytes()==reference_icon('fixture-symbolic',1,color), (name,'icon')
    print('GTK providers: independent real-widget query/render/text/icon/state lifecycle reference passed',flush=True)
    print('GTK widget reference: exact gradient/frame/icon/text/markup pixels at scales 1 and 2; live custom provider pixels passed', flush=True)
    execute_backdrop(probe,output)
    run([probe,CSS,output,'native-rounded'])
    expected_rounded,image_rounded=reference_rounded_window()
    assert image_rounded!=expected_rounded, 'Rounded fixture must exercise native curve coverage'
    (output/'native-rounded-reference.rgba').write_bytes(expected_rounded)
    actual_rounded=(output/'native-rounded.rgba').read_bytes()
    assert actual_rounded==expected_rounded, ('Actual native rounded GTK window',sum(a!=b for a,b in zip(actual_rounded,expected_rounded)))
    print('GTK native rounded window: exact real widget pixels after backdrop upload',flush=True)
    font_results=[]
    for explicit in (False,True):
        name='native-font-explicit' if explicit else 'native-font-default'
        run([probe,CSS,output,name])
        actual=(output/(name+'.rgba')).read_bytes()
        expected=reference_native_font_widget(explicit)
        (output/(name+'-reference.rgba')).write_bytes(expected)
        assert actual==expected, (name,'actual X11 GtkLabel vs native raster/CPU composition',sum(a!=b for a,b in zip(actual,expected)))
        font_results.append(actual)
    assert font_results[0]!=font_results[1], 'Default and explicit hinting fixtures must exercise different glyph pixels'
    print('GTK fonts: exact actual X11 GtkLabel pixels with discovered backend defaults and explicit GTK font settings',flush=True)


def reference_rounded_window():
    """Actual owned GTK window pixels, independent of any raster helper."""
    gtk,gdk,obj=(C.CDLL(n) for n in ('libgtk-3.so.0','libgdk-3.so.0','libgobject-2.0.so.0'))
    ptr,int_,text=C.c_void_p,C.c_int,C.c_char_p
    def fn(lib,name,result,*args):
        f=getattr(lib,name);f.restype,f.argtypes=result,args;return f
    assert fn(gtk,'gtk_init_check',int_,ptr,ptr)(None,None)
    screen=fn(gdk,'gdk_screen_get_default',ptr)()
    provider=fn(gtk,'gtk_css_provider_new',ptr)()
    assert fn(gtk,'gtk_css_provider_load_from_path',int_,ptr,text,ptr)(provider,os.fsencode(CSS),None)
    fn(gtk,'gtk_style_context_add_provider_for_screen',None,ptr,ptr,C.c_uint)(screen,provider,800)
    window=fn(gtk,'gtk_window_new',ptr,int_)(0)
    button=fn(gtk,'gtk_entry_new',ptr)()
    fn(gtk,'gtk_entry_set_width_chars',None,ptr,int_)(button,1)
    fn(gtk,'gtk_entry_set_max_width_chars',None,ptr,int_)(button,1)
    context=fn(gtk,'gtk_widget_get_style_context',ptr,ptr)
    add=fn(gtk,'gtk_style_context_add_class',None,ptr,text)
    add(context(window),b'rounded-root');add(context(button),b'flat')
    fn(gtk,'gtk_widget_set_name',None,ptr,text)(button,b'target')
    fn(gtk,'gtk_widget_set_can_focus',None,ptr,int_)(button,0)
    fn(gtk,'gtk_container_add',None,ptr,ptr)(window,button)
    fn(gtk,'gtk_window_set_default_size',None,ptr,int_,int_)(window,96,40)
    fn(gtk,'gtk_widget_show_all',None,ptr)(window)
    import time
    for _ in range(30):
        while fn(gtk,'gtk_events_pending',int_)():fn(gtk,'gtk_main_iteration_do',int_,int_)(0)
        time.sleep(.01)
    native=fn(gtk,'gtk_widget_get_window',ptr,ptr)(window)
    assert fn(gdk,'gdk_window_get_width',int_,ptr)(native)==96
    assert fn(gdk,'gdk_window_get_height',int_,ptr)(native)==40
    pixbuf=fn(gdk,'gdk_pixbuf_get_from_window',ptr,ptr,int_,int_,int_,int_)(native,0,0,96,40)
    assert pixbuf
    pix=C.CDLL('libgdk_pixbuf-2.0.so.0')
    stride=fn(pix,'gdk_pixbuf_get_rowstride',int_,ptr)(pixbuf)
    channels=fn(pix,'gdk_pixbuf_get_n_channels',int_,ptr)(pixbuf)
    data=fn(pix,'gdk_pixbuf_get_pixels',ptr,ptr)(pixbuf)
    pixels=C.string_at(data,stride*40);result=bytearray()
    for y in range(40):
        for x in range(96):
            at=y*stride+x*channels;result.extend(pixels[at:at+3]);result.append(255)
    cairo=C.CDLL('libcairo.so.2');double=C.c_double
    surface=fn(cairo,'cairo_image_surface_create',ptr,int_,int_,int_)(0,96,40)
    draw=fn(cairo,'cairo_create',ptr,ptr)(surface)
    fn(cairo,'cairo_set_source_rgb',None,ptr,double,double,double)(draw,12/255,14/255,20/255)
    fn(cairo,'cairo_paint',None,ptr)(draw)
    for name in ('gtk_render_background','gtk_render_frame'):
        fn(gtk,name,None,ptr,ptr,double,double,double,double)(context(button),draw,0,0,96,40)
    fn(cairo,'cairo_surface_flush',None,ptr)(surface)
    stride=fn(cairo,'cairo_image_surface_get_stride',int_,ptr)(surface)
    data=fn(cairo,'cairo_image_surface_get_data',ptr,ptr)(surface)
    raw=C.string_at(data,stride*40);image=bytearray()
    for y in range(40):
        for x in range(96):
            pixel=int.from_bytes(raw[y*stride+x*4:y*stride+x*4+4],sys.byteorder)
            image.extend(((pixel>>16)&255,(pixel>>8)&255,pixel&255,255))
    fn(cairo,'cairo_destroy',None,ptr)(draw)
    fn(cairo,'cairo_surface_destroy',None,ptr)(surface)
    fn(obj,'g_object_unref',None,ptr)(pixbuf)
    fn(gtk,'gtk_widget_destroy',None,ptr)(window)
    fn(gtk,'gtk_style_context_remove_provider_for_screen',None,ptr,ptr)(screen,provider)
    fn(obj,'g_object_unref',None,ptr)(provider)
    return bytes(result),bytes(image)


def execute_backdrop(probe,output):
    output.mkdir(exist_ok=True)
    run([probe,CSS,output,'backdrop'])
    for scale in (1,2):
        suffix='' if scale==1 else '-2'
        expected=reference(scale,backdrop=True)
        (output/f'backdrop-reference{suffix}.rgba').write_bytes(expected)
        actual=(output/f'backdrop-full{suffix}.rgba').read_bytes()
        assert actual==expected, ('Native-compatible Cairo CSS backdrop',scale,
            sum(a!=b for a,b in zip(actual,expected)),[(i,a,b) for i,(a,b) in enumerate(zip(actual,expected)) if a!=b][:12])
        w=24*scale
        cropped=b''.join(expected[(y*w+3*scale)*4:(y*w+20*scale)*4] for y in range(2*scale,13*scale))
        assert (output/f'backdrop-crop{suffix}.rgba').read_bytes()==cropped, ('Backdrop crop CSS coordinates',scale)
        if scale==1:
            painter=bytearray()
            for y in range(16):
                for x in range(24):painter.extend((12+x%5,14+y%7,20+(x+y)%9,0 if x%5==0 else 128 if x%3==0 else 255))
            for y in range(3,12):painter[(y*w+4)*4:(y*w+16)*4]=expected[(y*w+4)*4:(y*w+16)*4]
            assert (output/'backdrop-painter.rgba').read_bytes()==painter, 'Painter replacement and parent clip'
    print('GTK native backdrop: exact alpha 0/128/255, CSS scales 1/2, crop, parent clip; GDK_SCALE='+os.environ.get('GDK_SCALE','1'),flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', nargs=2, metavar=('PROBE', 'OUTPUT'))
    parser.add_argument('--execute-backdrop', nargs=2, metavar=('PROBE', 'OUTPUT'))
    args = parser.parse_args()
    if args.execute:
        execute(Path(args.execute[0]), Path(args.execute[1]))
        return
    if args.execute_backdrop:
        execute_backdrop(Path(args.execute_backdrop[0]),Path(args.execute_backdrop[1]))
        return
    from toolchain import UI_MODULE_ARGS, ZIRAN, ZIRAN_ROOT
    env = {k: v for k, v in os.environ.items() if k not in SCRUB}
    env.pop('GTK_THEME', None)
    env.update(YUE_DESKTOP_RECOVERY='0', GDK_BACKEND='x11', NO_AT_BRIDGE='1',
               GSETTINGS_BACKEND='memory', GIO_USE_VFS='local', G_DEBUG='fatal-criticals')
    WORK.mkdir(parents=True, exist_ok=True)
    for variable, folder in (('HOME', 'home'), ('XDG_CONFIG_HOME', 'config'),
                             ('XDG_DATA_HOME', 'data'), ('XDG_CACHE_HOME', 'cache'),
                             ('XDG_STATE_HOME', 'state'), ('XDG_RUNTIME_DIR', 'runtime'), ('TMPDIR', 'tmp')):
        private = WORK / folder
        private.mkdir(mode=0o700, exist_ok=True)
        private.chmod(0o700)
        env[variable] = str(private)
    config = WORK / 'config'
    settings = config / 'gtk-3.0'
    settings.mkdir(parents=True, exist_ok=True)
    (settings / 'settings.ini').write_text('[Settings]\ngtk-theme-name=Adwaita\ngtk-xft-dpi=98304\n')
    env['XDG_CONFIG_HOME'] = str(config)
    make_icon_themes(WORK/'icons')
    env['GTK_STYLE_ICON_PATH']=str(WORK/'icons')
    source = ROOT / 'tests/gtk_style_linux_test.zi'
    ir = WORK / 'ir'
    common = ['--module-path', ROOT / 'src/backend', *UI_MODULE_ARGS,
              '--module-path', ZIRAN_ROOT / 'std']
    for package in ('webp', 'image_codec', 'avif'):
        package_root = Path(subprocess.check_output([ZIRAN, 'pkg', 'path', package], cwd=ROOT, text=True).strip())
        common += ['--module-path', package+'='+str(package_root/'src')]
    run([ZIRAN, 'ir', '--root', ROOT / 'tests', *common, '-o', ir, source], env=env)
    libs = subprocess.check_output(['pkg-config', '--libs', 'gtk+-3.0', 'cairo-ft', 'fontconfig'], text=True).split()
    for form in ('source', 'saved'):
        output = WORK / form
        output.mkdir(exist_ok=True)
        root, input_ = (ROOT / 'tests', source) if form == 'source' else (ir, ir / 'gtk_style_linux_test.zir')
        run([ZIRAN, 'build', '--target=c', '--no-main', '--root', root, *common,
             '-o', output, input_], env=env)
        probe = output / 'probe'
        run([os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-ffunction-sections', '-fdata-sections',
             '-Wl,--gc-sections', '-I'+str(ZIRAN_ROOT / 'include'),
             '-I'+str(output), *sorted(output.glob('*.c')), *libs, '-lm',
             '-Wl,--wrap=calloc', '-Wl,--wrap=free', '-Wl,--wrap=cairo_image_surface_create',
             '-o', probe], env=env)
        run([probe, CSS, output, 'no-display'], env=env, timeout=30)
        run(['timeout', '--kill-after=2s', '60s', 'xvfb-run', '-a', '-n', '390',
             '-s', '-screen 0 640x480x24 -nolisten tcp', 'dbus-run-session', '--',
             sys.executable, __file__, '--execute', probe, output], env=env, timeout=70)
        run(['timeout','--kill-after=2s','30s','xvfb-run','-a','-n','390',
             '-s','-screen 0 640x480x24 -nolisten tcp','dbus-run-session','--',
             sys.executable,__file__,'--execute-backdrop',probe,output/'gdk-scale-2'],
            env=env | {'GDK_SCALE':'2'},timeout=40)
        print(f'GTK native style {form}: PASS', flush=True)


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--scaled-image-reference':
        Path(sys.argv[2]).write_bytes(reference_image(2, True))
    else:
        main()
