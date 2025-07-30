//! Comprehensive tests for the file upload widget

use anyhow::Result;
use widget_tests::*;
use kryon_shared::widgets::file_upload::*;
use glam::Vec2;
use serde_json::Value;
use std::collections::HashMap;

#[tokio::test]
async fn test_file_upload_basic_functionality() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let mut harness = WidgetTestHarness::new(file_upload)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 200.0),
            content_size: Vec2::new(400.0, 200.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test initial state
    assert!(harness.state().files.is_empty());
    assert_eq!(harness.state().upload_state, UploadState::Idle);
    assert!(!harness.state().is_hovered);
    assert!(!harness.state().is_focused);

    // Test that it renders successfully
    harness.assert_renders_successfully()?;

    Ok(())
}

#[tokio::test]
async fn test_file_upload_drag_and_drop() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let initial_state = FileUploadState {
        files: vec![],
        upload_state: UploadState::Idle,
        is_hovered: false,
        is_focused: false,
        drag_active: false,
        progress: HashMap::new(),
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let config = FileUploadConfig {
        accept: vec!["image/*".to_string(), ".pdf".to_string()],
        multiple: true,
        max_file_size: 10 * 1024 * 1024, // 10MB
        max_files: 5,
        drag_drop_enabled: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 200.0),
            content_size: Vec2::new(400.0, 200.0),
            scroll_offset: Vec2::ZERO,
        });

    // Simulate drag enter
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("dragenter".to_string()));
            obj.insert("files".to_string(), Value::Array(vec![
                Value::Object({
                    let mut file = serde_json::Map::new();
                    file.insert("name".to_string(), Value::String("test.jpg".to_string()));
                    file.insert("size".to_string(), Value::Number(1024.into()));
                    file.insert("type".to_string(), Value::String("image/jpeg".to_string()));
                    file
                })
            ]));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);
    assert!(harness.state().is_hovered);
    assert!(harness.state().drag_active);

    // Simulate drag leave
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("dragleave".to_string()));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);
    assert!(!harness.state().is_hovered);

    // Simulate drop
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("drop".to_string()));
            obj.insert("files".to_string(), Value::Array(vec![
                Value::Object({
                    let mut file = serde_json::Map::new();
                    file.insert("name".to_string(), Value::String("test.jpg".to_string()));
                    file.insert("size".to_string(), Value::Number(1024.into()));
                    file.insert("type".to_string(), Value::String("image/jpeg".to_string()));
                    file
                })
            ]));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);
    
    // Verify file was added
    assert_eq!(harness.state().files.len(), 1);
    assert_eq!(harness.state().files[0].name, "test.jpg");
    assert_eq!(harness.state().files[0].size, 1024);
    assert_eq!(harness.state().files[0].mime_type, "image/jpeg");

    Ok(())
}

#[tokio::test]
async fn test_file_upload_browse_selection() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let initial_state = FileUploadState {
        files: vec![],
        upload_state: UploadState::Idle,
        is_hovered: false,
        is_focused: false,
        drag_active: false,
        progress: HashMap::new(),
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let config = FileUploadConfig {
        accept: vec!["*/*".to_string()],
        multiple: false,
        max_file_size: 5 * 1024 * 1024, // 5MB
        max_files: 1,
        drag_drop_enabled: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 200.0),
            content_size: Vec2::new(400.0, 200.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click browse button
    let browse_button_x = 200.0; // Center of widget
    let browse_button_y = 100.0;
    
    let result = harness.handle_event(mouse_click(browse_button_x, browse_button_y))?;
    assert_eq!(result, EventResult::StateChanged);

    // Simulate file selection (normally would open file dialog)
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("file_selected".to_string()));
            obj.insert("file".to_string(), Value::Object({
                let mut file = serde_json::Map::new();
                file.insert("name".to_string(), Value::String("document.pdf".to_string()));
                file.insert("size".to_string(), Value::Number((2 * 1024 * 1024).into()));
                file.insert("type".to_string(), Value::String("application/pdf".to_string()));
                file
            }));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify file was selected
    assert_eq!(harness.state().files.len(), 1);
    assert_eq!(harness.state().files[0].name, "document.pdf");
    assert_eq!(harness.state().files[0].size, 2 * 1024 * 1024);

    Ok(())
}

