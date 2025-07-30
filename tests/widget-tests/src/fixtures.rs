//! Test fixtures and data generators for widget testing
//! 
//! Provides reusable test data, configuration presets, and utilities
//! for generating realistic test scenarios for widget testing.

use serde_json::Value;
use std::collections::HashMap;
use rand::{Rng, SeedableRng};
use rand::rngs::StdRng;
use glam::{Vec2, Vec4};

/// Test data generator with deterministic random values
pub struct TestDataGenerator {
    rng: StdRng,
}

impl TestDataGenerator {
    /// Create a new generator with a seed
    pub fn new(seed: u64) -> Self {
        Self {
            rng: StdRng::seed_from_u64(seed),
        }
    }

    /// Generate random text of specified length
    pub fn generate_text(&mut self, min_length: usize, max_length: usize) -> String {
        let words = vec![
            "Lorem", "ipsum", "dolor", "sit", "amet", "consectetur", 
            "adipiscing", "elit", "sed", "do", "eiusmod", "tempor",
            "incididunt", "ut", "labore", "et", "dolore", "magna",
        ];
        
        let length = self.rng.gen_range(min_length..=max_length);
        let mut result = String::new();
        
        while result.len() < length {
            if !result.is_empty() {
                result.push(' ');
            }
            let word = words[self.rng.gen_range(0..words.len())];
            result.push_str(word);
        }
        
        result.truncate(length);
        result
    }

    /// Generate random number
    pub fn generate_number(&mut self, min: f64, max: f64) -> f64 {
        self.rng.gen_range(min..=max)
    }

    /// Generate random color
    pub fn generate_color(&mut self) -> Vec4 {
        Vec4::new(
            self.rng.gen(),
            self.rng.gen(),
            self.rng.gen(),
            1.0,
        )
    }

    /// Generate random boolean
    pub fn generate_bool(&mut self) -> bool {
        self.rng.gen()
    }

    /// Generate random date string
    pub fn generate_date(&mut self) -> String {
        let year = self.rng.gen_range(2020..=2025);
        let month = self.rng.gen_range(1..=12);
        let day = self.rng.gen_range(1..=28);
        format!("{:04}-{:02}-{:02}", year, month, day)
    }

    /// Generate random email
    pub fn generate_email(&mut self) -> String {
        let names = vec!["john", "jane", "bob", "alice", "charlie", "diana"];
        let domains = vec!["example.com", "test.org", "demo.net", "sample.io"];
        
        let name = names[self.rng.gen_range(0..names.len())];
        let number = self.rng.gen_range(1..=999);
        let domain = domains[self.rng.gen_range(0..domains.len())];
        
        format!("{}{}@{}", name, number, domain)
    }

    /// Generate random phone number
    pub fn generate_phone(&mut self) -> String {
        format!(
            "+1-{:03}-{:03}-{:04}",
            self.rng.gen_range(200..=999),
            self.rng.gen_range(200..=999),
            self.rng.gen_range(1000..=9999)
        )
    }

    /// Generate random JSON value
    pub fn generate_json_value(&mut self, depth: usize) -> Value {
        if depth == 0 {
            // Leaf values only
            match self.rng.gen_range(0..4) {
                0 => Value::String(self.generate_text(5, 20)),
                1 => Value::Number(serde_json::Number::from_f64(self.generate_number(-100.0, 100.0)).unwrap()),
                2 => Value::Bool(self.generate_bool()),
                _ => Value::Null,
            }
        } else {
            // Can generate nested structures
            match self.rng.gen_range(0..6) {
                0 => Value::String(self.generate_text(5, 20)),
                1 => Value::Number(serde_json::Number::from_f64(self.generate_number(-100.0, 100.0)).unwrap()),
                2 => Value::Bool(self.generate_bool()),
                3 => Value::Null,
                4 => {
                    // Array
                    let size = self.rng.gen_range(0..5);
                    let mut arr = Vec::new();
                    for _ in 0..size {
                        arr.push(self.generate_json_value(depth - 1));
                    }
                    Value::Array(arr)
                }
                _ => {
                    // Object
                    let size = self.rng.gen_range(0..5);
                    let mut obj = serde_json::Map::new();
                    for i in 0..size {
                        obj.insert(
                            format!("field_{}", i),
                            self.generate_json_value(depth - 1)
                        );
                    }
                    Value::Object(obj)
                }
            }
        }
    }
}

