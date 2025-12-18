# Inline Formatting Examples

This document demonstrates all inline formatting options supported by Kryon's markdown renderer.

## Text Styling

You can make text **bold** using double asterisks or __double underscores__.

You can make text *italic* using single asterisks or _single underscores_.

You can combine them: **bold and *italic* together** or ***all bold and italic***.

## Code

Inline `code` is shown using backticks. This is useful for `variable_names`, `function()`, and `file.txt`.

You can also use ``code with `backticks` inside`` by using double backticks.

## Links

Here are different types of links:

- [External link](https://example.com)
- [Link with title](https://example.com "Click me!")
- [Reference-style link][ref]
- Autolink: <https://example.com>
- Email: <user@example.com>

[ref]: https://example.com "Reference link"

## Strikethrough

~~This text is crossed out~~ using double tildes.

## Superscript and Subscript

(Note: Not all markdown parsers support these)

- Superscript: E = mc^2^
- Subscript: H~2~O

## HTML Entities

You can use HTML entities like &copy;, &reg;, &trade;, &mdash;, and &nbsp;.

Special characters are automatically escaped: < > & " '

## Escaping

You can escape special characters with a backslash:

- \*Not italic\*
- \**Not bold\**
- \`Not code\`
- \[Not a link\](url)

## Combined Examples

Here's a paragraph with **bold text**, *italic text*, `inline code`, a [link](https://example.com), and ~~strikethrough~~ all together in one sentence!

You can even have **bold with *nested italic* inside** or *italic with **nested bold** inside*.

---

*Try running this with `kryon run inline_formatting.md` to see how all these formats render!*
