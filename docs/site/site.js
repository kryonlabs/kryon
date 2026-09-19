(function () {
  "use strict";
  var menu = document.querySelector(".menu-button");
  var nav = document.getElementById("primary-navigation");
  if (menu && nav) {
    menu.addEventListener("click", function () {
      var open = nav.classList.toggle("is-open");
      menu.setAttribute("aria-expanded", String(open));
    });
    document.addEventListener("keydown", function (event) {
      if (event.key === "Escape" && nav.classList.contains("is-open")) {
        nav.classList.remove("is-open");
        menu.setAttribute("aria-expanded", "false");
        menu.focus();
      }
    });
  }
  document.querySelectorAll("pre").forEach(function (block) {
    if (block.classList.contains("artifact-output")) return;
    var wrapper = document.createElement("div");
    wrapper.className = "code-block";
    block.parentNode.insertBefore(wrapper, block);
    wrapper.appendChild(block);
    var button = document.createElement("button");
    button.type = "button";
    button.className = "copy-code";
    button.textContent = "Copy";
    button.setAttribute("aria-label", "Copy code to clipboard");
    button.addEventListener("click", function () {
      if (!navigator.clipboard) {
        button.textContent = "Select code to copy";
        return;
      }
      navigator.clipboard.writeText(block.textContent).then(function () {
        button.textContent = "Copied";
        window.setTimeout(function () { button.textContent = "Copy"; }, 1800);
      }).catch(function () { button.textContent = "Select code to copy"; });
    });
    wrapper.appendChild(button);
  });

  var contents = document.querySelector("[data-page-contents]");
  document.querySelectorAll("[data-tabs] [role=tab]").forEach(function (tab) {
    tab.addEventListener("keydown", function (event) {
      var tabs = Array.from(tab.closest("[data-tabs]").querySelectorAll("[role=tab]"));
      var index = tabs.indexOf(tab);
      if (event.key === "ArrowRight") index = (index + 1) % tabs.length;
      else if (event.key === "ArrowLeft") index = (index - 1 + tabs.length) % tabs.length;
      else if (event.key === "Home") index = 0;
      else if (event.key === "End") index = tabs.length - 1;
      else return;
      event.preventDefault();
      tabs[index].focus();
      tabs[index].click();
    });
  });

  if (contents) {
    document.querySelectorAll("main h2").forEach(function (heading) {
      if (!heading.id) {
        heading.id = heading.textContent.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
      }
      var link = document.createElement("a");
      link.href = "#" + heading.id;
      link.textContent = heading.textContent;
      contents.appendChild(link);
    });
    var search = document.querySelector("[data-contents-search]");
    if (search) {
      search.addEventListener("input", function () {
        contents.querySelectorAll("a").forEach(function (link) {
          link.hidden = !link.textContent.toLowerCase().includes(search.value.toLowerCase());
        });
      });
    }
  }

  document.querySelectorAll("main table").forEach(function (table) {
    var wrapper = table.closest(".table-wrap");
    if (!wrapper) {
      wrapper = document.createElement("div");
      wrapper.className = "table-wrap";
      table.parentNode.insertBefore(wrapper, table);
      wrapper.appendChild(table);
    }
    wrapper.tabIndex = 0;
    wrapper.setAttribute("role", "region");
    wrapper.setAttribute("aria-label", "Scrollable reference table");
  });
})();
