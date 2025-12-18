# Table Examples

This document demonstrates table formatting supported by Kryon's markdown renderer.

## Simple Table

| Name | Age | City |
|------|-----|------|
| Alice | 28 | Seattle |
| Bob | 34 | Portland |
| Carol | 22 | Austin |

## Table with Alignment

Tables support left, center, and right alignment:

| Left Aligned | Center Aligned | Right Aligned |
|:-------------|:--------------:|--------------:|
| Apple        | $1.50          | 100           |
| Banana       | $0.75          | 250           |
| Cherry       | $3.00          | 50            |

## Table with Different Column Widths

| Short | Medium Length | Very Long Column Header |
|-------|--------------|-------------------------|
| A     | Data         | More information here   |
| B     | Values       | Additional details      |
| C     | Items        | Extended content        |

## Table with Inline Formatting

| Feature | Status | Description |
|---------|:------:|-------------|
| **Bold** | ✅ | *Works great* |
| *Italic* | ✅ | `Code too` |
| `Code` | ✅ | [Links](url) |
| ~~Strike~~ | ✅ | All formatting |

## Table with Empty Cells

| Column 1 | Column 2 | Column 3 |
|----------|----------|----------|
| Data     |          | More     |
|          | Empty    |          |
| Full     | Row      | Here     |

## Compact Table (no spacing)

|A|B|C|
|-|-|-|
|1|2|3|
|4|5|6|

## Table with Long Content

| ID | Description | Notes |
|----|-------------|-------|
| 1 | This is a longer piece of text that demonstrates how table cells handle wrapping and longer content | Additional notes can go here |
| 2 | Another row with substantial text to show table rendering behavior | More information |

## Pipe Characters in Content

To include pipe characters `|` in table cells, escape them:

| Code | Output |
|------|--------|
| `a \| b` | a \| b |
| `x \|\| y` | x \|\| y |

## Real-World Example: Feature Matrix

| Feature | Basic | Pro | Enterprise |
|---------|:-----:|:---:|:----------:|
| Users | 5 | 50 | Unlimited |
| Storage | 10 GB | 100 GB | 1 TB |
| Support | Email | Priority | 24/7 |
| Price | $10/mo | $50/mo | Custom |

---

*Run with `kryon run table_demo.md` to see how tables render with proper alignment!*
