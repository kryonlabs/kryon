(function () {
  var root = document.documentElement;
  var saved;
  try { saved = localStorage.getItem('kryon-theme'); } catch (_) {}
  if (saved === 'dark' || saved === 'light') root.dataset.theme = saved;
  var toggle = document.querySelector('.theme-toggle');
  if (toggle) toggle.addEventListener('click', function () {
    var next = root.dataset.theme === 'dark' ? 'light' : 'dark';
    root.dataset.theme = next;
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
