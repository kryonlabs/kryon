export function createEditor(options) {
  const previewQuery = new URLSearchParams(location.search);
  let sourceURL = options.sourceURL;
  let search = options.search;
  let exampleGeneration = 0;
  let disabled = false;
  const editor = document.getElementById('source-editor');
  const highlight = document.querySelector('#highlight code');
  const workspace = document.getElementById('workspace');
  const sourcePanel = document.getElementById('source-panel');
  const previewPanel = document.getElementById('preview-panel');
  const previewLoading = document.getElementById('preview-loading');
  const previewLoadingMessage = document.getElementById('preview-loading-message');
  const staticPreview = options.staticPreview;
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
  let staticReady = false;

  function showStatic(visible) {
    staticPreview.hidden = !visible;
    staticPreview.contentWindow?.postMessage({type: 'preview:pause', paused: !visible}, location.origin);
    if (!visible && !options.keepStatic) {
      staticPreview.src = 'about:blank'; staticReady = false;
    } else if (visible && (!staticPreview.hasAttribute('src') || staticPreview.getAttribute('src') === 'about:blank')) {
      staticReady = false;
      staticPreview.src = previewURL(options.staticURL);
    }
  }

  function selectedTheme() {
    return document.documentElement.dataset.theme === 'dark' ? 2 : previewQuery.get('theme') === 'waozi' ? 1 : 0;
  }
  function previewURL(asset) {
    const query = new URLSearchParams();
    if (mini) query.set('mini', '');
    if (previewQuery.get('theme') === 'waozi') query.set('theme', 'waozi');
    if (document.documentElement.dataset.theme === 'dark') query.set('color', 'dark');
    return asset + (query.size ? '?' + query : '');
  }
  function sendTheme() {
    if (rendererReady) livePreview.contentWindow?.postMessage({type: 'preview:options', disabled}, '*');
    const message = {type: 'preview:theme', theme: selectedTheme()};
    if (staticPreview.src !== 'about:blank') staticPreview.contentWindow?.postMessage(message, '*');
    if (rendererReady) livePreview.contentWindow?.postMessage(message, '*');
  }
  staticPreview.addEventListener('load', () => {
    staticReady = staticPreview.getAttribute('src') !== 'about:blank';
    sendTheme();
    if (status.textContent === 'Loading preview…') setStatus('Ready to explore');
    else setStatus(status.textContent, status.dataset.state);
  });
  addEventListener('themechange', sendTheme);
  staticPreview.src = previewURL(options.staticURL);
  function setStatus(message, state = 'ready') {
    status.textContent = message;
    status.dataset.state = state;
    const busy = state === 'busy';
    previewPanel.setAttribute('aria-busy', String(busy));
    const visiblePreview = (!staticPreview.hidden && staticReady) || (!livePreview.hidden && livePreview.classList.contains('is-active'));
    previewLoading.hidden = !busy || visiblePreview;
    if (busy) previewLoadingMessage.textContent = message === 'Updating preview…' || message === 'Loading preview…' ? message : 'Compiling preview…';
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
      const generation = exampleGeneration;
      try {
        const response = await fetch(sourceURL);
        if (!response.ok) throw new Error('Unable to load the demo source. Reload to try again.');
        const text = await response.text();
        if (generation !== exampleGeneration) return false;
        original = text;
        editor.value = original;
        editor.disabled = compileButton.disabled = resetButton.disabled = false;
        paintSource();
        if (options.autoCompile === false) setStatus('Ready to explore');
        return true;
      } catch (cause) {
        if (generation !== exampleGeneration) return false;
        error(cause.message); sourcePromise = undefined; return false;
      }
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
    if (view !== 'preview' && await loadSource() && compiledRevision < 0 && !workerBusy && options.autoCompile !== false) compile();
  }
  document.querySelectorAll('button[data-view]').forEach(button => button.addEventListener('click', () => {
    requestedView = button.dataset.view;
    applyView();
  }));
  wide.addEventListener('change', applyView);

  function stop(message = 'Compilation stopped', terminate = true) {
    clearTimeout(editTimer); editTimer = undefined;
    clearTimeout(watchdog);
    if (terminate) { worker?.terminate(); worker = undefined; }
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
      showStatic(true);
    }, 15000);
  }
  function postBundle() {
    if (!rendererReady || !pendingBundle) return;
    const bundle = pendingBundle;
    pendingBundle = undefined;
    rendererDiagnostics = [];
    livePreview.contentWindow.postMessage({type: 'preview:pause', paused: false}, '*');
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
    livePreview.hidden = false;
    if (!livePreview.hasAttribute('src')) {
      livePreview.src = previewURL('assets/preview.html');
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
    worker.postMessage({source: editor.value, revision, search});
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
  resetButton.addEventListener('click', () => { editor.value = original; revision++; paintSource(); compile(); });
  addEventListener('message', ({source, data}) => {
    if (source !== livePreview.contentWindow || !data || typeof data !== 'object') return;
    if (data.type === 'preview:status' && data.state === 'ready') {
      rendererReady = true;
      sendTheme();
      postBundle();
    } else if (data.revision === revision && data.type === 'preview:diagnostic') {
      if (rendererDiagnostics.join('\n').length < 32768) rendererDiagnostics.push(data.message);
    } else if (data.revision === revision && data.type === 'preview:status') {
      clearTimeout(renderWatchdog);
      if (data.state === 'rendered') {
        successfulRevision = revision;
        livePreview.classList.add('is-active');
        livePreview.removeAttribute('aria-hidden');
        showStatic(false);
        setStatus('Live · changes compiled');
      } else if (data.state === 'error') error(rendererDiagnostics.join('\n') || data.message);
    }
  });
  addEventListener('pagehide', () => {
    clearTimeout(editTimer); clearTimeout(watchdog); clearTimeout(renderWatchdog);
    worker?.terminate();
  });
  applyView();
  return {
    setOptions(value) { disabled = value.disabled === true; sendTheme(); },
    async setExample(example) {
      exampleGeneration++;
      sourceURL = example.sourceURL; search = example.search;
      stop('Loading source…', workerBusy);
      original = undefined; sourcePromise = undefined; pendingBundle = undefined;
      successfulRevision = compiledRevision = -1;
      clearTimeout(renderWatchdog);
      if (rendererReady) livePreview.contentWindow.postMessage({type: 'preview:pause', paused: true}, '*');
      else livePreview.removeAttribute('src');
      livePreview.classList.remove('is-active'); livePreview.hidden = true;
      showStatic(true);
      editor.value = ''; paintSource();
      editor.disabled = compileButton.disabled = resetButton.disabled = true;
      await applyView();
    }
  };
}
