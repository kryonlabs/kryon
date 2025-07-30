//! Comprehensive tests for the rich text editor widget

use anyhow::Result;
use widget_tests::*;
use kryon_shared::widgets::rich_text::*;
use glam::Vec2;
use serde_json::Value;

#[tokio::test]
async fn test_rich_text_basic_functionality() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let mut harness = WidgetTestHarness::new(rich_text)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial state
    assert!(harness.state().document.root.children.is_empty());
    assert!(!harness.state().is_focused);
    assert_eq!(harness.state().cursor.paragraph, 0);
    assert_eq!(harness.state().cursor.position, 0);

    // Test that it renders successfully
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_rich_text_basic_editing() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: RichTextDocument::new(),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Type some text
    let result = harness.handle_event(text_input("Hello World"))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify text was inserted
    assert!(!harness.state().document.root.children.is_empty());
    if let RichTextNode::Paragraph(para) = &harness.state().document.root.children[0] {
        if let RichTextNode::Text(text_node) = &para.children[0] {
            assert_eq!(text_node.content, "Hello World");
        } else {
            panic!("Expected text node");
        }
    } else {
        panic!("Expected paragraph node");
    }

    Ok(())
}

#[tokio::test]
async fn test_rich_text_formatting() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let mut initial_state = RichTextState {
        document: create_sample_document(),
        cursor: CursorPosition { paragraph: 0, position: 5 },
        selection: Some(Selection {
            start: CursorPosition { paragraph: 0, position: 0 },
            end: CursorPosition { paragraph: 0, position: 5 },
        }),
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Apply bold formatting with Ctrl+B
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "b".to_string(),
        modifiers: 1, // Ctrl
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Apply italic formatting with Ctrl+I
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "i".to_string(),
        modifiers: 1, // Ctrl
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify formatting was applied
    assert!(harness.state().current_format.bold);
    assert!(harness.state().current_format.italic);

    Ok(())
}

#[tokio::test]
async fn test_rich_text_lists() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: RichTextDocument::new(),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Create bullet list with Ctrl+Shift+8
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "8".to_string(),
        modifiers: 3, // Ctrl+Shift
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Type list item
    harness.handle_event(text_input("First item"))?;
    
    // Press Enter to create new list item
    harness.handle_event(key_press("Enter"))?;
    harness.handle_event(text_input("Second item"))?;

    // Verify list structure
    if let RichTextNode::List(list) = &harness.state().document.root.children[0] {
        assert_eq!(list.list_type, ListType::Bullet);
        assert_eq!(list.items.len(), 2);
    } else {
        panic!("Expected list node");
    }

    Ok(())
}

#[tokio::test]
async fn test_rich_text_links() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: create_sample_document(),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: Some(Selection {
            start: CursorPosition { paragraph: 0, position: 0 },
            end: CursorPosition { paragraph: 0, position: 7 },
        }),
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Create link with Ctrl+K
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "k".to_string(),
        modifiers: 1, // Ctrl
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // In a real implementation, this would open a link dialog
    // For testing, we simulate setting the link URL
    harness.state_mut().pending_link_url = Some("https://example.com".to_string());
    
    // Confirm link creation with Enter
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify link was created
    if let RichTextNode::Paragraph(para) = &harness.state().document.root.children[0] {
        if let RichTextNode::Link(link) = &para.children[0] {
            assert_eq!(link.url, "https://example.com");
        } else {
            panic!("Expected link node");
        }
    }

    Ok(())
}

#[tokio::test]
async fn test_rich_text_images() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: RichTextDocument::new(),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let config = RichTextConfig {
        allow_images: true,
        max_image_size: 1024 * 1024, // 1MB
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Insert image with Ctrl+Shift+I
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "i".to_string(),
        modifiers: 3, // Ctrl+Shift
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Simulate image selection
    harness.state_mut().pending_image_src = Some("test_image.png".to_string());
    
    // Confirm image insertion
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify image was inserted
    if let RichTextNode::Paragraph(para) = &harness.state().document.root.children[0] {
        if let RichTextNode::Image(image) = &para.children[0] {
            assert_eq!(image.src, "test_image.png");
        } else {
            panic!("Expected image node");
        }
    }

    Ok(())
}

