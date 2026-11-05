(function () {
  var root = document.documentElement;
  var saved;
  try { saved = localStorage.getItem('kryon-theme'); } catch (_) {}
  var query = new URLSearchParams(location.search);
  var selected = query.get('color') || query.get('theme');
  if (selected !== 'dark' && selected !== 'light') selected = saved;
  var toggles = document.querySelectorAll('.theme-toggle');
  function valid(theme) { return theme === 'dark' || theme === 'light'; }
  function sendTheme(frame) {
    frame.contentWindow?.postMessage({type: 'site:theme', theme: root.dataset.theme}, '*');
  }
  function applyTheme(theme) {
    if (!valid(theme)) return;
    root.dataset.theme = theme;
    toggles.forEach(function (toggle) { toggle.setAttribute('aria-pressed', String(theme === 'dark')); });
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
  toggles.forEach(function (toggle) {
    toggle.addEventListener('click', function () {
      var next = root.dataset.theme === 'dark' ? 'light' : 'dark';
      applyTheme(next);
      try { localStorage.setItem('kryon-theme', next); } catch (_) {}
    });
  });

  // The narrow-screen menu opens and closes the primary navigation.
  var menu = document.querySelector('.menu-button');
  var nav = document.getElementById('primary-nav');
  if (!menu || !nav) return;
  function setMenu(open, returnFocus) {
    menu.setAttribute('aria-expanded', String(open));
    nav.classList.toggle('is-open', open);
    if (!open && returnFocus) menu.focus();
  }
  function isOpen() { return menu.getAttribute('aria-expanded') === 'true'; }
  menu.addEventListener('click', function () { setMenu(!isOpen(), false); });
  document.addEventListener('keydown', function (event) {
    if (event.key === 'Escape' && isOpen()) setMenu(false, true);
  });
  document.addEventListener('click', function (event) {
    if (isOpen() && !menu.contains(event.target) && !nav.contains(event.target)) setMenu(false, false);
  });
  nav.addEventListener('click', function (event) {
    if (event.target.closest('a')) setMenu(false, false);
  });
  matchMedia('(min-width: 651px)').addEventListener('change', function (event) {
    if (event.matches) setMenu(false, false);
  });
})();
