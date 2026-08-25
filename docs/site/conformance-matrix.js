(function() {
  var summary = document.querySelector("[data-conformance-summary]");
  var pipelineBody = document.querySelector("[data-conformance-pipelines]");
  var rendererBody = document.querySelector("[data-conformance-renderers]");
  var rendererCheckBody = document.querySelector("[data-conformance-renderer-checks]");
  var runtimeCheckBody = document.querySelector("[data-conformance-runtime-checks]");
  var downstreamCheckBody = document.querySelector("[data-conformance-downstream-checks]");
  var sourceBody = document.querySelector("[data-conformance-sources]");
  var search = document.querySelector("[data-conformance-search]");
  var typeFilter = document.querySelector("[data-conformance-type]");
  var widgetBody = document.querySelector("[data-conformance-widgets]");
  if (!summary || !pipelineBody || !rendererBody || !sourceBody) {
    return;
  }

  function escapeText(value) {
    return String(value == null ? "" : value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");
  }

  function cell(status) {
    var cls = status.status_class || "na";
    return '<td class="' + escapeText(cls) + '">' + escapeText(status.status || "unknown") + "</td>";
  }

  function evidence(items) {
    if (Array.isArray(items)) {
      return items.map(function(item) {
        return "<code>" + escapeText(item) + "</code>";
      }).join(", ");
    }
    return escapeText(items || "");
  }

  function renderSummary(data) {
    var s = data.summary;
    summary.innerHTML = [
      ["Sources", s.source_cases],
      ["Examples", s.examples],
      ["Parity Fixtures", s.parity_fixtures],
      ["Pipeline Cells", s.pipeline_cells],
      ["KRB RGB Visual", s.krb_rgb_visual_cases],
      ["KRB Byte Exact", s.krb_alpha_byte_exact_cases],
      ["State Parity", s.semantic_parity_cases],
      ["Widgets Seen", s.widgets_detected]
    ].map(function(item) {
      return '<div class="matrix-stat"><span>' + escapeText(item[0]) + "</span><strong>" + escapeText(item[1]) + "</strong></div>";
    }).join("");
  }

  function renderPipelines(data) {
    pipelineBody.innerHTML = data.pipelines.map(function(row) {
      return "<tr><td><code>" + escapeText(row.id) + "</code></td><td>" + escapeText(row.label) + "</td>" +
        cell(row) + "<td>" + evidence(row.evidence) + "</td></tr>";
    }).join("");
  }

  function renderRenderers(data) {
    rendererBody.innerHTML = data.renderers.map(function(row) {
      return "<tr><td>" + escapeText(row.label) + "</td><td>" + escapeText(row.platform) + "</td><td>" +
        escapeText(row.approach) + "</td>" + cell(row) + "<td>" + evidence(row.evidence) + "</td><td>" +
        escapeText(row.scope || "") + "</td><td>" + escapeText(row.notes) + "</td></tr>";
    }).join("");
  }

  function renderRendererChecks(data) {
    renderCheckRows(rendererCheckBody, data.renderer_checks);
  }

  function renderCheckRows(body, rows) {
    if (!body) {
      return;
    }
    body.innerHTML = (rows || []).map(function(row) {
      return "<tr><td>" + escapeText(row.label) + "</td><td><code>" +
        escapeText(row.command) + "</code></td><td>" + escapeText(row.scope) + "</td></tr>";
    }).join("");
  }

  function pipelineCells(row) {
    return ["k2ir", "k2c", "k2g", "k2b"].map(function(id) {
      return cell(row.pipelines[id] || {status: "missing", status_class: "no"});
    }).join("");
  }

  function visualCells(row) {
    var visuals = row.visuals || {};
    return cell(visuals.krb_rgb || {status: "missing", status_class: "no"}) +
      cell(visuals.krb_alpha || {status: "missing", status_class: "no"});
  }

  function renderSources(data) {
    var q = (search && search.value || "").trim().toLowerCase();
    var type = typeFilter && typeFilter.value || "all";
    var rows = data.cases.filter(function(row) {
      if (type !== "all" && row.type !== type) {
        return false;
      }
      if (!q) {
        return true;
      }
      return (row.path + " " + row.label + " " + row.widgets.join(" ")).toLowerCase().indexOf(q) !== -1;
    });
    sourceBody.innerHTML = rows.map(function(row) {
      var widgets = row.widgets.length ? row.widgets.map(function(widget) {
        return '<span class="matrix-chip">' + escapeText(widget) + "</span>";
      }).join("") : '<span class="matrix-muted">No widget calls detected</span>';
      return "<tr><td><code>" + escapeText(row.path) + "</code><br><span class=\"matrix-muted\">" +
        escapeText(row.label) + "</span></td><td>" + escapeText(row.type) + "</td>" +
        pipelineCells(row) + visualCells(row) + "<td>" + widgets + "</td><td>" +
        escapeText(row.semantic_evidence) + "</td></tr>";
    }).join("");
  }

  function renderWidgets(data) {
    if (!widgetBody) {
      return;
    }
    var widgets = Object.keys(data.widget_counts).sort(function(a, b) {
      return data.widget_counts[b] - data.widget_counts[a] || a.localeCompare(b);
    });
    widgetBody.innerHTML = widgets.map(function(widget) {
      return "<tr><td>" + escapeText(widget) + "</td><td>" + escapeText(data.widget_counts[widget]) + "</td></tr>";
    }).join("");
  }

  fetch("conformance-matrix.json")
    .then(function(response) {
      if (!response.ok) {
        throw new Error("matrix fetch failed");
      }
      return response.json();
    })
    .then(function(data) {
      renderSummary(data);
      renderPipelines(data);
      renderRenderers(data);
      renderRendererChecks(data);
      renderCheckRows(runtimeCheckBody, data.runtime_checks);
      renderCheckRows(downstreamCheckBody, data.downstream_checks);
      renderSources(data);
      renderWidgets(data);
      if (search) {
        search.addEventListener("input", function() { renderSources(data); });
      }
      if (typeFilter) {
        typeFilter.addEventListener("change", function() { renderSources(data); });
      }
    })
    .catch(function() {
      summary.innerHTML = '<p class="section-lead">The generated conformance matrix could not be loaded.</p>';
    });
})();