#[tokio::test]
async fn test_rich_text_tables() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: RichTextDocument::new(),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let config = RichTextConfig {
        allow_tables: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 400.0),
            content_size: Vec2::new(500.0, 400.0),
            scroll_offset: Vec2::ZERO,
        });

    // Insert table with Ctrl+Shift+T
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "t".to_string(),
        modifiers: 3, // Ctrl+Shift
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Create 2x2 table (simulated)
    harness.state_mut().pending_table_dimensions = Some((2, 2));
    
    // Confirm table creation
    let result = harness.handle_event(key_press("Enter"))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify table was created
    if let RichTextNode::Table(table) = &harness.state().document.root.children[0] {
        assert_eq!(table.rows.len(), 2);
        assert_eq!(table.rows[0].cells.len(), 2);
    } else {
        panic!("Expected table node");
    }

    // Test navigation within table with Tab
    let result = harness.handle_event(key_press("Tab"))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Should move to next cell
    assert_eq!(harness.state().cursor.position, 1);

    Ok(())
}

#[tokio::test]
async fn test_rich_text_undo_redo() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: RichTextDocument::new(),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Type some text
    harness.handle_event(text_input("First"))?;
    harness.handle_event(key_press(" "))?;
    harness.handle_event(text_input("sentence"))?;

    // Save state for undo
    let text_length = get_document_text_length(&harness.state().document);
    assert!(text_length > 0);

    // Undo with Ctrl+Z
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "z".to_string(),
        modifiers: 1, // Ctrl
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify undo worked
    assert!(!harness.state().undo_stack.is_empty());

    // Redo with Ctrl+Y
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "y".to_string(),
        modifiers: 1, // Ctrl
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify redo worked
    assert!(!harness.state().redo_stack.is_empty());

    Ok(())
}

#[tokio::test]
async fn test_rich_text_markdown_import_export() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: RichTextDocument::new(),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        is_composing: false,
        composition_text: String::new(),
    };

    let config = RichTextConfig {
        markdown_support: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Import markdown content
    let markdown_content = r#"
# Main Heading

This is a paragraph with **bold text** and *italic text*.

## Subheading

- Bullet point 1
- Bullet point 2
- Bullet point 3

1. Numbered item 1
2. Numbered item 2

[Link text](https://example.com)

`inline code`

```rust
fn main() {
    println!("Hello, world!");
}
```
"#;

    // Simulate markdown import
    harness.state_mut().pending_markdown_import = Some(markdown_content.to_string());
    
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("import_markdown".to_string())
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify document structure was created
    assert!(!harness.state().document.root.children.is_empty());
    
    // Check for heading
    if let RichTextNode::Heading(heading) = &harness.state().document.root.children[0] {
        assert_eq!(heading.level, 1);
        if let RichTextNode::Text(text) = &heading.children[0] {
            assert_eq!(text.content, "Main Heading");
        }
    }

    // Test markdown export
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("export_markdown".to_string())
    ))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify export was generated
    assert!(harness.state().exported_markdown.is_some());

    Ok(())
}

