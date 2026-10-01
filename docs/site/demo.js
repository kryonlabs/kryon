(() => {
  const editor = document.getElementById('source-editor');
  const highlight = document.querySelector('#highlight code');
  const workspace = document.getElementById('workspace');
  const sourcePanel = document.getElementById('source-panel');
  const previewPanel = document.getElementById('preview-panel');
  const staticPreview = document.getElementById('demo-preview');
  const livePreview = document.getElementById('live-preview');
  const status = document.getElementById('status');
  const diagnostics = document.getElementById('diagnostics');
  const compileButton = document.getElementById('compile');
  const stopButton = document.getElementById('stop');
  const resetButton = document.getElementById('reset');
  const wide = matchMedia('(min-width: 960px)');
  const mini = previewQuery.has('mini');
  let requestedView = mini ? 'preview' : 'split';
  let original;
  let sourcePromise;
  let worker;
  let revision = 0;
  let workerBusy = false;
  let editTimer;
  let watchdog;
  let renderWatchdog;
  let rendererReady = false;
  let pendingBundle;
  let rendererDiagnostics = [];
  let successfulRevision = -1;
  let compiledRevision = -1;

  if (mini && previewQuery.get('theme') === 'waozi') staticPreview.src = 'assets/widgets.html?mini&theme=waozi';
  function setStatus(message, state = 'ready') {
    status.textContent = message;
    status.dataset.state = state;
  }
  function error(message) {
    setStatus('Check the source', 'error');
    diagnostics.textContent = message;
    diagnostics.hidden = false;
  }
  function paintSource() {
    const escape = text => text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    const tokens = /\/\/[^\n]*|"(?:\\.|[^"\\])*"|\b(?:using|if|else|ifx|then|return|while|for|cast|true|false|unused|struct|enum|null)\b|#[a-z_]+/g;
    let at = 0, html = '';
    for (const match of editor.value.matchAll(tokens)) {
      html += escape(editor.value.slice(at, match.index));
      const kind = match[0].startsWith('//') ? 'comment' : match[0].startsWith('"') ? 'string' : 'keyword';
      html += '<span class="token-' + kind + '">' + escape(match[0]) + '</span>';
      at = match.index + match[0].length;
    }
    highlight.innerHTML = html + escape(editor.value.slice(at)) + '\n';
    syncScroll();
  }
  function syncScroll() {
    highlight.parentElement.scrollTop = editor.scrollTop;
    highlight.parentElement.scrollLeft = editor.scrollLeft;
  }
  async function loadSource() {
    if (!sourcePromise) sourcePromise = (async () => {
      try {
        const response = await fetch('demo/src/app.zi');
        if (!response.ok) throw new Error('Unable to load the demo source. Reload to try again.');
        original = await response.text();
        editor.value = original;
        editor.disabled = compileButton.disabled = resetButton.disabled = false;
        paintSource();
        return true;
      } catch (cause) { error(cause.message); sourcePromise = undefined; return false; }
    })();
    return sourcePromise;
  }
  async function applyView() {
    const view = requestedView === 'split' && !wide.matches ? 'preview' : requestedView;
    workspace.dataset.view = view;
    sourcePanel.hidden = view === 'preview';
    previewPanel.hidden = view === 'source';
    document.querySelectorAll('button[data-view]').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.view === view)));
    if (view === 'source') {
      clearTimeout(renderWatchdog);
      if (!workerBusy && compiledRevision === revision && successfulRevision !== revision) setStatus('Compiled · open Preview');
    } else if (compiledRevision === revision && successfulRevision !== revision) {
      setStatus('Updating preview…', 'busy');
      watchRenderer(revision);
    }
    if (view !== 'preview' && await loadSource() && compiledRevision < 0 && !workerBusy) compile();
  }
  document.querySelectorAll('button[data-view]').forEach(button => button.addEventListener('click', () => {
    requestedView = button.dataset.view;
    applyView();
  }));
  wide.addEventListener('change', applyView);

  function stop(message = 'Compilation stopped') {
    clearTimeout(editTimer); editTimer = undefined;
    clearTimeout(watchdog);
    worker?.terminate(); worker = undefined;
    workerBusy = false;
    revision++;
    stopButton.hidden = true;
    compileButton.disabled = original === undefined;
    setStatus(message);
  }
  function watchRenderer(builtRevision) {
    clearTimeout(renderWatchdog);
    renderWatchdog = setTimeout(() => {
      if (builtRevision !== revision) return;
      error('The preview took too long. Try simplifying the program, then compile again.');
      livePreview.removeAttribute('src');
      livePreview.classList.remove('is-active');
      rendererReady = false;
      staticPreview.hidden = false;
      staticPreview.src = 'assets/widgets.html';
    }, 15000);
  }
  function postBundle() {
    if (!rendererReady || !pendingBundle) return;
    const bundle = pendingBundle;
    pendingBundle = undefined;
    rendererDiagnostics = [];
    livePreview.contentWindow.postMessage({type: 'preview:bundle', revision: bundle.revision, bundle: bundle.bytes}, '*', [bundle.bytes]);
    if (!previewPanel.hidden) watchRenderer(bundle.revision);
  }
  function showBundle(bytes, builtRevision) {
    compiledRevision = builtRevision;
    if (previewPanel.hidden) {
      clearTimeout(renderWatchdog);
      setStatus('Compiled · open Preview');
    } else {
      setStatus('Updating preview…', 'busy');
      watchRenderer(builtRevision);
    }
    pendingBundle = {bytes, revision: builtRevision};
    if (!livePreview.hasAttribute('src')) {
      livePreview.hidden = false;
      livePreview.src = 'assets/preview.html';
    }
    postBundle();
  }
  function compile() {
    clearTimeout(editTimer); editTimer = undefined;
    if (original === undefined || workerBusy) return;
    diagnostics.hidden = true;
    diagnostics.textContent = '';
    workerBusy = true;
    compileButton.disabled = true;
    stopButton.hidden = false;
    setStatus('Compiling…', 'busy');
    if (!worker) {
      const compiler = worker = new Worker('demo-worker.js');
      compiler.onmessage = ({data}) => {
        if (worker !== compiler) return;
        if (data.type === 'stage') {
          if (data.revision === revision) setStatus(data.stage, 'busy');
          return;
        }
        clearTimeout(watchdog);
        workerBusy = false;
        compileButton.disabled = false;
        stopButton.hidden = true;
        if (data.revision === revision) {
          if (data.ok) showBundle(data.bundle, data.revision);
          else error(data.diagnostics);
        } else if (!editTimer) compile();
      };
      compiler.onerror = event => {
        if (worker !== compiler) return;
        stop();
        error(event.message || 'Unable to load the browser compiler. Reload to try again.');
      };
    }
    worker.postMessage({source: editor.value, revision});
    watchdog = setTimeout(() => {
      stop();
      error('Compilation took too long. Try simplifying the source, then compile again.');
    }, 15000);
  }
  function changed() {
    revision++;
    paintSource();
    setStatus('Waiting for edits…', 'busy');
    clearTimeout(editTimer);
    if (!editor.matches(':disabled')) editTimer = setTimeout(compile, 350);
  }
  editor.addEventListener('input', event => { if (!event.isComposing) changed(); else paintSource(); });
  editor.addEventListener('compositionend', changed);
  editor.addEventListener('scroll', syncScroll);
  editor.addEventListener('keydown', event => {
    if (event.key === 'Enter' && (event.ctrlKey || event.metaKey)) { event.preventDefault(); compile(); }
    else if (event.key === 'Tab') {
      event.preventDefault();
      editor.setRangeText('    ', editor.selectionStart, editor.selectionEnd, 'end');
      changed();
    }
  });
  compileButton.addEventListener('click', compile);
  stopButton.addEventListener('click', () => stop());
  resetButton.addEventListener('click', () => { editor.value = original; changed(); });
  addEventListener('message', ({source, data}) => {
    if (source !== livePreview.contentWindow || !data || typeof data !== 'object') return;
    if (data.type === 'preview:status' && data.state === 'ready') {
      rendererReady = true;
      postBundle();
    } else if (data.revision === revision && data.type === 'preview:diagnostic') {
      if (rendererDiagnostics.join('\n').length < 32768) rendererDiagnostics.push(data.message);
    } else if (data.revision === revision && data.type === 'preview:status') {
      clearTimeout(renderWatchdog);
      if (data.state === 'rendered') {
        successfulRevision = revision;
        livePreview.classList.add('is-active');
        livePreview.removeAttribute('aria-hidden');
        staticPreview.hidden = true;
        staticPreview.src = 'about:blank';
        setStatus('Live · changes compiled');
      } else if (data.state === 'error') error(rendererDiagnostics.join('\n') || data.message);
    }
  });
  addEventListener('pagehide', () => {
    clearTimeout(editTimer); clearTimeout(watchdog); clearTimeout(renderWatchdog);
    worker?.terminate();
  });
  applyView();
})();
