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


def reference(scale, screen_css=None, local_css=None, state=0):
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
    fn(cairo, 'cairo_scale', None, ptr, double, double)(draw, scale, scale)
    for name in ('gtk_render_background', 'gtk_render_frame'):
        fn(gtk, name, None, ptr, ptr, double, double, double, double)(context, draw, 0, 0, 24, 16)
    fn(cairo, 'cairo_surface_flush', None, ptr)(surface)
    stride = fn(cairo, 'cairo_image_surface_get_stride', int_, ptr)(surface)
    data = fn(cairo, 'cairo_image_surface_get_data', ptr, ptr)(surface)
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
    fn(cairo, 'cairo_destroy', None, ptr)(draw)
    fn(cairo, 'cairo_surface_destroy', None, ptr)(surface)
    for new,local in extra:
        if not local: fn(gtk,'gtk_style_context_remove_provider_for_screen',None,ptr,ptr)(screen,new)
        fn(obj,'g_object_unref',None,ptr)(new)
    fn(gtk, 'gtk_widget_destroy', None, ptr)(window)
    fn(gtk, 'gtk_style_context_remove_provider_for_screen', None, ptr, ptr)(screen, provider)
    fn(obj, 'g_object_unref', None, ptr)(provider)
    return bytes(result)


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


def reference_text(variant, scale, screen_css=None, local_css=None, state=0):
    gtk=C.CDLL('libgtk-3.so.0'); gdk=C.CDLL('libgdk-3.so.0'); obj=C.CDLL('libgobject-2.0.so.0')
    pango=C.CDLL('libpango-1.0.so.0'); cairo=C.CDLL('libcairo.so.2')
    ptr,text,int_,double=C.c_void_p,C.c_char_p,C.c_int,C.c_double
    def fn(lib,name,result,*args):
        f=getattr(lib,name); f.restype,f.argtypes=result,args; return f
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
    fn(gtk,'gtk_render_layout',None,ptr,ptr,double,double,ptr)(context,draw,0,(64-th.value)/2,layout)
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


def execute(probe, output):
    # Only this subprocess has the private server's display and private bus.
    run([probe, CSS, output, 'full'])
    run([probe, CSS, output, 'rebind'])
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', nargs=2, metavar=('PROBE', 'OUTPUT'))
    args = parser.parse_args()
    if args.execute:
        execute(Path(args.execute[0]), Path(args.execute[1]))
        return
    from toolchain import ZIRAN, ZIRAN_ROOT
    env = {k: v for k, v in os.environ.items() if k not in SCRUB}
    env.pop('GTK_THEME', None)
    env.update(YUE_DESKTOP_RECOVERY='0', GDK_BACKEND='x11', NO_AT_BRIDGE='1',
               GSETTINGS_BACKEND='memory', GIO_USE_VFS='local')
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
    common = ['--module-path', ROOT / 'src/backend', '--module-path', ROOT / 'src/ui',
              '--module-path', ZIRAN_ROOT / 'std']
    run([ZIRAN, 'ir', '--root', ROOT / 'tests', *common, '-o', ir, source], env=env)
    libs = subprocess.check_output(['pkg-config', '--libs', 'gtk+-3.0'], text=True).split()
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
        print(f'GTK native style {form}: PASS', flush=True)


if __name__ == '__main__':
    main()
