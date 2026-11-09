#!/usr/bin/env python3
"""Compare native source/saved scrollbar hosts with independent real GTK widgets."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/scratch/gtk-scroll-linux'

def run(args, **kw):
    try:
        return subprocess.run(list(map(str,args)),cwd=ROOT,check=True,timeout=180,**kw)
    except subprocess.CalledProcessError as error:
        print('FAILED',args,'environment case:',{k:v for k,v in kw.get('env',{}).items() if k.startswith('SCROLL_')},file=sys.stderr)
        if error.stdout:print(error.stdout,file=sys.stderr)
        if error.stderr:print(error.stderr,file=sys.stderr)
        raise

def oracle(case):
    import gi,cairo
    gi.require_version('Gtk','3.0')
    from gi.repository import Gtk,Gdk
    Gtk.Settings.get_default().set_property('gtk-enable-animations',bool(case.get('SCROLL_ANIMATIONS')))
    provider=None
    if case.get('SCROLL_CSS'):
        provider=Gtk.CssProvider();provider.load_from_data(case['SCROLL_CSS'].encode())
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(),provider,800)
    native_capture=case.get('SCROLL_BACKDROP_ALPHA')==255 and case.get('SCROLL_SCALE',1)==1
    window=Gtk.Window() if native_capture else Gtk.OffscreenWindow()
    group=Gtk.WindowGroup();group.add_window(window)
    window.get_style_context().add_class('background')
    grid=Gtk.Grid();window.add(grid)
    scroller=Gtk.ScrolledWindow();grid.add(scroller)
    scroller.set_hexpand(True);scroller.set_vexpand(True)
    scroller.get_style_context().add_class('frame')
    scroller.set_overlay_scrolling(bool(case['SCROLL_OVERLAY']))
    scroller.set_policy(Gtk.PolicyType.AUTOMATIC,Gtk.PolicyType.AUTOMATIC)
    layout=Gtk.Layout();scroller.add(layout)
    layout.set_size(case['SCROLL_CONTENT_WIDTH'],case['SCROLL_CONTENT_HEIGHT'])
    window.set_size_request(case['SCROLL_WIDTH'],case['SCROLL_HEIGHT'])
    window.show_all()
    try:
        while Gtk.events_pending():Gtk.main_iteration_do(False)
        scroller.get_hadjustment().set_value(case['SCROLL_X'])
        scroller.get_vadjustment().set_value(case['SCROLL_Y'])
        end=time.monotonic()+.12
        while time.monotonic()<end:
            while Gtk.events_pending():Gtk.main_iteration_do(False)
            time.sleep(.005)
        if case.get('SCROLL_CSS_CHANGE'):
            provider.load_from_data(case['SCROLL_CSS_CHANGE'].encode())
            end=time.monotonic()+.12
            while time.monotonic()<end:
                while Gtk.events_pending():Gtk.main_iteration_do(False)
                time.sleep(.005)
            window.check_resize()
        def rectangle(widget):
            box=widget.get_allocation();x,y=widget.translate_coordinates(window,0,0)
            return [x,y,box.width,box.height]
        if case.get('SCROLL_WHEEL'):
            result={}
            pointer=Gdk.Display.get_default().get_default_seat().get_pointer()
            for horizontal in (0,1):
                for step in range(8):
                    event=Gdk.Event.new(Gdk.EventType.SCROLL)
                    event.window=layout.get_bin_window()
                    event.send_event=True
                    event.direction=(Gdk.ScrollDirection.RIGHT if step<4 else Gdk.ScrollDirection.LEFT) if horizontal else (Gdk.ScrollDirection.DOWN if step<4 else Gdk.ScrollDirection.UP)
                    event.set_device(pointer)
                    scroller.emit('scroll-event',event)
                    result[f'{horizontal}:{step}']=[scroller.get_hadjustment().get_value(),scroller.get_vadjustment().get_value()]
            return result,b''
        if case.get('SCROLL_IDLE_MS'):
            if case['SCROLL_Y']>0:assert scroller.get_vscrollbar().get_opacity()>0,'offset change must reveal native bar before idle'
            end=time.monotonic()+case['SCROLL_IDLE_MS']/1000
            while time.monotonic()<end:
                while Gtk.events_pending():Gtk.main_iteration_do(False)
                window.check_resize()
                box=scroller.get_vscrollbar().get_allocation()
                scratch=cairo.ImageSurface(cairo.FORMAT_ARGB32,box.width,box.height)
                scroller.get_vscrollbar().draw(cairo.Context(scratch))
                time.sleep(.02)
            assert scroller.get_vscrollbar().get_opacity()==0,'revealed native bar must fade after idle'
        result={}
        action=case.get('SCROLL_ACTION')
        if action:
            horizontal=action=='h';bar=scroller.get_hscrollbar() if horizontal else scroller.get_vscrollbar()
            pointer=Gdk.Display.get_default().get_default_seat().get_pointer()
            def input_window(parent,target):
                for child in parent.get_children():
                    found=input_window(child,target)
                    if found is not None:return found
                probe=Gdk.Event.new(Gdk.EventType.MOTION_NOTIFY);probe.window=parent
                return parent if Gtk.get_event_widget(probe)==target else None
            def event(kind,x,y,down=False):
                grab=(group.get_current_device_grab(pointer) or group.get_current_grab())
                target=grab or (bar if bar.get_opacity()>0 else layout)
                event_window=target.get_bin_window() if target==layout else input_window(window.get_window(),target)
                ox=oy=0;ancestor=event_window;root_window=window.get_window()
                while ancestor!=root_window:
                    dx,dy=ancestor.get_position();ox+=dx;oy+=dy;ancestor=ancestor.get_effective_parent()
                    assert ancestor is not None
                e=Gdk.Event.new(kind);e.window=event_window;e.send_event=True;e.time=Gdk.CURRENT_TIME
                e.set_device(pointer);e.set_source_device(pointer)
                e.x=x-ox;e.y=y-oy;e.x_root=x;e.y_root=y
                e.state=Gdk.ModifierType.BUTTON1_MASK if down else Gdk.ModifierType(0)
                if kind!=Gdk.EventType.MOTION_NOTIFY:e.button=1
                Gtk.main_do_event(e)
                while Gtk.events_pending():Gtk.main_iteration_do(False)
                window.check_resize()
            def settle():
                end=time.monotonic()+.12
                while time.monotonic()<end:
                    while Gtk.events_pending():Gtk.main_iteration_do(False)
                    time.sleep(.005)
                window.check_resize()
            def snapshot(name):
                h=scroller.get_hscrollbar();v=scroller.get_vscrollbar()
                hb=h.get_allocation();vb=v.get_allocation()
                result[name]=[int(any(b.get_style_context().has_class('dragging') for b in (h,v))),h.get_opacity(),v.get_opacity(),
                    scroller.get_hadjustment().get_value(),scroller.get_vadjustment().get_value(),
                    hb.width,hb.height,vb.width,vb.height,h.get_slider_range()[0],v.get_slider_range()[0]]
            x,y,w,h=rectangle(bar);px=x+w*.5;py=y+h*.5
            event(Gdk.EventType.MOTION_NOTIFY,px,py);settle();snapshot('HOVER')
            x,y,w,h=rectangle(bar);start,end=bar.get_slider_range()
            if horizontal:px=x+(start+end)*.5
            else:py=y+(start+end)*.5
            event(Gdk.EventType.MOTION_NOTIFY,px,py);settle()
            event(Gdk.EventType.BUTTON_PRESS,px,py);snapshot('PRESS')
            if horizontal:px+=30
            else:py+=30
            event(Gdk.EventType.MOTION_NOTIFY,px,py,True);snapshot('DRAG')
            if horizontal:px=case['SCROLL_WIDTH']+100
            else:py=case['SCROLL_HEIGHT']+100
            event(Gdk.EventType.MOTION_NOTIFY,px,py,True);snapshot('OUTSIDE')
            event(Gdk.EventType.BUTTON_RELEASE,px,py,True);snapshot('RELEASE')
        result['VIEW']=rectangle(layout)

        for name,bar,adjustment in (('H',scroller.get_hscrollbar(),scroller.get_hadjustment()),('V',scroller.get_vscrollbar(),scroller.get_vadjustment())):
            bounds=rectangle(bar);minimum,_=bar.get_preferred_size()
            slider=bar.get_slider_range()
            result[name]=[int(bar.get_child_visible() and adjustment.get_upper()-adjustment.get_lower()>adjustment.get_page_size()),
                int(bar.get_style_context().has_class('overlay-indicator')),bar.get_opacity(),*bounds,*slider,
                minimum.width,minimum.height,adjustment.get_value(),adjustment.get_lower(),adjustment.get_upper(),adjustment.get_page_size()]
        axis='H' if case.get('SCROLL_RASTER_H') else 'V'
        bar=scroller.get_hscrollbar() if axis=='H' else scroller.get_vscrollbar();box=bar.get_allocation()
        scale=case.get('SCROLL_SCALE',1);width=box.width*scale;height=box.height*scale
        if result[axis][0]:
            alpha=case.get('SCROLL_BACKDROP_ALPHA')
            if alpha is not None:
                # Select a native compatible target, reading only our allocated
                # surface. The opaque scale-one oracle below instead captures
                # the independently drawn real GTK window.
                surface=Gdk.get_default_root_window().create_similar_surface(cairo.CONTENT_COLOR_ALPHA,width,height)
                surface.set_device_scale(1,1)
                initial=cairo.ImageSurface(cairo.FORMAT_ARGB32,width,height)
                context=cairo.Context(initial);context.set_source_rgba(32/255,48/255,80/255,alpha/255);context.paint()
                context=cairo.Context(surface);context.set_operator(cairo.OPERATOR_SOURCE);context.set_source_surface(initial);context.paint();surface.flush()
            else:surface=cairo.ImageSurface(cairo.FORMAT_ARGB32,width,height)
            surface.set_device_scale(scale,scale);bar.draw(cairo.Context(surface));surface.flush()
            mapped=surface.map_to_image(None) if alpha is not None else surface
            raw=bytes(mapped.get_data());rgba=bytearray()
            for y in range(height):
                for x in range(width):
                    at=y*mapped.get_stride()+x*4;b,g,r,a=raw[at:at+4]
                    rgba.extend([min(255,(c*255+a//2)//a) if a else 0 for c in (r,g,b)])
                    rgba.append(a)
            if alpha is not None:surface.unmap_image(mapped)
            if native_capture:
                owned=Gdk.pixbuf_get_from_window(window.get_window(),*result[axis][3:7])
                assert owned and owned.get_width()==width and owned.get_height()==height
                data=owned.get_pixels();stride=owned.get_rowstride();channels=owned.get_n_channels()
                actual_window=bytearray()
                for y in range(height):
                    for x in range(width):
                        at=y*stride+x*channels;actual_window.extend(data[at:at+3]);actual_window.append(255)
                assert actual_window==rgba,'owned native window differs from native scrollbar raster'
                rgba=actual_window
            result['IMAGE']=[width,height,len(rgba)]
        else:result['IMAGE']=[0,0,0];rgba=bytearray()
        if result[axis][0] and result[axis][2]==0:assert not any(rgba[3::4]),'idle native overlay must be transparent'
        return result,bytes(rgba)
    finally:
        window.destroy()
        if provider is not None:Gtk.StyleContext.remove_provider_for_screen(Gdk.Screen.get_default(),provider)

def worker():
    number=os.environ['DISPLAY'].removeprefix(':').split('.')[0]
    pid=int(Path(f'/tmp/.X{number}-lock').read_text())
    assert Path(f'/proc/{pid}/comm').read_text().strip()=='Xvfb'
    assert os.environ['YUE_DESKTOP_RECOVERY']=='0'
    base=dict(SCROLL_WIDTH=300,SCROLL_HEIGHT=200,SCROLL_CONTENT_WIDTH=100,SCROLL_CONTENT_HEIGHT=1000,SCROLL_X=0,SCROLL_Y=0,SCROLL_OVERLAY=1)
    cases=[]
    for overlay in (0,1):
        for width,height in ((100,1000),(1000,100),(1000,1000),(100,100)):
            for offset in (0,350,2147483647):
                cases.append(dict(base,SCROLL_OVERLAY=overlay,SCROLL_CONTENT_WIDTH=width,SCROLL_CONTENT_HEIGHT=height,SCROLL_X=offset,SCROLL_Y=offset))
    for overlay in (0,1):
        for action in ('h','v'):
            cases.append(dict(base,SCROLL_OVERLAY=overlay,SCROLL_CONTENT_WIDTH=1000,SCROLL_CONTENT_HEIGHT=1000,SCROLL_X=350,SCROLL_Y=350,SCROLL_ACTION=action))
    cases.extend(dict(c,SCROLL_CANCEL=1) for c in list(cases) if 'SCROLL_ACTION' in c)
    cases.extend(dict(c,SCROLL_CANCEL=2) for c in list(cases) if c.get('SCROLL_ACTION') and not c.get('SCROLL_CANCEL'))
    for horizontal in (0,1):
        for scale in (1,2,4):
            cases.append(dict(base,SCROLL_OVERLAY=0,SCROLL_CONTENT_WIDTH=1000,SCROLL_CONTENT_HEIGHT=1000,SCROLL_X=350,SCROLL_Y=350,SCROLL_RASTER_H=horizontal,SCROLL_SCALE=scale))
    css='scrolledwindow { border:3px solid #223344; padding:2px; } scrollbar { opacity:0.5; background:#224466; } scrollbar slider { min-width:19px; min-height:23px; background:#6688aa; }'
    replacement='scrolledwindow { border:5px solid #445566; padding:3px; } scrollbar { opacity:0.75; background:#886644; } scrollbar slider { min-width:17px; min-height:27px; background:#aa8866; }'
    cases.append(dict(base,SCROLL_OVERLAY=0,SCROLL_CSS=css,SCROLL_CONTENT_WIDTH=1000,SCROLL_CONTENT_HEIGHT=1000,SCROLL_X=350,SCROLL_Y=350))
    cases.append(dict(cases[-1],SCROLL_CSS_CHANGE=replacement))
    idle_cases=[dict(base,SCROLL_ANIMATIONS=1,SCROLL_IDLE_MS=3000,SCROLL_Y=offset) for offset in (0,350)]
    backdrop_css='window, layout { background: #203050; } scrollbar { background: transparent; } scrollbar slider { border-radius: 7px; background: rgba(200,80,130,0.7); border: 1px solid rgba(50,100,200,0.4); box-shadow: inset 1px 1px rgba(255,255,255,0.3); }'
    backdrop_cases=[dict(base,SCROLL_OVERLAY=0,SCROLL_CONTENT_WIDTH=1000,SCROLL_X=350,SCROLL_Y=350,
        SCROLL_CSS=backdrop_css,SCROLL_BACKDROP_ALPHA=alpha,SCROLL_RASTER_H=horizontal,SCROLL_SCALE=scale)
        for horizontal in (0,1) for alpha,scale in ((0,1),(128,1),(255,1),(255,2))]
    cases=idle_cases if os.environ.get('KRYON_SCROLL_IDLE_ONLY') else backdrop_cases if os.environ.get('KRYON_SCROLL_BACKDROP_ONLY') else cases+idle_cases+backdrop_cases
    for form in ('source','saved'):
        executable=OUT/form/'gtk_scroll_linux_test'
        for width,height in ((300,200),(780,520),(960,1020)):
            case=dict(base,SCROLL_WHEEL=1,SCROLL_WIDTH=width,SCROLL_HEIGHT=height,SCROLL_CONTENT_WIDTH=5000,SCROLL_CONTENT_HEIGHT=5000)
            expected,_=oracle(case)
            env=dict(os.environ,**{key:str(value) for key,value in case.items()})
            result=run([executable],env=env,capture_output=True,text=True)
            observed={f'{p[1]}:{p[2]}':list(map(float,p[3:])) for line in result.stdout.splitlines() if (p:=line.split()) and p[0]=='WHEEL'}
            assert observed.keys()==expected.keys(),(form,observed,expected)
            for key in expected:
                assert all(abs(a-b)<.00001 for a,b in zip(observed[key],expected[key])),(form,width,height,key,observed[key],expected[key])
        print(form,'native wheel distance, immediate value, fractional accumulation and reverse direction passed',flush=True)
        for index,case in enumerate(cases):
            expected,rgba=oracle(case)
            target=OUT/(form+'.rgba')
            target.unlink(missing_ok=True)
            env=dict(os.environ,SCROLL_OUTPUT=str(target),**{key:str(value) for key,value in case.items()})
            result=run([executable],env=env,capture_output=True,text=True)
            observed={line.split()[0]:[float(item) for item in line.split()[1:]] for line in result.stdout.splitlines() if line.split() and line.split()[0] in ('VIEW','H','V','IMAGE','HOVER','PRESS','DRAG','OUTSIDE','RELEASE')}
            assert observed==expected,(form,index,case,observed,expected,result.stderr)
            actual=target.read_bytes() if target.exists() else b''
            if case.get('SCROLL_CSS') and 'SCROLL_BACKDROP_ALPHA' not in case:assert 0<max(rgba[3::4])<255,'native CSS opacity must be applied once'
            if actual!=rgba:
                (OUT/f'{form}-{index}-expected.rgba').write_bytes(rgba)
                (OUT/f'{form}-{index}-actual.rgba').write_bytes(actual)
                differences=[(i//4,tuple(actual[i:i+4]),tuple(rgba[i:i+4])) for i in range(0,min(len(actual),len(rgba)),4) if actual[i:i+4]!=rgba[i:i+4]]
                raise AssertionError((form,index,'native raster differs',observed,expected,len(actual),len(rgba),differences[:8]))
        for action in ('h','v'):
            env=dict(os.environ,SCROLL_ACTION=action,SCROLL_DESTROY='1',SCROLL_CONTENT_WIDTH='1000',SCROLL_CONTENT_HEIGHT='1000',SCROLL_X='350',SCROLL_Y='350')
            result=run([executable],env=env,capture_output=True,text=True)
            assert 'DESTROY 1' in result.stdout,(form,action,result.stdout,result.stderr)
        result=run([executable],env=dict(os.environ,SCROLL_LIFECYCLE='1'),capture_output=True,text=True)
        assert 'BOUNDS 1' in result.stdout,(form,result.stdout,result.stderr)
        print(form,len(cases),'native scrollbar allocations/range/raster cases passed',flush=True)
    return 0

def main():
    run(['git','check-ignore',OUT/'source'],stdout=subprocess.DEVNULL)
    OUT.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ)
    for name in ('DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','ICEAUTHORITY','GTK_THEME','GTK_PATH'):
        env.pop(name,None)
    for name,folder in (('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')):
        target=OUT/'private'/folder;target.mkdir(parents=True,exist_ok=True);target.chmod(0o700);env[name]=str(target)
    settings=Path(env['XDG_CONFIG_HOME'])/'gtk-3.0/settings.ini'
    settings.parent.mkdir(parents=True,exist_ok=True)
    settings.write_text('[Settings]'+chr(10)+'gtk-enable-animations=false'+chr(10))
    env.update(YUE_DESKTOP_RECOVERY='0',LC_ALL='C.UTF-8',NO_AT_BRIDGE='1',GTK_USE_PORTAL='0',G_DEBUG='fatal-criticals',GDK_BACKEND='x11')
    env['LDLIBS']=subprocess.check_output(['pkg-config','--libs','gtk+-3.0'],text=True).strip()
    compiler=env.get('ZIRAN_BIN') or shutil.which('ziran')
    source=ROOT/'tests/gtk_scroll_linux_test.zi'
    common=['--module-path','src/backend','--entry','gtk_scroll_linux_test:main']
    executable=OUT/'source/gtk_scroll_linux_test'
    inputs=[source,ROOT/'src/backend/gtk_scroll_linux.zi',ROOT/'src/backend/gtk_style_linux.zi']
    if not executable.exists() or executable.stat().st_mtime<max(path.stat().st_mtime for path in inputs):
        run(['build-queue','--',compiler,'build','--root','tests',*common,'--target=c','--exe','-o',OUT/'source',source],env=env)
    run(['build-queue','--',compiler,'ir','--root','tests',*common,'-o',OUT/'ir',source],env=env)
    run(['build-queue','--',compiler,'build','--root',OUT/'ir',*common,'--target=c','--exe','-o',OUT/'saved',OUT/'ir/gtk_scroll_linux_test.zir'],env=env)
    return run(['xvfb-run','-a','-s','-screen 0 1280x1100x24 -nolisten tcp','/usr/bin/python3',__file__,'--worker'],env=env).returncode

if __name__=='__main__':
    raise SystemExit(worker() if '--worker' in sys.argv else main())
