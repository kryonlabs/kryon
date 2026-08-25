(function() {
  var summary = document.querySelector("[data-conformance-summary]");
  var pipelineBody = document.querySelector("[data-conformance-pipelines]");
  var rendererBody = document.querySelector("[data-conformance-renderers]");
  var rendererCheckBody = document.querySelector("[data-conformance-renderer-checks]");
  var visualComparisonBody = document.querySelector("[data-conformance-visual-comparisons]");
  var runtimeCheckBody = document.querySelector("[data-conformance-runtime-checks]");
  var downstreamCheckBody = document.querySelector("[data-conformance-downstream-checks]");
  var sourceHead = document.querySelector("[data-conformance-source-head]");
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
    var title = [status.evidence, status.scope].filter(Boolean).join(" | ");
    return '<td class="' + escapeText(cls) + '" title="' + escapeText(title) + '">' +
      escapeText(status.status || "unknown") + "</td>";
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
      ["Raylib C Capture", s.raylib_c_visual_cases + " / " + s.source_cases],
      ["Raylib C Gaps", s.raylib_c_visual_gaps],
      ["Canvas C Capture", s.web_canvas_c_visual_cases + " / " + s.source_cases],
      ["Canvas C Gaps", s.web_canvas_c_visual_gaps],
      ["Libdraw C Capture", s.libdraw_c_visual_cases + " / " + s.source_cases],
      ["Libdraw C Gaps", s.libdraw_c_visual_gaps],
      ["Exact Comparisons", s.visual_comparison_exact_rows + " / " + s.visual_comparison_rows],
      ["Pending Comparisons", s.visual_comparison_pending_rows],
      ["Renderer Cells", s.renderer_source_cells],
      ["Renderer OK Cells", s.renderer_source_ok_cells],
      ["Renderer Partial Cells", s.renderer_source_partial_cells],
      ["State Parity", s.semantic_parity_cases],
      ["Widgets Covered", s.widgets_detected + " / " + s.widgets_declared],
      ["Widgets Missing", s.widgets_missing]
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

  function renderVisualComparisons(data) {
    if (!visualComparisonBody) {
      return;
    }
    visualComparisonBody.innerHTML = (data.visual_comparisons || []).map(function(row) {
      return "<tr><td>" + escapeText(row.label) + "</td><td>" + escapeText(row.mode) +
        "</td>" + cell(row) + "<td>" + escapeText(row.compared) + " / " +
        escapeText(row.source_cases) + "</td><td>" + escapeText(row.gap_cases) +
        " " + escapeText(row.gap_label) + "</td><td>" + evidence(row.evidence) +
        "</td><td>" + escapeText(row.scope) + "</td></tr>";
    }).join("");
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

  function sourceRendererColumns(data) {
    return data.source_renderers || [
      {id: "krb_rgb", label: "KRB RGB"},
      {id: "krb_alpha", label: "KRB Alpha"},
      {id: "libdraw_c", label: "Libdraw C"}
    ];
  }

  function renderSourceHead(data) {
    if (!sourceHead) {
      return;
    }
    var rendererHeaders = sourceRendererColumns(data).map(function(renderer) {
      return '<th title="' + escapeText(renderer.scope || "") + '">' + escapeText(renderer.label) + "</th>";
    }).join("");
    sourceHead.innerHTML = "<th>Source</th><th>Type</th><th>k2ir</th><th>k2c</th><th>k2g</th><th>k2b</th>" +
      rendererHeaders + "<th>Widgets</th><th>State Evidence</th>";
  }

  function sourceRendererCells(data, row) {
    var matrix = row.renderer_matrix || row.visuals || {};
    return sourceRendererColumns(data).map(function(renderer) {
      return cell(matrix[renderer.id] || {status: "missing", status_class: "no"});
    }).join("");
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
        pipelineCells(row) + sourceRendererCells(data, row) + "<td>" + widgets + "</td><td>" +
        escapeText(row.semantic_evidence) + "</td></tr>";
    }).join("");
  }

  function renderWidgets(data) {
    if (!widgetBody) {
      return;
    }
    var widgets = (data.widget_coverage || []).slice().sort(function(a, b) {
      return a.status_class.localeCompare(b.status_class) ||
        b.source_count - a.source_count ||
        a.id.localeCompare(b.id);
    });
    widgetBody.innerHTML = widgets.map(function(widget) {
      var sources = widget.sources.length ? widget.sources.map(function(source) {
        return "<code>" + escapeText(source) + "</code>";
      }).join(", ") : '<span class="matrix-muted">No .kry source</span>';
      var semantic = widget.semantic_sources.length ? widget.semantic_sources.map(function(source) {
        return "<code>" + escapeText(source) + "</code>";
      }).join(", ") : '<span class="matrix-muted">Lowering/visual only</span>';
      return "<tr><td>" + escapeText(widget.id) + "</td>" + cell(widget) +
        "<td>" + escapeText(widget.source_count) + "</td><td>" + sources +
        "</td><td>" + semantic + "</td></tr>";
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
      renderVisualComparisons(data);
      renderCheckRows(runtimeCheckBody, data.runtime_checks);
      renderCheckRows(downstreamCheckBody, data.downstream_checks);
      renderSourceHead(data);
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