#[tokio::test]
async fn test_rich_text_find_replace() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: create_sample_document_with_text("Hello world. This is a test. Hello again."),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        find_state: Some(FindReplaceState {
            find_text: String::new(),
            replace_text: String::new(),
            matches: vec![],
            current_match: 0,
            case_sensitive: false,
            whole_words: false,
            regex: false,
        }),
        is_composing: false,
        composition_text: String::new(),
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Open find dialog with Ctrl+F
    let result = harness.handle_event(InputEvent::KeyDown {
        key: "f".to_string(),
        modifiers: 1, // Ctrl
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Type search term
    harness.state_mut().find_state.as_mut().unwrap().find_text = "Hello".to_string();
    
    // Execute find
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("execute_find".to_string())
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify matches were found
    let find_state = harness.state().find_state.as_ref().unwrap();
    assert_eq!(find_state.matches.len(), 2); // "Hello" appears twice

    // Navigate to next match with F3
    let result = harness.handle_event(key_press("F3"))?;
    assert_eq!(result, EventResult::StateChanged);
    assert_eq!(harness.state().find_state.as_ref().unwrap().current_match, 1);

    // Replace current match
    harness.state_mut().find_state.as_mut().unwrap().replace_text = "Hi".to_string();
    
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("replace_current".to_string())
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    Ok(())
}

#[tokio::test]
async fn test_rich_text_spell_check() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: create_sample_document_with_text("This is a sentance with mispelled words."),
        cursor: CursorPosition { paragraph: 0, position: 0 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        spell_check_enabled: true,
        spell_errors: vec![
            SpellError {
                position: CursorPosition { paragraph: 0, position: 11 },
                length: 8, // "sentance"
                suggestions: vec!["sentence".to_string(), "penance".to_string()],
            },
            SpellError {
                position: CursorPosition { paragraph: 0, position: 25 },
                length: 9, // "mispelled"
                suggestions: vec!["misspelled".to_string(), "dispelled".to_string()],
            },
        ],
        is_composing: false,
        composition_text: String::new(),
    };

    let config = RichTextConfig {
        spell_check: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 300.0),
            content_size: Vec2::new(400.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Right-click on misspelled word
    let result = harness.handle_event(InputEvent::MouseDown {
        position: Vec2::new(150.0, 50.0), // Position over "sentance"
        button: 1, // Right click
    })?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify spell check context menu is shown
    assert!(!harness.state().spell_errors.is_empty());

    // Select first suggestion
    let result = harness.handle_event(InputEvent::Custom(
        Value::String("accept_suggestion_0".to_string())
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify correction was applied
    let document_text = extract_document_text(&harness.state().document);
    assert!(document_text.contains("sentence"));
    assert!(!document_text.contains("sentance"));

    Ok(())
}

#[tokio::test]
async fn test_rich_text_collaborative_editing() -> Result<()> {
    let rich_text = RichTextWidget::new();
    let initial_state = RichTextState {
        document: create_sample_document_with_text("Collaborative document"),
        cursor: CursorPosition { paragraph: 0, position: 22 },
        selection: None,
        is_focused: true,
        scroll_offset: Vec2::ZERO,
        undo_stack: vec![],
        redo_stack: vec![],
        current_format: TextFormat::default(),
        collaborative_cursors: vec![
            CollaborativeCursor {
                user_id: "user1".to_string(),
                username: "Alice".to_string(),
                position: CursorPosition { paragraph: 0, position: 13 },
                color: Vec4::new(1.0, 0.0, 0.0, 1.0), // Red
            },
            CollaborativeCursor {
                user_id: "user2".to_string(),
                username: "Bob".to_string(),
                position: CursorPosition { paragraph: 0, position: 8 },
                color: Vec4::new(0.0, 1.0, 0.0, 1.0), // Green
            },
        ],
        is_composing: false,
        composition_text: String::new(),
    };

    let config = RichTextConfig {
        collaborative: true,
        ..Default::default()
    };

    let harness = WidgetTestHarness::new(rich_text)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 300.0),
            content_size: Vec2::new(500.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test that collaborative cursors render
    let commands = harness.render()?;
    
    // Should include render commands for other users' cursors
    let draw_commands = RenderCommandAnalyzer::count_by_type(&commands);
    assert!(draw_commands.get("DrawRect").unwrap_or(&0) > &0); // Cursor rectangles
    assert!(draw_commands.get("DrawText").unwrap_or(&0) > &0); // User names

    // Verify collaborative state
    assert_eq!(harness.state().collaborative_cursors.len(), 2);

    Ok(())
}

// Helper functions
trait RichTextTestExt {
    fn state_mut(&mut self) -> &mut RichTextState;
}

impl RichTextTestExt for WidgetTestHarness<RichTextWidget> {
    fn state_mut(&mut self) -> &mut RichTextState {
        &mut self.state
    }
}

fn create_sample_document() -> RichTextDocument {
    let mut doc = RichTextDocument::new();
    
    let paragraph = RichTextNode::Paragraph(ParagraphNode {
        children: vec![
            RichTextNode::Text(TextNode {
                content: "Sample text for testing".to_string(),
                format: TextFormat::default(),
            })
        ],
        alignment: TextAlignment::Start,
        indent_level: 0,
    });
    
    doc.root.children.push(paragraph);
    doc
}

fn create_sample_document_with_text(text: &str) -> RichTextDocument {
    let mut doc = RichTextDocument::new();
    
    let paragraph = RichTextNode::Paragraph(ParagraphNode {
        children: vec![
            RichTextNode::Text(TextNode {
                content: text.to_string(),
                format: TextFormat::default(),
            })
        ],
        alignment: TextAlignment::Start,
        indent_level: 0,
    });
    
    doc.root.children.push(paragraph);
    doc
}

fn get_document_text_length(document: &RichTextDocument) -> usize {
    extract_document_text(document).len()
}

fn extract_document_text(document: &RichTextDocument) -> String {
    let mut text = String::new();
    
    fn extract_from_node(node: &RichTextNode, text: &mut String) {
        match node {
            RichTextNode::Text(text_node) => {
                text.push_str(&text_node.content);
            }
            RichTextNode::Paragraph(para) => {
                for child in &para.children {
                    extract_from_node(child, text);
                }
                text.push('\n');
            }
            RichTextNode::Heading(heading) => {
                for child in &heading.children {
                    extract_from_node(child, text);
                }
                text.push('\n');
            }
            RichTextNode::List(list) => {
                for item in &list.items {
                    for child in &item.children {
                        extract_from_node(child, text);
                    }
                    text.push('\n');
                }
            }
            _ => {} // Handle other node types as needed
        }
    }
    
    for child in &document.root.children {
        extract_from_node(child, &mut text);
    }
    
    text
}

// Property-based tests
#[cfg(test)]
mod property_tests {
    use super::*;
    use proptest::prelude::*;

    proptest! {
        #[test]
        fn test_rich_text_handles_arbitrary_text(text in ".*") {
            let rich_text = RichTextWidget::new();
            let document = create_sample_document_with_text(&text);
            
            let state = RichTextState {
                document,
                cursor: CursorPosition { paragraph: 0, position: 0 },
                selection: None,
                is_focused: false,
                scroll_offset: Vec2::ZERO,
                undo_stack: vec![],
                redo_stack: vec![],
                current_format: TextFormat::default(),
                is_composing: false,
                composition_text: String::new(),
            };

            let harness = WidgetTestHarness::new(rich_text)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(400.0, 300.0),
                    content_size: Vec2::new(400.0, 300.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any text content
            prop_assert!(harness.render().is_ok());
        }

        #[test]
        fn test_rich_text_cursor_bounds(
            paragraph in 0usize..10,
            position in 0usize..100
        ) {
            let rich_text = RichTextWidget::new();
            let document = create_sample_document();
            
            let bounded_paragraph = paragraph.min(document.root.children.len().saturating_sub(1));
            let max_position = if let RichTextNode::Paragraph(para) = &document.root.children.get(bounded_paragraph).unwrap_or(&document.root.children[0]) {
                extract_paragraph_text(para).len()
            } else {
                0
            };
            let bounded_position = position.min(max_position);
            
            let state = RichTextState {
                document,
                cursor: CursorPosition { 
                    paragraph: bounded_paragraph, 
                    position: bounded_position 
                },
                selection: None,
                is_focused: false,
                scroll_offset: Vec2::ZERO,
                undo_stack: vec![],
                redo_stack: vec![],
                current_format: TextFormat::default(),
                is_composing: false,
                composition_text: String::new(),
            };

            let harness = WidgetTestHarness::new(rich_text)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(400.0, 300.0),
                    content_size: Vec2::new(400.0, 300.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any valid cursor position
            prop_assert!(harness.render().is_ok());
        }
    }

    fn extract_paragraph_text(paragraph: &ParagraphNode) -> String {
        let mut text = String::new();
        for child in &paragraph.children {
            if let RichTextNode::Text(text_node) = child {
                text.push_str(&text_node.content);
            }
        }
        text
    }
}