"""Native profile artifact linkage through the real shared descriptor consumer."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from toolchain import ZIRAN_ROOT

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import link_artifacts


class ProjectArtifactTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.scratch=ROOT/'work/project-artifact-link-test'
        subprocess.run(['git','check-ignore',str(cls.scratch)],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
        cls.scratch.mkdir(parents=True,exist_ok=True)

    def setUp(self):
        temporary=tempfile.TemporaryDirectory(dir=self.scratch);self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)
        (self.root/'value.c').write_text('int ArtifactValue(void) { return 42; }\n')
        (self.root/'main.c').write_text('int ArtifactValue(void); int main(void) { return ArtifactValue()!=42; }\n')
        subprocess.run(['cc','-c',str(self.root/'value.c'),'-o',str(self.root/'value.o')],check=True)
        subprocess.run(['ar','rcs',str(self.root/'support.a'),str(self.root/'value.o')],check=True)
        (self.root/'link.txt').write_text('archive\tsupport.a\nlibrary\t-lm\n')
        self.args=SimpleNamespace(toolchain=ZIRAN_ROOT,project=self.root,
            platform='linux-x86_64-gnu',artifacts='support:native',command=['--','cc',
            str(self.root/'main.c'),link_artifacts.MARKER,'-o',str(self.root/'consumer')])
        self.descriptor=self.root/'artifact.tsv'
        self.save(['support.a','link.txt'])

    def save(self,outputs,artifact='native'):
        rows=dict(schema='1',package='support',artifact=artifact,platform=self.args.platform,key='a'*64)
        rows.update({name:hashlib.sha256((self.root/name).read_bytes()).hexdigest() for name in outputs})
        self.descriptor.write_text(''.join(f'{key}\t{value}\n' for key,value in rows.items()))

    def test_real_native_link_and_locked_request(self):
        with patch.object(link_artifacts.subprocess,'check_output',return_value=str(self.descriptor)+'\n') as request:
            self.assertEqual(link_artifacts.link(self.args),0)
        subprocess.run([str(self.root/'consumer')],check=True)
        self.assertEqual(request.call_args.args[0],[str(ZIRAN_ROOT/'build/bin/ziran'),'pkg','artifact',
            'support:native','--platform',self.args.platform,'--locked'])
        self.assertEqual(request.call_args.kwargs['env']['CC'],'cc')

    def test_renderer_metadata_uses_same_verified_outputs(self):
        (self.root/'font.ttf').write_bytes(b'font');(self.root/'license').write_bytes(b'license')
        meta=dict(schema=1,artifact='renderer-raylib',platform=self.args.platform,
            archives=['support.a'],compile_flags=[],link_flags=['-lm'],defines=[],include_directories=[],
            resources=dict(font='font.ttf',font_license='license'))
        (self.root/'renderer.json').write_text(json.dumps(meta))
        self.save(['support.a','font.ttf','license','renderer.json'],artifact='renderer-raylib')
        self.args.artifacts='kryon:renderer-raylib'
        with patch.object(link_artifacts.subprocess,'check_output',return_value=str(self.descriptor)):
            self.assertEqual(link_artifacts.link(self.args),0)
        subprocess.run([str(self.root/'consumer')],check=True)

    def test_corrupt_output_never_invokes_compiler(self):
        (self.root/'support.a').write_bytes(b'corrupt')
        with patch.object(link_artifacts.subprocess,'check_output',return_value=str(self.descriptor)),\
             patch.object(link_artifacts.subprocess,'run') as compiler:
            with self.assertRaisesRegex(ValueError,'digest'):
                link_artifacts.link(self.args)
            compiler.assert_not_called()

    def test_library_marker_retains_order_and_failure_status(self):
        self.args.command=['--','cc','object.o',link_artifacts.MARKER,'-lafter','-o','result']
        with patch.object(link_artifacts.subprocess,'check_output',return_value=str(self.descriptor)),\
             patch.object(link_artifacts.subprocess,'run',return_value=SimpleNamespace(returncode=7)) as compiler:
            self.assertEqual(link_artifacts.link(self.args),7)
        self.assertEqual(compiler.call_args.args[0],['cc','object.o',str(self.root/'support.a'),'-lm','-lafter','-o','result'])

    def test_several_selections_keep_provider_order(self):
        extra=self.root/'extra';extra.mkdir()
        (extra/'support.a').write_bytes((self.root/'support.a').read_bytes())
        (extra/'link.txt').write_text('archive\tsupport.a\nflag\t-pthread\n')
        rows=dict(schema='1',package='support',artifact='extra',platform=self.args.platform,key='b'*64)
        rows.update({name:hashlib.sha256((extra/name).read_bytes()).hexdigest() for name in ('support.a','link.txt')})
        second=extra/'artifact.tsv';second.write_text(''.join(f'{key}\t{value}\n' for key,value in rows.items()))
        self.args.artifacts='support:native support:extra'
        with patch.object(link_artifacts.subprocess,'check_output',side_effect=[str(self.descriptor),str(second)]) as request,\
             patch.object(link_artifacts.subprocess,'run',return_value=SimpleNamespace(returncode=0)) as compiler:
            self.assertEqual(link_artifacts.link(self.args),0)
        self.assertEqual([call.args[0][3] for call in request.call_args_list],['support:native','support:extra'])
        arguments=compiler.call_args.args[0]
        at=arguments.index(str(self.root/'support.a'))
        self.assertEqual(arguments[at:at+4],[str(self.root/'support.a'),'-lm',str(extra/'support.a'),'-pthread'])

    def test_unsafe_selections_and_missing_marker_fail_before_request(self):
        for selections in ('support:native;touch','support:$(touch)','support:native  other:native','support:'):
            self.args.artifacts=selections
            with patch.object(link_artifacts.subprocess,'check_output') as request:
                with self.assertRaises(ValueError): link_artifacts.link(self.args)
                request.assert_not_called()
        self.args.artifacts='support:native';self.args.command=['cc','file.c']
        with self.assertRaisesRegex(ValueError,'marker'): link_artifacts.link(self.args)


if __name__=='__main__':unittest.main()
