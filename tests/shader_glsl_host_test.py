"""Compile ES100 materials and compare real GLES pixels with ShaderRun."""
import ctypes as C
import json
import os
from pathlib import Path
import select
import subprocess
import sys


def bind(lib,name,result,*args):
    function=getattr(lib,name);function.restype=result;function.argtypes=list(args);return function


class GLES:
    def __init__(self):
        self.egl=C.CDLL('libEGL.so.1');self.gl=C.CDLL('libGLESv2.so.2')
        p=C.c_void_p;i=C.c_int;u=C.c_uint;f=C.c_float
        self.display=bind(self.egl,'eglGetDisplay',p,p)(None)
        major,minor=i(),i()
        assert bind(self.egl,'eglInitialize',u,p,C.POINTER(i),C.POINTER(i))(self.display,C.byref(major),C.byref(minor))
        assert bind(self.egl,'eglBindAPI',u,u)(0x30a0)
        config=p();count=i();attributes=(i*13)(0x3033,1,0x3040,4,0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3038)
        assert bind(self.egl,'eglChooseConfig',u,p,C.POINTER(i),C.POINTER(p),i,C.POINTER(i))(self.display,attributes,C.byref(config),1,C.byref(count)) and count.value
        self.surface=bind(self.egl,'eglCreatePbufferSurface',p,p,p,C.POINTER(i))(self.display,config,(i*5)(0x3057,4,0x3056,4,0x3038))
        self.context=bind(self.egl,'eglCreateContext',p,p,p,p,C.POINTER(i))(self.display,config,None,(i*3)(0x3098,2,0x3038))
        assert self.context and self.surface
        assert bind(self.egl,'eglMakeCurrent',u,p,p,p,p)(self.display,self.surface,self.surface,self.context)
        signatures={
            'glCreateShader':(u,u),'glShaderSource':(None,u,i,C.POINTER(C.c_char_p),C.POINTER(i)),
            'glCompileShader':(None,u),'glGetShaderiv':(None,u,u,C.POINTER(i)),
            'glGetShaderInfoLog':(None,u,i,C.POINTER(i),C.c_void_p),
            'glCreateProgram':(u,),'glAttachShader':(None,u,u),'glLinkProgram':(None,u),
            'glGetProgramiv':(None,u,u,C.POINTER(i)),'glGetProgramInfoLog':(None,u,i,C.POINTER(i),C.c_void_p),
            'glUseProgram':(None,u),'glGetUniformLocation':(i,u,C.c_char_p),
            'glUniform4f':(None,i,f,f,f,f),'glUniform2f':(None,i,f,f),'glUniform1f':(None,i,f),'glUniform1i':(None,i,i),
            'glGetAttribLocation':(i,u,C.c_char_p),'glEnableVertexAttribArray':(None,u),
            'glVertexAttribPointer':(None,u,i,u,u,i,C.c_void_p),'glDrawArrays':(None,u,i,i),
            'glViewport':(None,i,i,i,i),'glDisable':(None,u),'glClearColor':(None,f,f,f,f),'glClear':(None,u),
            'glReadPixels':(None,i,i,i,i,u,u,C.c_void_p),'glGenTextures':(None,i,C.POINTER(u)),
            'glBindTexture':(None,u,u),'glTexParameteri':(None,u,u,i),'glActiveTexture':(None,u),
            'glTexImage2D':(None,u,i,i,i,i,i,u,u,C.c_void_p),'glDeleteTextures':(None,i,C.POINTER(u)),
            'glDeleteShader':(None,u),'glDeleteProgram':(None,u),'glGetError':(u,),
            'glGetString':(C.c_char_p,u),'glGetShaderPrecisionFormat':(None,u,u,C.POINTER(i),C.POINTER(i))}
        for name,(result,*args) in signatures.items():setattr(self,name,bind(self.gl,name,result,*args))
        precision=i();ranges=(i*2)();self.glGetShaderPrecisionFormat(0x8b30,0x8df2,ranges,C.byref(precision))
        assert precision.value>=23 and min(ranges)>=127,('float32 fragment highp unavailable',list(ranges),precision.value)
        self.identity=dict(renderer=self.glGetString(0x1f01).decode(),version=self.glGetString(0x1f02).decode(),precision_bits=precision.value)

    def shader(self,kind,source):
        shader=self.glCreateShader(kind);text=C.c_char_p(source.encode())
        self.glShaderSource(shader,1,C.byref(text),None);self.glCompileShader(shader)
        status=C.c_int();self.glGetShaderiv(shader,0x8b81,C.byref(status))
        if not status.value:
            log=C.create_string_buffer(4096);self.glGetShaderInfoLog(shader,len(log),None,log)
            raise AssertionError(log.value.decode()+'\n'+source)
        return shader

    def run(self,case):
        vertex=self.shader(0x8b31,'#version 100\nattribute vec2 aPosition;void main(){gl_Position=vec4(aPosition,0.0,1.0);}')
        fragment=self.shader(0x8b30,'#version 100\nprecision highp float;\n'+case['source']+'\nuniform vec4 oracleColor,oraclePosition,oracleNormal,oracleUV;\nvoid main(){gl_FragColor=kryonMaterial(oracleColor,oraclePosition,oracleNormal,oracleUV);}')
        program=self.glCreateProgram();self.glAttachShader(program,vertex);self.glAttachShader(program,fragment);self.glLinkProgram(program)
        status=C.c_int();self.glGetProgramiv(program,0x8b82,C.byref(status))
        if not status.value:
            log=C.create_string_buffer(4096);self.glGetProgramInfoLog(program,len(log),None,log);raise AssertionError(log.value.decode())
        self.glUseProgram(program)
        def location(name):return self.glGetUniformLocation(program,name.encode())
        for name,values in [('oracleColor',[.1,.3,.7,.9]),('oraclePosition',[.2,.4,.6,1]),('oracleNormal',[.3,.4,0,.7]),('oracleUV',case['uv']+[0,0])]:
            self.glUniform4f(location(name),*values)
        for slot in range(case['uniforms']):self.glUniform4f(location(f'kryonUniforms[{slot}]'),*([.14,.36,.58,.8] if slot==0 else [.21,.43,.65,.87] if slot==15 else [0,0,0,0]))
        textures=(C.c_uint*case['samplers'])();self.glGenTextures(len(textures),textures)
        pixels=(C.c_ubyte*16)(255,0,0,255,0,255,0,128,0,0,255,64,255,255,0,192)
        for slot,texture in enumerate(textures):
            self.glActiveTexture(0x84c0+slot);self.glBindTexture(0x0de1,texture)
            for parameter,value in [(0x2800,0x2600),(0x2801,0x2600),(0x2802,0x812f),(0x2803,0x812f)]:self.glTexParameteri(0x0de1,parameter,value)
            self.glTexImage2D(0x0de1,0,0x1908,2,2,0,0x1908,0x1401,pixels)
            self.glUniform1i(location(f'kryonSampler{slot}'),slot)
            self.glUniform2f(location(f'kryonSamplerSize[{slot}]'),2,2)
            self.glUniform1f(location(f'kryonSamplerRepeat[{slot}]'),float(case['repeat']))
        positions=(C.c_float*8)(-1,-1,1,-1,-1,1,1,1);attribute=self.glGetAttribLocation(program,b'aPosition')
        self.glEnableVertexAttribArray(attribute);self.glVertexAttribPointer(attribute,2,0x1406,0,0,positions)
        self.glViewport(0,0,4,4)
        for flag in (0x0bd0,0x0be2,0x0b71):self.glDisable(flag)
        sentinel=[23,47,71,97];self.glClearColor(*(v/255 for v in sentinel));self.glClear(0x4000)
        self.glDrawArrays(5,0,4);pixel=(C.c_ubyte*4)();self.glReadPixels(1,1,1,1,0x1908,0x1401,pixel)
        assert self.glGetError()==0,case['index']
        expected=sentinel if case['discard'] else [round(max(0,min(1,v))*255) for v in case['expected']]
        assert max(abs(a-b) for a,b in zip(pixel,expected))<=2,(case['index'],list(pixel),expected)
        self.glDeleteTextures(len(textures),textures);self.glDeleteProgram(program)
        self.glDeleteShader(vertex);self.glDeleteShader(fragment)
        return dict(index=case['index'],actual=list(pixel),expected=expected)

    def close(self):
        p=C.c_void_p;u=C.c_uint
        bind(self.egl,'eglMakeCurrent',u,p,p,p,p)(self.display,None,None,None)
        bind(self.egl,'eglDestroyContext',u,p,p)(self.display,self.context)
        bind(self.egl,'eglDestroySurface',u,p,p)(self.display,self.surface)
        bind(self.egl,'eglTerminate',u,p)(self.display)


