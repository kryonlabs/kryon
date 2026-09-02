(function() {
  "use strict";

  var root = document.documentElement;
  var storageKey = "kryon-theme";
  var media = window.matchMedia("(prefers-color-scheme: light)");
  var saved = null;

  try { saved = localStorage.getItem(storageKey); } catch (_) {}

  function preferredTheme() {
    return saved === "light" || saved === "dark" ? saved : (media.matches ? "light" : "dark");
  }

  function applyTheme(theme) {
    root.dataset.theme = theme;
    root.style.colorScheme = theme;
    var meta = document.querySelector('meta[name="theme-color"]');
    if (meta) meta.content = theme === "light" ? "#f4efdf" : "#151512";
    var button = document.querySelector(".theme-toggle");
    if (button) {
      var next = theme === "dark" ? "light" : "dark";
      button.setAttribute("aria-label", "Use " + next + " theme");
      button.setAttribute("title", "Use " + next + " theme");
      button.innerHTML = '<span aria-hidden="true">' + (theme === "dark" ? "☀" : "☾") + '</span><b>' + next + '</b>';
    }
  }

  applyTheme(preferredTheme());

  document.addEventListener("DOMContentLoaded", function() {
    var nav = document.querySelector(".header-nav");
    if (!nav || nav.querySelector(".theme-toggle")) return;
    var button = document.createElement("button");
    button.className = "theme-toggle";
    button.type = "button";
    var primary = nav.querySelector(".btn-primary");
    nav.insertBefore(button, primary || null);
    button.addEventListener("click", function() {
      var next = root.dataset.theme === "dark" ? "light" : "dark";
      saved = next;
      try { localStorage.setItem(storageKey, next); } catch (_) {}
      applyTheme(next);
    });
    applyTheme(root.dataset.theme || preferredTheme());
  });

  media.addEventListener("change", function() {
    if (!saved) applyTheme(preferredTheme());
  });
})();