#[tokio::test]
async fn test_file_upload_validation() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let initial_state = FileUploadState {
        files: vec![],
        upload_state: UploadState::Idle,
        is_hovered: false,
        is_focused: false,
        drag_active: false,
        progress: HashMap::new(),
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let config = FileUploadConfig {
        accept: vec!["image/*".to_string()],
        multiple: true,
        max_file_size: 1024 * 1024, // 1MB
        max_files: 2,
        drag_drop_enabled: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 200.0),
            content_size: Vec2::new(400.0, 200.0),
            scroll_offset: Vec2::ZERO,
        });

    // Try to upload file that's too large
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("drop".to_string()));
            obj.insert("files".to_string(), Value::Array(vec![
                Value::Object({
                    let mut file = serde_json::Map::new();
                    file.insert("name".to_string(), Value::String("large.jpg".to_string()));
                    file.insert("size".to_string(), Value::Number((5 * 1024 * 1024).into())); // 5MB
                    file.insert("type".to_string(), Value::String("image/jpeg".to_string()));
                    file
                })
            ]));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // File should be rejected, error should be recorded
    assert_eq!(harness.state().files.len(), 0);
    assert!(!harness.state().errors.is_empty());

    // Try to upload invalid file type
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("drop".to_string()));
            obj.insert("files".to_string(), Value::Array(vec![
                Value::Object({
                    let mut file = serde_json::Map::new();
                    file.insert("name".to_string(), Value::String("document.txt".to_string()));
                    file.insert("size".to_string(), Value::Number(1024.into()));
                    file.insert("type".to_string(), Value::String("text/plain".to_string()));
                    file
                })
            ]));
            obj
        })
    ))?;

    // Should also be rejected
    assert_eq!(harness.state().files.len(), 0);

    // Upload valid files
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("drop".to_string()));
            obj.insert("files".to_string(), Value::Array(vec![
                Value::Object({
                    let mut file = serde_json::Map::new();
                    file.insert("name".to_string(), Value::String("photo1.jpg".to_string()));
                    file.insert("size".to_string(), Value::Number((500 * 1024).into()));
                    file.insert("type".to_string(), Value::String("image/jpeg".to_string()));
                    file
                }),
                Value::Object({
                    let mut file = serde_json::Map::new();
                    file.insert("name".to_string(), Value::String("photo2.png".to_string()));
                    file.insert("size".to_string(), Value::Number((300 * 1024).into()));
                    file.insert("type".to_string(), Value::String("image/png".to_string()));
                    file
                })
            ]));
            obj
        })
    ))?;

    // Both files should be accepted
    assert_eq!(harness.state().files.len(), 2);

    // Try to add third file (exceeds max_files)
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("drop".to_string()));
            obj.insert("files".to_string(), Value::Array(vec![
                Value::Object({
                    let mut file = serde_json::Map::new();
                    file.insert("name".to_string(), Value::String("photo3.gif".to_string()));
                    file.insert("size".to_string(), Value::Number((100 * 1024).into()));
                    file.insert("type".to_string(), Value::String("image/gif".to_string()));
                    file
                })
            ]));
            obj
        })
    ))?;

    // Should still have only 2 files
    assert_eq!(harness.state().files.len(), 2);

    Ok(())
}

