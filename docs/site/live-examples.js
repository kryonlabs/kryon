(function () {
  "use strict";
  var destination = "ide.html" + window.location.search + window.location.hash;
  document.querySelector("[data-playground-link]").href = destination;
  window.location.replace(destination);
})();
