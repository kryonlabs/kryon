#!/usr/bin/env python3
"""Native catalog interaction audit. Run under xvfb-run; requires xdotool and
ImageMagick's import. Generated C is used only to observe the .kry app's state.
Screenshots and JSON results are written under build/widget-catalog-audit.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess as sp
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/widget-catalog-audit"
STATE = OUT / "state.json"
window = None
results = []
group = ""

HARNESS = r'''
#include <string.h>
static int href_activated;
int __wrap_OpenURI(const char *uri) { href_activated = strcmp(uri, "https://kryonlabs.com") == 0; return 1; }
#include "@ROOT@/build/examples/codegen/26_widget_catalog/26_widget_catalog.c"
static void AuditFrame(Rectangle viewport) {
    WidgetCatalogExample(viewport);
    FILE *out = fopen("@STATE@.tmp", "w");
    if(out) {
        fprintf(out,"{\"clicks\":%d,\"category\":%d,\"scroll\":%d,\"picked\":%d,\"slider\":%d,\"toggle\":%d,\"checkbox\":%d,\"selected\":%d,\"flags\":%d,\"dialog\":%d,\"open\":%d,\"tree\":%d,\"row\":%d,\"multi\":%d,\"same_text\":%d,\"accepted\":%d,\"number\":%.6f,\"integer\":%d,\"double\":%.6f,\"rmin\":%.6f,\"imin\":%d,\"angle\":%.6f,\"red\":%.6f,\"alpha\":%.6f,\"text_test\":%d,\"area_test\":%d,",action_count,category,scroll_off,picked,slider_value,toggle_value,checkbox_value,selected,flags,dialog,open,tree_selected,table_selected_row,multi_count,strcmp(text_value,area_value)==0,accepted_size,numbers[0],integers[0],doubles[0],range_min,int_min,angle,rgba[0],rgba[3],strstr(text_value,"TEST")!=NULL,strstr(area_value,"TEST")!=NULL);
        fprintf(out,"\"tabs\":%d,\"tab_selected\":%d,\"subtab\":%d,\"pane\":%d,\"command\":%d,\"split\":%d,\"dark\":%d,\"theme\":%d,\"zoom\":%.3f,\"pan_x\":%d,\"pan_y\":%d,\"cascade_count\":%d,\"cascade_selected\":%d,\"preview\":%d,\"route\":%d,\"picker\":%d,",tab_count,tab_selected,subtab_selected,pane_selected,menu_command,split,dark_mode,theme_id,canvas_zoom,canvas_scroll_x,canvas_scroll_y,cascade_count,cascade_selected,page_preview,page_route,picker_choice);
        fprintf(out,"\"fits\":%d,\"scale\":%.4f,\"href\":%d,\"effective_dark\":%d,\"copied\":%d,\"mode\":%d,\"style\":%d,\"dialog_result\":%d,\"n1\":%.4f,\"n2\":%.4f,\"i1\":%d,\"i2\":%d,\"focus\":%d,\"popover_test\":%d}",Scale(1060)<=viewport.width && Scale(546)<=viewport.height-Scale(52),GetUIScale(),href_activated,GetEffectiveThemeDarkMode(),GetClipboardText()!=NULL && strstr(GetClipboardText(),"Select")!=NULL,theme_mode,theme_style,dialog_result,numbers[1],numbers[2],integers[1],integers[2],GetUIFocus(),strstr(text_value,"POPOVER_TEST")!=NULL);
        fclose(out); rename("@STATE@.tmp","@STATE@");
    }
}
#define WidgetCatalogExample AuditFrame
#include "@ROOT@/build/examples/codegen/26_widget_catalog/kryon_project.c"
'''

def prepare(build):
    OUT.mkdir(parents=True, exist_ok=True)
    if build:
        sp.run(["make", "-j4", "build/linux-x86_64/libkryon.a"], cwd=ROOT, check=True)
        sp.run(["make", "-C", "examples", "-j4", "26_widget_catalog"], cwd=ROOT, check=True)
    dry = sp.check_output(["make", "-n", "-C", "examples", "26_widget_catalog"], cwd=ROOT, text=True).replace("\\\n", " ")
    command = next(line for line in dry.splitlines() if line.startswith("cc ") and "-o ../build/examples/bin/26_widget_catalog" in line)
    args = shlex.split(command)
    args[args.index("-o") + 1] = str(OUT / "audit-app")
    generated = "../build/examples/codegen/26_widget_catalog/"
    args = [v for v in args if v not in (generated + "26_widget_catalog.c", generated + "kryon_project.c")]
    harness = OUT / "state_observer.c"
    harness.write_text(HARNESS.replace("@ROOT@", str(ROOT)).replace("@STATE@", str(STATE)))
    args.insert(args.index("-L../build/linux-x86_64"), str(harness))
    args.append("-Wl,--wrap=OpenURI")
    sp.run(args, cwd=ROOT / "examples", check=True)

def xd(*args):
    sp.run(["xdotool", *map(str,args)], check=True, stdout=sp.DEVNULL)

def state():
    time.sleep(.16)
    return json.loads(STATE.read_text())

def click(x,y,button=1):
    xd("mousemove", "--window", window, x,y)
    xd("mousedown",button); time.sleep(.10)
    xd("mouseup",button); time.sleep(.18)

def drag(x,y,xx,yy):
    xd("mousemove","--window",window,x,y); xd("mousedown",1); time.sleep(.12)
    for i in range(1,7):
        xd("mousemove","--window",window,round(x+(xx-x)*i/6),round(y+(yy-y)*i/6)); time.sleep(.045)
    xd("mouseup",1); time.sleep(.16)

def key_chord(chord):
    keys = chord.split("+")
    for key in keys:
        xd("keydown",key)
    time.sleep(.10)
    for key in reversed(keys):
        xd("keyup",key)
    time.sleep(.10)

def category(n):
    for attempt in range(3):
        click(100,105+n*46)
        if state()["category"] == n: return
    raise AssertionError((n,state()))

def check(name,key,action,expected=None):
    before=state(); action(); after=state()
    passed=after[key]!=before[key] if expected is None else after[key]==expected
    results.append(dict(widget=name,passed=passed,key=key,before=before[key],after=after[key]))

def capture(name):
    sp.run(["import","-window",window,str(OUT/(group+"-"+name+".png"))],check=True,timeout=10)

def run_screens():
    for n in range(10):
     category(n);capture(f'{n}-top')
     before=state()['scroll'];xd('mousemove','--window',window,1100,620);xd('click','--repeat',18,'--delay',25,5)
     after=state()['scroll'];capture(f'{n}-bottom')
     results.append(dict(widget=f'Category {n} navigation and scrolling',passed=state()['category']==n and (after>before if n in (0,1,7) else True),scroll=after))

def run_actions():
    category(1)
    check('Checkbox','checkbox',lambda:click(263,422))
    check('Toggle','toggle',lambda:click(350,381))
    check('Slider','slider',lambda:drag(358,350,460,350))
    check('Radio','picked',lambda:click(674,287),1)
    check('Spinbox','slider',lambda:click(790,331))
    def selectable():
     before=state()['selected']
     for _ in range(3):
      click(300,538)
      if state()['selected']!=before: break
    check('Selectable','selected',selectable)
    check('Checkbox','flags',lambda:click(674,492))
    def text():
     click(730,137);xd('key','ctrl+a');xd('type','--clearmodifiers','--delay',80,'TEST')
    check('TextField','text_test',text,1)
    def area():
     click(730,195);xd('key','ctrl+a');xd('type','--clearmodifiers','--delay',80,'AREA_TEST')
    check('TextArea','area_test',area,1)
    def dropdown(): click(320,287);capture('dropdown-open');click(300,315)
    check('Dropdown','picked',dropdown)
    def rich_dropdown(): click(730,381);capture('dropdown-rich-open');click(710,443)
    check('Dropdown rich','picked',rich_dropdown)
    category(2)
    check('Layout TabBar','picked',lambda:click(805,137))
    check('Collapsible','open',lambda:click(750,205))
    category(3)
    check('ListBox','picked',lambda:click(290,160),1)
    check('TreeView','tree',lambda:click(510,160),2)
    check('TableView','row',lambda:click(310,302),1)
    check('MultiSelectList','multi',lambda:click(300,455))
    check('DragDropSource / DragDropTarget','same_text',lambda:drag(330,628,770,628))

def run_buttons():
    category(1)
    for name,x,y in [('Button',320,137),('Icon Button',421,137),('Small Button',310,490),('Arrow button',408,490),('InvisibleButton',760,538)]:
     check(name,'clicks',lambda x=x,y=y:click(x,y))
    before=state()['clicks'];check('Disabled button','clicks',lambda:click(320,599),before)
    category(2)
    for name,x,y in [('Row A',297,167),('Row B',375,167),('Grid A',325,246),('Grid B',510,246)]:
     check(name,'clicks',lambda x=x,y=y:click(x,y))
    category(0);check('Image Button','clicks',lambda:click(808,620))
    category(8);check('Color swatch','clicks',lambda:click(325,550))
    category(5);click(1000,192);check('Modal open','dialog',lambda:None,4);capture('Modal');click(705,447)
    results.append(dict(widget='Modal close',passed=state()['dialog']==0))

def run_dialogs():
    category(4)
    check('TabBar','picked',lambda:click(320,195),0)
    click(277,246);capture('menubar-open');click(300,278)
    click(790,240,3);capture('context-open');xd('key','Escape')
    category(5)
    for name,x,value in [('Modal message',490,1),('Modal confirm',735,2),('Prompt modal',900,3)]:
     check(name,'dialog',lambda x=x:click(x,139),value);capture(name);key_chord('Escape');time.sleep(.2)
     if state()['dialog']!=0:
      click(710,447)
      time.sleep(.3)
     if state()['dialog']!=0:
      capture('dialog-needs-close')
      raise AssertionError(('dialog did not close',state()))

def run_numbers():
    category(7)
    for name,key,act in [
     ('Drag float','number',lambda:drag(295,187,320,187)),('Drag int','integer',lambda:drag(710,187,716,187)),
     ('Drag range float','rmin',lambda:drag(300,299,312,299)),('Drag range int','imin',lambda:drag(710,299,714,299)),
     ('Slider float','number',lambda:click(340,410)),('Slider int','integer',lambda:click(680,410)),
     ('Slider angle','angle',lambda:click(520,522)),('Input float step','number',lambda:click(766,522)),
     ('Input int step','integer',lambda:click(356,635)),('Input double step','double',lambda:click(820,635))]:
     check(name,key,act)
    # Scroll exposes the vertical sliders, with their positions shifted by max_scroll.
    xd('mousemove','--window',window,1100,620);xd('click','--repeat',18,'--delay',25,5);off=state()['scroll']
    check('VSlider float','number',lambda:click(275,82+666-off+25))
    check('VSlider int','integer',lambda:click(350,82+666-off+25))
    category(8)
    check('ColorPicker RGB edit','red',lambda:click(345,190))
    check('ColorPicker RGBA edit alpha','alpha',lambda:click(949,190))
    check('ColorPicker RGB','red',lambda:click(290,310))
    check('ColorPicker RGBA alpha','alpha',lambda:click(720,425))
    capture('colors-edited')

def run_extended():
    category(4)
    check('New tab','tabs',lambda:click(720,410),3)
    check('Close tab','tabs',lambda:click(484,352),2)
    check('Level 2 TabBar','subtab',lambda:click(420,490),1)
    check('Pane TabBar','pane',lambda:click(880,490),1)
    click(277,246);click(300,278);check('Menu command','command',lambda:None,1)
    category(2);check('PanedView drag','split',lambda:drag(950,150,925,150),65)
    category(6)
    check('Canvas zoom','zoom',lambda:(xd('mousemove','--window',window,400,180),xd('click',4)))
    check('Canvas pan','pan_x',lambda:drag(400,180,470,200))
    capture('canvas-interactive')
    category(9);capture('pages-settings')
    check('Page preview','preview',lambda:click(400,140),1);capture('page-preview')
    check('NavigationBar route','route',lambda:click(600,765),1)
    check('Return to catalog','preview',lambda:click(100,90),0)

def run_new_dialogs():
    category(4);capture('navigation-new')
    category(5);capture('feedback-new');click(490,139);capture('modal-layered');click(600,447)
    click(330,530);capture('picker-new');click(550,403)
    check('Picker selection','picker',lambda:None,1)
    click(530,530);capture('modal-frame-new');click(510,410);xd('key','Escape');time.sleep(.3)
    click(730,530);capture('popover-new');xd('key','Escape')
    category(3);xd('mousemove','--window',window,1100,620);xd('click','--repeat',9,'--delay',25,5);capture('cascade-new')

def run_remaining():
    category(5)
    # Exercise the toggle from dark regardless of the host's initial theme.
    if not state()['dark']:
        click(385,189)
    check('Theme light','dark',lambda:click(385,189),0)
    check('Theme light rendered','effective_dark',lambda:None,0)
    check('Theme palette','theme',lambda:click(790,189))
    capture('theme-light')
    click(900,139);click(520,410);key_chord('ctrl+a');xd('type','--clearmodifiers','--delay',60,'PROMPT_TEST');capture('prompt-typed');click(710,447)
    check('Prompt submit','dialog',lambda:None,0);check('Prompt value','text_test',lambda:None,1)
    click(730,530);check('Popover opens','dialog',lambda:None,7);capture('popover-new')
    for _ in range(3):
     click(760,587)
     if state()['focus']==971: break
    key_chord('ctrl+a');xd('type','--clearmodifiers','--delay',60,'POPOVER_TEST')
    for _ in range(3):
     key_chord('Return');time.sleep(.3)
     if state()['dialog']==0: break
    check('Popover submit','dialog',lambda:None,0);check('Popover value','popover_test',lambda:None,1)
    category(3);xd('mousemove','--window',window,1100,620);xd('click','--repeat',9,'--delay',25,5)
    check('Cascading expand','cascade_count',lambda:click(285,495),2)
    check('Cascading select','cascade_selected',lambda:click(350,525),12)
    category(0);drag(253,600,410,600);capture('selection-drag');xd('keydown','ctrl');xd('keydown','c');time.sleep(.15);xd('keyup','c');xd('keyup','ctrl');check('Selectable text copy','copied',lambda:None,1)
    category(1);xd('mousemove','--window',window,320,599);time.sleep(1);capture('tooltip-hover')

def run_vectors():
    category(7)
    for component, fk, ik in [(0,"number","integer"),(1,"n1","i1"),(2,"n2","i2")]:
        left = 295 + component * 350 / 3
        right = 710 + component * 350 / 3
        check(f"Drag float component {component}",fk,lambda:drag(left,187,left+15,187))
        check(f"Drag int component {component}",ik,lambda:drag(right,187,right-2,187))
        check(f"Slider float component {component}",fk,lambda:click(left-20,410))
        check(f"Slider int component {component}",ik,lambda:click(right-20,410))
    for component, key in [(0,"number"),(1,"n1"),(2,"n2")]:
        click(680+component*350/3,522)
        key_chord("ctrl+a"); xd("type","--clearmodifiers","--delay",60,"0.625"); key_chord("Return")
        check(f"Input float keyboard component {component}",key,lambda:None,.625)
    category(3)
    click(300,435)
    xd("keydown","ctrl"); click(300,495); xd("keyup","ctrl")
    check("Ctrl multiselection","multi",lambda:None,2)
    click(300,435)
    xd("keydown","shift"); click(300,495); xd("keyup","shift")
    check("Shift range selection","multi",lambda:None,3)
    category(1)
    check("Link dispatches expected URL","href",lambda:click(500,137),1)

def run_tab_limits():
    category(4)
    check("Close first tab","tabs",lambda:click(612,352),1)
    check("Close last tab","tabs",lambda:click(992,352),0)

    def add_tab(name, expected):
        before=state()["tabs"]
        after=before
        # Xvfb can occasionally drop an injected click while llvmpipe is busy.
        # Retry only while the button had no effect; never click again after the
        # count changes, so a real double-add or capacity error still fails.
        for _ in range(3):
            click(730,410)
            after=state()["tabs"]
            if after != before:
                break
        results.append(dict(widget=name, passed=after == expected, key="tabs",
                            before=before, after=after))

    add_tab("Reopen empty tab bar",1)
    for count in range(2,9):
        add_tab(f"Add tab {count}",count)
    before=state()["tabs"]
    for _ in range(3):
        click(730,410)
    after=state()["tabs"]
    results.append(dict(widget="Tab capacity disables add",
                        passed=before == 8 and after == 8, key="tabs",
                        before=before, after=after))
    capture("full-tab-bar")

def run_settings():
    category(9)
    capture("page-content")
    click(800,243); capture("palette-menu")
    click(800,300)
    capture("app-theme-settings")
    click(800,243); click(800,300)
    check("Theme controls light mode","mode",lambda:None,1)
    capture("settings-light")
    click(800,393); capture("style-menu")
    click(800,450)
    check("Theme controls style","style",lambda:None,1)

def run_sizes():
    for width,height in [(800,600),(1120,640),(1600,1000)]:
        xd("windowsize",window,width,height)
        time.sleep(.5)
        scale=state()["scale"]
        click(round(100*scale),round(519*scale))
        check(f"Sidebar navigation at {width}x{height}","category",lambda:None,9)
        check(f"Content fits at {width}x{height}","fits",lambda:None,1)
        capture(f"{width}x{height}")

GROUPS = {
    "sizes": run_sizes,
    "settings": run_settings,
    "vectors": run_vectors,
    "tab-limits": run_tab_limits,
    'screens': run_screens,
    'actions': run_actions,
    'buttons': run_buttons,
    'dialogs': run_dialogs,
    'numbers': run_numbers,
    'extended': run_extended,
    'new-dialogs': run_new_dialogs,
    'remaining': run_remaining,
}

def main():
    global window, results, group
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--group", choices=["all",*GROUPS], default="all")
    parser.add_argument("--no-build", action="store_true", help="Use existing generated sources and libraries")
    args=parser.parse_args()
    prepare(not args.no_build)
    failures=0
    for group in GROUPS if args.group == "all" else [args.group]:
        results=[]
        with (OUT/(group+".log")).open("w") as log:
            app=sp.Popen([str(OUT/"audit-app")],cwd=ROOT,stdout=log,stderr=sp.STDOUT)
            try:
                time.sleep(2)
                if app.poll() is not None: raise RuntimeError("Native catalog failed to start")
                window=sp.check_output(["xdotool","search","--name","Kryon Widget Catalog"],text=True).splitlines()[-1]
                GROUPS[group]()
            except Exception as error:
                results.append(dict(widget="audit interrupted",passed=False,error=str(error)))
            finally:
                app.terminate()
                try: app.wait(timeout=5)
                except sp.TimeoutExpired:
                    app.kill(); app.wait()
        (OUT/(group+"-results.json")).write_text(json.dumps(results,indent=2)+"\n")
        failed=sum(not row["passed"] for row in results)
        failures+=failed
        print(f"{group}: {len(results)-failed}/{len(results)} passed",flush=True)
    return 1 if failures else 0

if __name__ == "__main__":
    raise SystemExit(main())