/// Common test fixtures for widgets
pub struct WidgetTestFixtures;

impl WidgetTestFixtures {
    /// Get standard dropdown items
    pub fn dropdown_items() -> Vec<(String, String, Value)> {
        vec![
            ("usa".to_string(), "United States".to_string(), Value::String("US".to_string())),
            ("uk".to_string(), "United Kingdom".to_string(), Value::String("UK".to_string())),
            ("canada".to_string(), "Canada".to_string(), Value::String("CA".to_string())),
            ("australia".to_string(), "Australia".to_string(), Value::String("AU".to_string())),
            ("germany".to_string(), "Germany".to_string(), Value::String("DE".to_string())),
            ("france".to_string(), "France".to_string(), Value::String("FR".to_string())),
            ("japan".to_string(), "Japan".to_string(), Value::String("JP".to_string())),
            ("brazil".to_string(), "Brazil".to_string(), Value::String("BR".to_string())),
        ]
    }

    /// Get sample data grid data
    pub fn data_grid_sample_data() -> Vec<HashMap<String, Value>> {
        vec![
            {
                let mut row = HashMap::new();
                row.insert("id".to_string(), Value::Number(1.into()));
                row.insert("name".to_string(), Value::String("John Doe".to_string()));
                row.insert("email".to_string(), Value::String("john@example.com".to_string()));
                row.insert("age".to_string(), Value::Number(30.into()));
                row.insert("active".to_string(), Value::Bool(true));
                row
            },
            {
                let mut row = HashMap::new();
                row.insert("id".to_string(), Value::Number(2.into()));
                row.insert("name".to_string(), Value::String("Jane Smith".to_string()));
                row.insert("email".to_string(), Value::String("jane@example.com".to_string()));
                row.insert("age".to_string(), Value::Number(25.into()));
                row.insert("active".to_string(), Value::Bool(true));
                row
            },
            {
                let mut row = HashMap::new();
                row.insert("id".to_string(), Value::Number(3.into()));
                row.insert("name".to_string(), Value::String("Bob Johnson".to_string()));
                row.insert("email".to_string(), Value::String("bob@example.com".to_string()));
                row.insert("age".to_string(), Value::Number(35.into()));
                row.insert("active".to_string(), Value::Bool(false));
                row
            },
        ]
    }

    /// Get sample rich text content
    pub fn rich_text_sample() -> String {
        r#"# Sample Document

This is a **sample document** with various formatting options.

## Features

- **Bold text** for emphasis
- *Italic text* for style
- `Code snippets` for technical content
- [Links](https://example.com) for navigation

### Code Example

```rust
fn main() {
    println!("Hello, world!");
}
```

> This is a blockquote with some interesting content.

1. First item
2. Second item
3. Third item

---

End of document."#.to_string()
    }

    /// Get color palette
    pub fn color_palette() -> Vec<(String, Vec4)> {
        vec![
            ("Red".to_string(), Vec4::new(1.0, 0.0, 0.0, 1.0)),
            ("Green".to_string(), Vec4::new(0.0, 1.0, 0.0, 1.0)),
            ("Blue".to_string(), Vec4::new(0.0, 0.0, 1.0, 1.0)),
            ("Yellow".to_string(), Vec4::new(1.0, 1.0, 0.0, 1.0)),
            ("Purple".to_string(), Vec4::new(0.5, 0.0, 0.5, 1.0)),
            ("Orange".to_string(), Vec4::new(1.0, 0.5, 0.0, 1.0)),
            ("Black".to_string(), Vec4::new(0.0, 0.0, 0.0, 1.0)),
            ("White".to_string(), Vec4::new(1.0, 1.0, 1.0, 1.0)),
        ]
    }

    /// Get file upload test files
    pub fn file_upload_test_files() -> Vec<(String, String, usize)> {
        vec![
            ("document.pdf".to_string(), "application/pdf".to_string(), 1024 * 100),
            ("image.jpg".to_string(), "image/jpeg".to_string(), 1024 * 500),
            ("spreadsheet.xlsx".to_string(), "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet".to_string(), 1024 * 200),
            ("text.txt".to_string(), "text/plain".to_string(), 1024 * 10),
            ("presentation.pptx".to_string(), "application/vnd.openxmlformats-officedocument.presentationml.presentation".to_string(), 1024 * 800),
        ]
    }

    /// Get standard layout configurations
    pub fn layout_configurations() -> Vec<(String, Vec2, Vec2)> {
        vec![
            ("Small Widget".to_string(), Vec2::new(200.0, 100.0), Vec2::new(200.0, 100.0)),
            ("Medium Widget".to_string(), Vec2::new(400.0, 300.0), Vec2::new(400.0, 300.0)),
            ("Large Widget".to_string(), Vec2::new(800.0, 600.0), Vec2::new(800.0, 600.0)),
            ("Full Screen".to_string(), Vec2::new(1920.0, 1080.0), Vec2::new(1920.0, 1080.0)),
            ("Mobile Portrait".to_string(), Vec2::new(375.0, 812.0), Vec2::new(375.0, 812.0)),
            ("Mobile Landscape".to_string(), Vec2::new(812.0, 375.0), Vec2::new(812.0, 375.0)),
            ("Tablet".to_string(), Vec2::new(768.0, 1024.0), Vec2::new(768.0, 1024.0)),
        ]
    }
}

/// Configuration presets for widgets
pub struct ConfigPresets;

impl ConfigPresets {
    /// Get minimal widget configuration
    pub fn minimal() -> HashMap<String, Value> {
        HashMap::new()
    }

