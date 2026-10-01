(function () {
  var root = document.documentElement;
  var saved;
  try { saved = localStorage.getItem('kryon-theme'); } catch (_) {}
  var query = new URLSearchParams(location.search);
  var selected = query.get('color') || query.get('theme');
  if (selected !== 'dark' && selected !== 'light') selected = saved;
  function valid(theme) { return theme === 'dark' || theme === 'light'; }
  function sendTheme(frame) {
    frame.contentWindow?.postMessage({type: 'site:theme', theme: root.dataset.theme}, '*');
  }
  function applyTheme(theme) {
    if (!valid(theme)) return;
    root.dataset.theme = theme;
    dispatchEvent(new CustomEvent('themechange', {detail: theme}));
    document.querySelectorAll('iframe').forEach(sendTheme);
  }
  document.querySelectorAll('iframe').forEach(function (frame) {
    frame.addEventListener('load', function () { sendTheme(frame); });
  });
  addEventListener('message', function (event) {
    if (parent !== window && event.source === parent && event.data?.type === 'site:theme') applyTheme(event.data.theme);
  });
  addEventListener('storage', function (event) {
    if (event.key === 'kryon-theme') applyTheme(event.newValue);
  });
  applyTheme(valid(selected) ? selected : 'light');
  var toggle = document.querySelector('.theme-toggle');
  if (toggle) toggle.addEventListener('click', function () {
    var next = root.dataset.theme === 'dark' ? 'light' : 'dark';
    applyTheme(next);
    try { localStorage.setItem('kryon-theme', next); } catch (_) {}
  });
  var menu = document.querySelector('.menu-button');
  var nav = document.getElementById('primary-nav');
  if (menu && nav) menu.addEventListener('click', function () {
    var open = menu.getAttribute('aria-expanded') !== 'true';
    menu.setAttribute('aria-expanded', String(open));
    nav.classList.toggle('is-open', open);
  });
})();