#[tokio::test]
async fn test_file_upload_progress_tracking() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let file_id = "file_123".to_string();
    
    let initial_state = FileUploadState {
        files: vec![
            UploadFile {
                id: file_id.clone(),
                name: "test.jpg".to_string(),
                size: 1024 * 1024, // 1MB
                mime_type: "image/jpeg".to_string(),
                status: FileStatus::Uploading,
                last_modified: std::time::SystemTime::now(),
                path: None,
                preview_url: None,
            }
        ],
        upload_state: UploadState::Uploading,
        is_hovered: false,
        is_focused: false,
        drag_active: false,
        progress: {
            let mut progress = HashMap::new();
            progress.insert(file_id.clone(), UploadProgress {
                bytes_uploaded: 256 * 1024, // 25%
                total_bytes: 1024 * 1024,
                percentage: 25.0,
                speed_bps: 1024 * 100, // 100 KB/s
                eta_seconds: Some(7),
            });
            progress
        },
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 250.0),
            content_size: Vec2::new(500.0, 250.0),
            scroll_offset: Vec2::ZERO,
        });

    // Verify progress is displayed
    let commands = harness.render()?;
    assert_render_commands!(commands, contains DrawRect); // Progress bar
    assert_render_commands!(commands, contains DrawText with "25%");

    // Update progress
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("upload_progress".to_string()));
            obj.insert("file_id".to_string(), Value::String(file_id.clone()));
            obj.insert("bytes_uploaded".to_string(), Value::Number((512 * 1024).into()));
            obj.insert("total_bytes".to_string(), Value::Number((1024 * 1024).into()));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify progress updated
    let progress = &harness.state().progress[&file_id];
    assert_eq!(progress.percentage, 50.0);

    // Complete upload
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("upload_complete".to_string()));
            obj.insert("file_id".to_string(), Value::String(file_id.clone()));
            obj.insert("url".to_string(), Value::String("https://example.com/test.jpg".to_string()));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify file status updated
    assert_eq!(harness.state().files[0].status, FileStatus::Completed);
    assert_eq!(harness.state().upload_state, UploadState::Completed);

    Ok(())
}

#[tokio::test]
async fn test_file_upload_preview_generation() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let file_id = "image_file".to_string();
    
    let initial_state = FileUploadState {
        files: vec![
            UploadFile {
                id: file_id.clone(),
                name: "photo.jpg".to_string(),
                size: 2 * 1024 * 1024,
                mime_type: "image/jpeg".to_string(),
                status: FileStatus::Pending,
                last_modified: std::time::SystemTime::now(),
                path: None,
                preview_url: None,
            }
        ],
        upload_state: UploadState::Idle,
        is_hovered: false,
        is_focused: false,
        drag_active: false,
        progress: HashMap::new(),
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let config = FileUploadConfig {
        show_preview: true,
        preview_size: 80,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 300.0),
            content_size: Vec2::new(500.0, 300.0),
            scroll_offset: Vec2::ZERO,
        });

    // Generate preview
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("generate_preview".to_string()));
            obj.insert("file_id".to_string(), Value::String(file_id.clone()));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Simulate preview generation completion
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("preview_ready".to_string()));
            obj.insert("file_id".to_string(), Value::String(file_id.clone()));
            obj.insert("preview_url".to_string(), Value::String("data:image/jpeg;base64,/9j/4AAQ...".to_string()));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify preview was cached
    assert!(harness.state().preview_cache.contains_key(&file_id));

    // Verify preview renders
    let commands = harness.render()?;
    assert_render_commands!(commands, contains DrawImage);

    Ok(())
}