    /// Get default widget configuration
    pub fn default() -> HashMap<String, Value> {
        let mut config = HashMap::new();
        config.insert("enabled".to_string(), Value::Bool(true));
        config.insert("visible".to_string(), Value::Bool(true));
        config
    }

    /// Get feature-rich configuration
    pub fn feature_rich() -> HashMap<String, Value> {
        let mut config = HashMap::new();
        config.insert("enabled".to_string(), Value::Bool(true));
        config.insert("visible".to_string(), Value::Bool(true));
        config.insert("searchable".to_string(), Value::Bool(true));
        config.insert("sortable".to_string(), Value::Bool(true));
        config.insert("filterable".to_string(), Value::Bool(true));
        config.insert("editable".to_string(), Value::Bool(true));
        config.insert("multi_select".to_string(), Value::Bool(true));
        config.insert("async_loading".to_string(), Value::Bool(true));
        config.insert("virtual_scrolling".to_string(), Value::Bool(true));
        config.insert("keyboard_navigation".to_string(), Value::Bool(true));
        config.insert("animations_enabled".to_string(), Value::Bool(true));
        config
    }

    /// Get performance-optimized configuration
    pub fn performance_optimized() -> HashMap<String, Value> {
        let mut config = HashMap::new();
        config.insert("enabled".to_string(), Value::Bool(true));
        config.insert("visible".to_string(), Value::Bool(true));
        config.insert("virtual_scrolling".to_string(), Value::Bool(true));
        config.insert("lazy_loading".to_string(), Value::Bool(true));
        config.insert("debounce_ms".to_string(), Value::Number(300.into()));
        config.insert("animations_enabled".to_string(), Value::Bool(false));
        config.insert("cache_enabled".to_string(), Value::Bool(true));
        config
    }

    /// Get accessibility-focused configuration
    pub fn accessibility_focused() -> HashMap<String, Value> {
        let mut config = HashMap::new();
        config.insert("enabled".to_string(), Value::Bool(true));
        config.insert("visible".to_string(), Value::Bool(true));
        config.insert("keyboard_navigation".to_string(), Value::Bool(true));
        config.insert("screen_reader_support".to_string(), Value::Bool(true));
        config.insert("high_contrast".to_string(), Value::Bool(true));
        config.insert("focus_indicators".to_string(), Value::Bool(true));
        config.insert("aria_labels".to_string(), Value::Bool(true));
        config
    }
}

/// Test scenario builder for complex test cases
pub struct TestScenarioBuilder {
    name: String,
    description: String,
    steps: Vec<TestStep>,
}

impl TestScenarioBuilder {
    pub fn new(name: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            description: String::new(),
            steps: Vec::new(),
        }
    }

    pub fn description(mut self, desc: impl Into<String>) -> Self {
        self.description = desc.into();
        self
    }

    pub fn add_step(mut self, step: TestStep) -> Self {
        self.steps.push(step);
        self
    }

