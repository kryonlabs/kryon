// The staged demo page: an ordinary WebGL cube on the island vocabulary.
// spin sets the rotation speed; focus pauses and resumes the frame loop;
// state reports what a native host needs to verify the render loop. No
// third-party module is used, so the staged demo needs no downloads.
let speed = 1, last = 0, frames = 0, active = true, rotation = 0;
const start = async () => {
  const canvas = document.querySelector('canvas');
  const gl = canvas.getContext('webgl', { antialias: true });
  if (!gl) throw Error('WebGL is unavailable');
  const make = (kind, source) => {
    const shader = gl.createShader(kind);
    gl.shaderSource(shader, source); gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw Error(gl.getShaderInfoLog(shader) || 'Shader did not compile');
    return shader;
  };
  const program = gl.createProgram();
  gl.attachShader(program, make(gl.VERTEX_SHADER,
    'attribute vec3 point; uniform mat4 spin; void main() { gl_Position = spin * vec4(point, 1.0); }'));
  gl.attachShader(program, make(gl.FRAGMENT_SHADER,
    'precision mediump float; void main() { gl_FragColor = vec4(0.38, 0.93, 1.0, 1.0); }'));
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw Error('Shader program did not link');
  gl.useProgram(program);
  const corners = new Float32Array([
    -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, -1, -1, 1, 1, -1, -1, 1, -1,
    -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, -1, 1, 1, 1, 1, -1, 1, 1,
    -1, -1, -1, -1, -1, 1, -1, 1, 1, -1, -1, -1, -1, 1, 1, -1, 1, -1,
    1, -1, -1, 1, -1, 1, 1, 1, 1, 1, -1, -1, 1, 1, 1, 1, 1, -1,
    -1, 1, -1, -1, 1, 1, 1, 1, 1, -1, 1, -1, 1, 1, 1, 1, 1, -1,
    -1, -1, -1, 1, -1, -1, 1, -1, 1, -1, -1, -1, 1, -1, 1, -1, -1, 1]);
  gl.bindBuffer(gl.ARRAY_BUFFER, gl.createBuffer());
  gl.bufferData(gl.ARRAY_BUFFER, corners, gl.STATIC_DRAW);
  const point = gl.getAttribLocation(program, 'point');
  gl.enableVertexAttribArray(point);
  gl.vertexAttribPointer(point, 3, gl.FLOAT, false, 0, 0);
  const spin = gl.getUniformLocation(program, 'spin');
  gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);
  gl.clearColor(0.063, 0.094, 0.153, 1);
  const draw = () => {
    gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.enable(gl.DEPTH_TEST);
    const angle = rotation, cosine = Math.cos(angle), sine = Math.sin(angle);
    gl.uniformMatrix4fv(spin, false, new Float32Array([
      cosine, 0, sine, 0,
      0, 1, 0, 0,
      -sine, 0, cosine, 0,
      0, 0, -4, 1]));
    gl.drawArrays(gl.TRIANGLES, 0, corners.length / 3);
  };
  const scene = window.KryonScene = { frame: 0 };
  const resize = () => { canvas.width = innerWidth; canvas.height = innerHeight; draw(); };
  scene.draw = time => {
    scene.frame = 0; if (!active) return;
    rotation += Math.min(.1, (time - last) / 1000) * speed; last = time;
    draw(); frames++;
    scene.frame = requestAnimationFrame(scene.draw);
  };
  window.addEventListener('resize', resize); resize();
  window.KryonPage = {
    receive(command) {
      if (command.type === 'spin') {
        if (!Number.isFinite(command.speed) || Math.abs(command.speed) > 100) throw Error('Invalid spin speed');
        speed = command.speed; return;
      }
      if (command.type === 'focus') { active = command.active; last = performance.now(); }
    },
    state(islandActive) {
      return { mode: 'scene', active: islandActive, frames, speed, rotation,
        width: canvas.width, height: canvas.height };
    }
  };
  scene.frame = requestAnimationFrame(scene.draw);
  window.KryonIsland.ready({ mode: 'scene' });
};
start().catch(error => window.KryonIsland.fail(error?.message || String(error)));