#[tokio::test]
async fn test_file_upload_file_removal() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let file_id_1 = "file_1".to_string();
    let file_id_2 = "file_2".to_string();
    
    let initial_state = FileUploadState {
        files: vec![
            UploadFile {
                id: file_id_1.clone(),
                name: "file1.txt".to_string(),
                size: 1024,
                mime_type: "text/plain".to_string(),
                status: FileStatus::Pending,
                last_modified: std::time::SystemTime::now(),
                path: None,
                preview_url: None,
            },
            UploadFile {
                id: file_id_2.clone(),
                name: "file2.txt".to_string(),
                size: 2048,
                mime_type: "text/plain".to_string(),
                status: FileStatus::Completed,
                last_modified: std::time::SystemTime::now(),
                path: None,
                preview_url: None,
            }
        ],
        upload_state: UploadState::Idle,
        is_hovered: false,
        is_focused: false,
        drag_active: false,
        progress: HashMap::new(),
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let config = FileUploadConfig {
        allow_remove: true,
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 200.0),
            content_size: Vec2::new(500.0, 200.0),
            scroll_offset: Vec2::ZERO,
        });

    // Click remove button for first file
    let remove_button_x = 480.0; // Right side of widget
    let remove_button_y = 50.0;  // First file row
    
    let result = harness.handle_event(mouse_click(remove_button_x, remove_button_y))?;
    assert_eq!(result, EventResult::StateChanged);

    // First file should be removed
    assert_eq!(harness.state().files.len(), 1);
    assert_eq!(harness.state().files[0].id, file_id_2);

    // Try to remove completed file (should show confirmation)
    let result = harness.handle_event(mouse_click(remove_button_x, remove_button_y))?;
    assert_eq!(result, EventResult::StateChanged);

    // Confirm removal
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("confirm_remove".to_string()));
            obj.insert("file_id".to_string(), Value::String(file_id_2.clone()));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // All files should be removed
    assert_eq!(harness.state().files.len(), 0);

    Ok(())
}

#[tokio::test]
async fn test_file_upload_error_handling() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let file_id = "failing_file".to_string();
    
    let initial_state = FileUploadState {
        files: vec![
            UploadFile {
                id: file_id.clone(),
                name: "test.jpg".to_string(),
                size: 1024 * 1024,
                mime_type: "image/jpeg".to_string(),
                status: FileStatus::Uploading,
                last_modified: std::time::SystemTime::now(),
                path: None,
                preview_url: None,
            }
        ],
        upload_state: UploadState::Uploading,
        is_hovered: false,
        is_focused: false,
        drag_active: false,
        progress: HashMap::new(),
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(500.0, 200.0),
            content_size: Vec2::new(500.0, 200.0),
            scroll_offset: Vec2::ZERO,
        });

    // Simulate upload error
    let result = harness.handle_event(InputEvent::Custom(
        Value::Object({
            let mut obj = serde_json::Map::new();
            obj.insert("type".to_string(), Value::String("upload_error".to_string()));
            obj.insert("file_id".to_string(), Value::String(file_id.clone()));
            obj.insert("error".to_string(), Value::String("Network connection failed".to_string()));
            obj
        })
    ))?;
    assert_eq!(result, EventResult::StateChanged);

    // Verify error was recorded
    assert!(harness.state().errors.contains_key(&file_id));
    assert_eq!(harness.state().files[0].status, FileStatus::Error);

    // Verify error message renders
    let commands = harness.render()?;
    assert_render_commands!(commands, contains DrawText with "Network connection failed");

    // Test retry functionality
    let retry_button_x = 450.0;
    let retry_button_y = 50.0;
    
    let result = harness.handle_event(mouse_click(retry_button_x, retry_button_y))?;
    assert_eq!(result, EventResult::StateChanged);

    // File status should reset to pending
    assert_eq!(harness.state().files[0].status, FileStatus::Pending);
    assert!(!harness.state().errors.contains_key(&file_id));

    Ok(())
}

#[tokio::test]
async fn test_file_upload_accessibility() -> Result<()> {
    let file_upload = FileUploadWidget::new();
    let initial_state = FileUploadState {
        files: vec![],
        upload_state: UploadState::Idle,
        is_hovered: false,
        is_focused: true,
        drag_active: false,
        progress: HashMap::new(),
        errors: HashMap::new(),
        preview_cache: HashMap::new(),
    };

    let config = FileUploadConfig {
        accessibility: AccessibilityConfig {
            aria_label: "Upload files".to_string(),
            descriptions: true,
            keyboard_navigation: true,
        },
        ..Default::default()
    };

    let mut harness = WidgetTestHarness::new(file_upload)
        .with_state(initial_state)
        .with_config(config)
        .with_layout(LayoutResult {
            position: Vec2::new(0.0, 0.0),
            size: Vec2::new(400.0, 200.0),
            content_size: Vec2::new(400.0, 200.0),
            scroll_offset: Vec2::ZERO,
        });

    // Test keyboard navigation - Space or Enter to trigger file selection
    let result = harness.handle_event(key_press("Space"))?;
    assert_eq!(result, EventResult::StateChanged);

    // Test screen reader announcements
    let commands = harness.render()?;
    
    // Should include accessibility information in render commands
    // In a real implementation, this would include ARIA attributes
    assert!(commands.len() > 0);

    Ok(())
}

