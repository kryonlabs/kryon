(function() {
  var keywords = /\b(return|if|else|while|for|int|float|void|const|static|typedef|struct|enum|include|define|draw|native)\b|#\b(module|output|import|enum|global|export|private|extern|intrinsic|type)\b/g;
  var types = /\b(Color|Rectangle|Texture2D|Camera2D|UIPanelFrame|UITextField|UILabelTextField|Font)\b/g;
  var functions = /\b([A-Za-z_][A-Za-z0-9_]*)\s*(?=\()/g;
  var strings = /("([^"\\]|\\.)*"|'([^'\\]|\\.)*')/g;
  var comments = /(\/\/[^\n]*|\/\*[\s\S]*?\*\/)/g;
  var numbers = /\b(\d+(?:\.\d+)?f?)\b/g;

  function escapeHtml(text) {
    return text
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;");
  }

  function span(cls, text) {
    return '<span class="' + cls + '">' + text + '</span>';
  }

  function highlight(text) {
    var placeholders = [];

    function keep(html) {
      var id = placeholders.length;
      placeholders.push(html);
      return "\u0000P" + id + "X\u0000";
    }

    text = escapeHtml(text);
    text = text.replace(comments, function(match) {
      return keep(span("tok-com", match));
    });
    text = text.replace(strings, function(match) {
      return keep(span("tok-str", match));
    });
    text = text
      .replace(types, function(match) { return span("tok-type", match); })
      .replace(keywords, function(match) { return span("tok-key", match); })
      .replace(numbers, function(match) { return span("tok-num", match); })
      .replace(functions, function(match, name) {
        return span("tok-fn", name);
      });

    return text.replace(/\u0000P(\d+)X\u0000/g, function(_, id) {
      return placeholders[Number(id)];
    });
  }

  document.querySelectorAll("pre code").forEach(function(block) {
    block.innerHTML = highlight(block.textContent);
  });
})();
