# List Examples

This document demonstrates all list types supported by Kryon's markdown renderer.

## Unordered Lists

Simple bullet list using `-`:

- First item
- Second item
- Third item

Using `*`:

* Apple
* Banana
* Cherry

Using `+`:

+ Red
+ Green
+ Blue

## Ordered Lists

Simple numbered list:

1. First step
2. Second step
3. Third step

Starting from a different number:

5. Step five
6. Step six
7. Step seven

## Nested Lists

Lists can be nested by indenting:

- Fruits
  - Apples
    - Red Delicious
    - Granny Smith
  - Oranges
  - Bananas
- Vegetables
  - Carrots
  - Broccoli
  - Spinach

Mixed ordered and unordered:

1. Preparation
   - Gather ingredients
   - Preheat oven to 350°F
   - Prepare baking sheet
2. Mixing
   - Combine dry ingredients
     1. Flour
     2. Sugar
     3. Baking powder
   - Add wet ingredients
3. Baking
   - Pour into pan
   - Bake for 25-30 minutes
   - Cool before serving

## Lists with Multiple Paragraphs

List items can contain multiple paragraphs:

1. First item

   This is the first paragraph of the first item.

   This is the second paragraph of the first item.

2. Second item

   This item also has multiple paragraphs.

   And this is its second paragraph.

## Lists with Code Blocks

List items can contain code blocks:

1. Install Kryon:

   ```bash
   git clone https://github.com/kryon/kryon
   cd kryon
   make install
   ```

2. Run an example:

   ```bash
   kryon run examples/md/hello_world.md
   ```

## Task Lists (if supported)

- [x] Completed task
- [x] Another completed task
- [ ] Incomplete task
- [ ] Another incomplete task

## Definition Lists (if supported)

Term 1
: Definition of term 1

Term 2
: Definition of term 2
: Another definition of term 2

---

*Run with `kryon run lists.md` to see how lists render!*