// Helper trait
trait FileUploadTestExt {
    fn state_mut(&mut self) -> &mut FileUploadState;
}

impl FileUploadTestExt for WidgetTestHarness<FileUploadWidget> {
    fn state_mut(&mut self) -> &mut FileUploadState {
        &mut self.state
    }
}

// Property-based tests
#[cfg(test)]
mod property_tests {
    use super::*;
    use proptest::prelude::*;

    proptest! {
        #[test]
        fn test_file_upload_handles_arbitrary_file_sizes(
            file_size in 0usize..=(100 * 1024 * 1024) // Up to 100MB
        ) {
            let file_upload = FileUploadWidget::new();
            
            let file = UploadFile {
                id: "test_file".to_string(),
                name: "test.txt".to_string(),
                size: file_size,
                mime_type: "text/plain".to_string(),
                status: FileStatus::Pending,
                last_modified: std::time::SystemTime::now(),
                path: None,
                preview_url: None,
            };
            
            let state = FileUploadState {
                files: vec![file],
                upload_state: UploadState::Idle,
                is_hovered: false,
                is_focused: false,
                drag_active: false,
                progress: HashMap::new(),
                errors: HashMap::new(),
                preview_cache: HashMap::new(),
            };

            let harness = WidgetTestHarness::new(file_upload)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(400.0, 200.0),
                    content_size: Vec2::new(400.0, 200.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any file size
            prop_assert!(harness.render().is_ok());
        }

        #[test]
        fn test_file_upload_progress_consistency(
            bytes_uploaded in 0usize..=1024,
            total_bytes in 1usize..=1024
        ) {
            let file_upload = FileUploadWidget::new();
            let file_id = "test_file".to_string();
            
            let clamped_uploaded = bytes_uploaded.min(total_bytes);
            let percentage = (clamped_uploaded as f32 / total_bytes as f32) * 100.0;
            
            let mut progress = HashMap::new();
            progress.insert(file_id.clone(), UploadProgress {
                bytes_uploaded: clamped_uploaded,
                total_bytes,
                percentage,
                speed_bps: 1000,
                eta_seconds: Some(10),
            });
            
            let state = FileUploadState {
                files: vec![
                    UploadFile {
                        id: file_id,
                        name: "test.txt".to_string(),
                        size: total_bytes,
                        mime_type: "text/plain".to_string(),
                        status: FileStatus::Uploading,
                        last_modified: std::time::SystemTime::now(),
                        path: None,
                        preview_url: None,
                    }
                ],
                upload_state: UploadState::Uploading,
                is_hovered: false,
                is_focused: false,
                drag_active: false,
                progress,
                errors: HashMap::new(),
                preview_cache: HashMap::new(),
            };

            let harness = WidgetTestHarness::new(file_upload)
                .with_state(state)
                .with_layout(LayoutResult {
                    position: Vec2::new(0.0, 0.0),
                    size: Vec2::new(400.0, 200.0),
                    content_size: Vec2::new(400.0, 200.0),
                    scroll_offset: Vec2::ZERO,
                });

            // Should handle any progress values
            prop_assert!(harness.render().is_ok());
            
            // Progress should be consistent
            let state = harness.state();
            for (_, progress) in &state.progress {
                prop_assert!(progress.bytes_uploaded <= progress.total_bytes);
                prop_assert!(progress.percentage >= 0.0 && progress.percentage <= 100.0);
            }
        }
    }
}