    pub fn build(self) -> TestScenario {
        TestScenario {
            name: self.name,
            description: self.description,
            steps: self.steps,
        }
    }
}

#[derive(Debug, Clone)]
pub struct TestScenario {
    pub name: String,
    pub description: String,
    pub steps: Vec<TestStep>,
}

#[derive(Debug, Clone)]
pub enum TestStep {
    SetState { state_json: String },
    TriggerEvent { event_type: String, params: HashMap<String, Value> },
    AssertState { field: String, expected: Value },
    AssertRenderCommand { command_type: String, count: Option<usize> },
    Wait { milliseconds: u64 },
    Custom { action: String, data: Value },
}

/// Common test scenarios
pub struct CommonScenarios;

impl CommonScenarios {
    /// User completes a form
    pub fn form_completion() -> TestScenario {
        TestScenarioBuilder::new("Form Completion")
            .description("User fills out and submits a form")
            .add_step(TestStep::TriggerEvent {
                event_type: "focus".to_string(),
                params: HashMap::new(),
            })
            .add_step(TestStep::TriggerEvent {
                event_type: "text_input".to_string(),
                params: {
                    let mut params = HashMap::new();
                    params.insert("text".to_string(), Value::String("John Doe".to_string()));
                    params
                },
            })
            .add_step(TestStep::TriggerEvent {
                event_type: "tab".to_string(),
                params: HashMap::new(),
            })
            .add_step(TestStep::TriggerEvent {
                event_type: "text_input".to_string(),
                params: {
                    let mut params = HashMap::new();
                    params.insert("text".to_string(), Value::String("john@example.com".to_string()));
                    params
                },
            })
            .add_step(TestStep::TriggerEvent {
                event_type: "submit".to_string(),
                params: HashMap::new(),
            })
            .build()
    }

    /// User searches and filters data
    pub fn search_and_filter() -> TestScenario {
        TestScenarioBuilder::new("Search and Filter")
            .description("User searches for specific data and applies filters")
            .add_step(TestStep::TriggerEvent {
                event_type: "focus_search".to_string(),
                params: HashMap::new(),
            })
            .add_step(TestStep::TriggerEvent {
                event_type: "text_input".to_string(),
                params: {
                    let mut params = HashMap::new();
                    params.insert("text".to_string(), Value::String("test".to_string()));
                    params
                },
            })
            .add_step(TestStep::Wait { milliseconds: 300 })
            .add_step(TestStep::AssertState {
                field: "filtered_items".to_string(),
                expected: Value::Array(vec![]),
            })
            .build()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_data_generator() {
        let mut gen = TestDataGenerator::new(42);
        
        let text = gen.generate_text(10, 20);
        assert!(text.len() >= 10 && text.len() <= 20);
        
        let number = gen.generate_number(0.0, 100.0);
        assert!(number >= 0.0 && number <= 100.0);
        
        let color = gen.generate_color();
        assert!(color.x >= 0.0 && color.x <= 1.0);
        assert!(color.w == 1.0);
        
        let email = gen.generate_email();
        assert!(email.contains('@'));
        
        let json = gen.generate_json_value(2);
        // Should be valid JSON
        assert!(serde_json::to_string(&json).is_ok());
    }

    #[test]
    fn test_fixtures() {
        let dropdown_items = WidgetTestFixtures::dropdown_items();
        assert!(!dropdown_items.is_empty());
        
        let grid_data = WidgetTestFixtures::data_grid_sample_data();
        assert_eq!(grid_data.len(), 3);
        
        let colors = WidgetTestFixtures::color_palette();
        assert_eq!(colors.len(), 8);
    }

    #[test]
    fn test_config_presets() {
        let minimal = ConfigPresets::minimal();
        assert!(minimal.is_empty());
        
        let default = ConfigPresets::default();
        assert_eq!(default.get("enabled"), Some(&Value::Bool(true)));
        
        let feature_rich = ConfigPresets::feature_rich();
        assert!(feature_rich.len() > 5);
    }

    #[test]
    fn test_scenario_builder() {
        let scenario = TestScenarioBuilder::new("Test Scenario")
            .description("A test scenario")
            .add_step(TestStep::Wait { milliseconds: 100 })
            .build();
        
        assert_eq!(scenario.name, "Test Scenario");
        assert_eq!(scenario.steps.len(), 1);
    }
}