def cases(path):
    case=None
    with path.open() as stream:
        for line in stream:
            parts=line.split()
            if line.startswith('kryon-glsl-case '):
                case=dict(index=int(parts[1]),expected=list(map(float,parts[2:6])),discard=parts[6]=='true',uniforms=int(parts[7]),samplers=int(parts[8]),source='')
            elif line.startswith('kryon-glsl-uv '):case['uv']=list(map(float,parts[1:]))
            elif line.startswith('kryon-glsl-repeat '):case['repeat']=parts[1]=='true'
            elif line.startswith('kryon-glsl-end'):yield case;case=None
            elif case is not None:case['source']+=line


def main():
    path=Path(sys.argv[1]).resolve();env=os.environ.copy()
    for key in ('DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','DESKTOP_SESSION','XDG_SESSION_ID','XDG_SESSION_TYPE','XDG_SESSION_DESKTOP','XDG_CURRENT_DESKTOP','XDG_RUNTIME_DIR'):env.pop(key,None)
    env.update(HARMONY_DESKTOP_RECOVERY='0',LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',EGL_PLATFORM='x11')
    readfd,writefd=os.pipe()
    with (path.parent/'gles-xvfb.log').open('w') as log:
        server=subprocess.Popen(['Xvfb','-displayfd',str(writefd),'-screen','0','64x64x24','-nolisten','tcp','-ac'],pass_fds=(writefd,),env=env,stdout=log,stderr=log)
        os.close(writefd)
        try:
            assert select.select([readfd],[],[],30)[0],'private Xvfb startup timed out'
            number=os.read(readfd,32).decode().strip();assert number.isdigit()
            command=Path(f'/proc/{server.pid}/cmdline').read_bytes().split(b'\0')
            assert Path(os.fsdecode(command[0])).name=='Xvfb' and b'-displayfd' in command
            env['DISPLAY']=':'+number;os.environ.clear();os.environ.update(env)
            gl=GLES()
            try:results=[gl.run(case) for case in cases(path)]
            finally:gl.close()
            assert [r['index'] for r in results]==list(range(34)),len(results)
            report=dict(**gl.identity,cases=results,private_xvfb=True)
            (path.parent/'gles-result.json').write_text(json.dumps(report,indent=2)+'\n')
            print(f'Local ES100 GLES pixel oracle passed: {len(results)} cases; {gl.identity}',flush=True)
        finally:
            os.close(readfd);server.terminate();server.wait(timeout=5)


if __name__=='__main__':main()
