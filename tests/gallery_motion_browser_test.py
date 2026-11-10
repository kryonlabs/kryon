"""Exercise maintained motion examples in the real website portable preview."""
import json
import os
from pathlib import Path
import shutil
import time
from PIL import ImageChops, ImageStat
from site_browser_test import Browser, SITE, serve, select_example, mouse, node_for, compile_ready, frame_crop

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/scratch/gallery-motion-browser'
EVIDENCE = Path(os.environ.get('KRYON_MOTION_EVIDENCE', ROOT / 'backup/cui-motion/visual-feedback'))

def difference(a,b):
    return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)

def run():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    scenes = json.JSONDecoder().raw_decode((SITE/'gallery-data.js').read_text().split('window.galleryScenes = ',1)[1])[0]
    server = serve(SITE)
    chromium = os.environ.get('CHROMIUM') or shutil.which('chromium') or shutil.which('chromium-browser') or shutil.which('google-chrome')
    assert chromium, 'Chromium or Google Chrome is required for the motion test'
    browser = Browser(OUTPUT/'profile', chromium)
    results = {}
    try:
        base = 'http://127.0.0.1:'+str(server.server_port)+'/'
        for name,target in [('TabBar','Overview'),('NavigationBar','Library'),('Collapsible','More details'),('Menu',None)]:
            selected = os.environ.get('KRYON_MOTION_SCENES','').split(',')
            if selected != [''] and name not in selected: continue
            select_example(browser,base,scenes,name)
            mouse(browser,node_for(browser,label='Source'))
            compile_ready(browser)
            print('Actual portable preview compiled:',name,flush=True)
            nodes = browser.evaluate('Module.previewTree')
            scene_index = next(i for i,s in enumerate(scenes) if s['name']==name)
            (OUTPUT/(name+'-tree.json')).write_text(json.dumps(nodes))
            if name == 'TabBar':
                print('Preview target nodes:',[(n['key'],n['label'],n['x'],n['y'],n['width'],n['height']) for n in nodes if n['interactive']],flush=True)
            # Continue with real trusted pointer input after inspecting preview geometry.
            heading = next(n for n in nodes if n['key']==9001)
            assert heading['label'] == name, (name,'Wrong maintained source',heading)
            viewport = dict(x=heading['x']-24,y=heading['y']-12,width=heading['width']+48,height=320)
            area = dict(x=viewport['x']+24,y=viewport['y']+64,width=viewport['width']-48,height=188)
            node = next((n for n in nodes if n['label']==target and n['interactive']),None)
            if target is None:
                node = dict(x=area['x']+350,y=area['y']+150,width=10,height=10)
            assert node is not None,(name,[(n['label'],n['interactive']) for n in nodes])
            assert node['x'] >= area['x'] and node['y'] >= area['y'] and node['x']+node['width'] <= area['x']+area['width'] and node['y']+node['height'] <= area['y']+area['height'], (name,'Target outside maintained preview',node,area)
            mouse(browser,node,click=False)
            time.sleep(.1)
            start = frame_crop(browser,area)
            mouse(browser,node)
            middle = [frame_crop(browser,area) for _ in range(5)]
            time.sleep(1.5)
            assert browser.evaluate('Module.gallerySceneRendered') == scene_index, (name,'Input escaped preview')
            end = frame_crop(browser,area)
            time.sleep(.15)
            rest = frame_crop(browser,area)
            changed = [frame for frame in middle if difference(frame,start)>.02 and difference(frame,end)>.02]
            start.save(EVIDENCE/(name+'-portable-start.webp'),'WEBP',lossless=True)
            (changed[0] if changed else middle[0]).save(EVIDENCE/(name+'-portable-mid.webp'),'WEBP',lossless=True)
            end.save(EVIDENCE/(name+'-portable-end.webp'),'WEBP',lossless=True)
            assert changed,(name,'No actual portable intermediate pixels')
            assert difference(end,rest)<.02,(name,'Portable motion did not settle')
            committed = browser.evaluate('Module.previewTree')
            if name in ('TabBar','NavigationBar'):
                assert any(n['label']==target and n['selected'] for n in committed),(name,'Expected guest selection absent')
            elif name == 'Collapsible':
                assert not any(n['key']==1406 for n in committed),(name,'Guest content did not close')
            else:
                assert not any(n['label']=='New note' for n in committed),(name,'Guest menu did not close')

            results[name]={'intermediate_images':len(changed),'rest_difference':difference(end,rest)}
            print('Actual portable motion:',name,results[name],flush=True)
            browser.reduced_motion(True)
            browser.until('Module.siteReducedMotion', 'actual reduced-motion media')
            frame = browser.evaluate('Module.siteFrame')
            browser.until('Module.siteFrame > '+str(frame+3), 'guest reduced-motion publication')
            reduced_target = {'TabBar':'Files','NavigationBar':'Settings','Collapsible':'More details','Menu':'File'}[name]
            node = browser.evaluate('Module.previewTree.find(n => n.interactive && n.label === '+json.dumps(reduced_target)+')')
            assert node is not None,(name,reduced_target)
            mouse(browser,node,click=False)
            time.sleep(.1)
            before_reduced = frame_crop(browser,area)
            mouse(browser,node)
            snapped = frame_crop(browser,area)
            time.sleep(.7)
            assert browser.evaluate('Module.gallerySceneRendered') == scene_index, (name,'Reduced input escaped preview')
            later = frame_crop(browser,area)
            assert difference(before_reduced,snapped)>.02,(name,'Reduced action did not fire')
            assert difference(snapped,later)<.02,(name,'Reduced motion continued after target')
            committed = browser.evaluate('Module.previewTree')
            if name in ('TabBar','NavigationBar'):
                assert any(n['label']==reduced_target and n['selected'] for n in committed),(name,'Reduced guest selection absent')
            elif name == 'Collapsible':
                assert any(n['key']==1406 for n in committed),(name,'Reduced guest content did not open')
            else:
                assert any(n['label']=='New note' and n['interactive'] for n in committed),(name,'Reduced guest menu did not open')

            snapped.save(EVIDENCE/(name+'-portable-reduced.webp'),'WEBP',lossless=True)
            results[name]['reduced_difference'] = difference(snapped,later)
            print('Actual portable reduced motion:',name,results[name]['reduced_difference'],flush=True)
            browser.reduced_motion(False)

        (EVIDENCE/'portable-motion-results.json').write_text(json.dumps(results,indent=2)+chr(10))
    finally:
        browser.close()
        server.shutdown()
        server.server_close()

if __name__=='__main__': run()
