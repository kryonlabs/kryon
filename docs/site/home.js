(function () {
  "use strict";
  var grid = document.querySelector("[data-home-projects]");
  var selected = ["inner-breeze", "pass", "krait"];
  function element(tag, className, text) {
    var node = document.createElement(tag);
    node.className = className;
    if (text) node.textContent = text;
    return node;
  }
  fetch("showcase-data.json").then(function (response) {
    if (!response.ok) throw new Error("Showcase unavailable");
    return response.json();
  }).then(function (data) {
    var cards = selected.map(function (slug) {
      var project = data.projects.find(function (entry) { return entry.slug === slug; });
      if (!project) return null;
      var card = element("article", "app-card");
      var image = element("img", "app-card-image");
      image.src = project.banner;
      image.alt = project.bannerAlt || project.name + " application screenshot";
      image.width = 640;
      image.height = 400;
      image.loading = "lazy";
      card.appendChild(image);
      var body = element("div", "app-card-body");
      body.appendChild(element("span", "app-kicker", project.tags.join(" · ")));
      body.appendChild(element("h3", "", project.name));
      body.appendChild(element("p", "", project.summary));
      var actions = element("div", "app-actions");
      var open = element("a", "text-link", "Explore " + project.name + " ↗");
      open.href = project.homepage || project.repository;
      actions.appendChild(open);
      if (project.repository) {
        var source = element("a", "text-link", "Source");
        source.href = project.repository;
        actions.appendChild(source);
      }
      body.appendChild(actions);
      card.appendChild(body);
      return card;
    }).filter(Boolean);
    if (cards.length) grid.replaceChildren.apply(grid, cards);
  }).catch(function () {
    // Keep the useful static showcase link if the registry cannot be loaded.
  });
})();
