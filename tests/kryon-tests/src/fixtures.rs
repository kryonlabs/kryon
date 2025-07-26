//! Test fixtures and sample data management

use crate::prelude::*;
use std::collections::BTreeMap;

/// Test fixture manager for organizing test data
#[derive(Debug, Clone)]
pub struct FixtureManager {
    fixtures: BTreeMap<String, TestFixture>,
    base_path: PathBuf,
}

/// A test fixture containing source code and expected results
#[derive(Debug, Clone)]
pub struct TestFixture {
    pub name: String,
    pub source: String,
    pub expected_elements: Option<usize>,
    pub expected_properties: Vec<(String, PropertyValue)>,
    pub expected_errors: Vec<String>,
    pub description: String,
    pub category: FixtureCategory,
}

/// Categories for organizing test fixtures
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FixtureCategory {
    Basic,
    Layout,
    Styling,
    Components,
    Theming,
    Scripting,
    Performance,
    ErrorCases,
}

impl FixtureManager {
    pub fn new(base_path: impl Into<PathBuf>) -> Self {
        let mut manager = Self {
            fixtures: BTreeMap::new(),
            base_path: base_path.into(),
        };
        manager.load_default_fixtures();
        manager
    }

    pub fn add_fixture(&mut self, fixture: TestFixture) {
        self.fixtures.insert(fixture.name.clone(), fixture);
    }

    pub fn get_fixture(&self, name: &str) -> Option<&TestFixture> {
        self.fixtures.get(name)
    }

    pub fn get_fixtures_by_category(&self, category: FixtureCategory) -> Vec<&TestFixture> {
        self.fixtures.values()
            .filter(|f| f.category == category)
            .collect()
    }

    pub fn all_fixtures(&self) -> impl Iterator<Item = &TestFixture> {
        self.fixtures.values()
    }

    fn load_default_fixtures(&mut self) {
        // Basic app fixture
        self.add_fixture(TestFixture {
            name: "basic_app".to_string(),
            source: r#"
App {
    window_width: 800
    window_height: 600
    Text { text: "Hello World" }
}
"#.to_string(),
            expected_elements: Some(2),
            expected_properties: vec![
                ("window_width".to_string(), PropertyValue::Integer(800)),
                ("window_height".to_string(), PropertyValue::Integer(600)),
            ],
            expected_errors: vec![],
            description: "Basic application with text element".to_string(),
            category: FixtureCategory::Basic,
        });

        // Flexbox layout fixture
        self.add_fixture(TestFixture {
            name: "flex_layout".to_string(),
            source: r#"
App {
    Container {
        layout: "row"
        justify_content: "space-between"
        align_items: "center"
        
        Button { text: "Start" }
        Button { text: "Middle" }
        Button { text: "End" }
    }
}
"#.to_string(),
            expected_elements: Some(5),
            expected_properties: vec![
                ("layout".to_string(), PropertyValue::String("row".to_string())),
                ("justify_content".to_string(), PropertyValue::String("space-between".to_string())),
                ("align_items".to_string(), PropertyValue::String("center".to_string())),
            ],
            expected_errors: vec![],
            description: "Flexbox layout with space-between distribution".to_string(),
            category: FixtureCategory::Layout,
        });

        // Theme fixture
        self.add_fixture(TestFixture {
            name: "themed_app".to_string(),
            source: r#"
Theme default {
    primary_color: "#3b82f6"
    background_color: "#ffffff"
    text_size: 16
}

App {
    background_color: ${theme.background_color}
    Text {
        text: "Themed Text"
        text_color: ${theme.primary_color}
        font_size: ${theme.text_size}
    }
}
"#.to_string(),
            expected_elements: Some(2),
            expected_properties: vec![
                ("background_color".to_string(), PropertyValue::String("#ffffff".to_string())),
                ("text_color".to_string(), PropertyValue::String("#3b82f6".to_string())),
                ("font_size".to_string(), PropertyValue::Integer(16)),
            ],
            expected_errors: vec![],
            description: "Application with theme variable interpolation".to_string(),
            category: FixtureCategory::Theming,
        });

        // Error case fixture
        self.add_fixture(TestFixture {
            name: "invalid_syntax".to_string(),
            source: r#"
App {
    Text {
        text: "Missing closing brace"
    // Missing closing brace
}
"#.to_string(),
            expected_elements: None,
            expected_properties: vec![],
            expected_errors: vec!["syntax error".to_string()],
            description: "Invalid syntax with missing closing brace".to_string(),
            category: FixtureCategory::ErrorCases,
        });

        // Component fixture
        self.add_fixture(TestFixture {
            name: "custom_component".to_string(),
            source: r#"
Define Card {
    Properties {
        title: String = "Default Title"
        content: String = "Default Content"
    }
    
    Container {
        padding: 16
        border_radius: 8
        background_color: "#f8fafc"
        
        Text {
            text: ${title}
            font_weight: "bold"
            margin_bottom: 8
        }
        
        Text {
            text: ${content}
            text_color: "#64748b"
        }
    }
}

App {
    Card {
        title: "Test Card"
        content: "This is a test card component"
    }
}
"#.to_string(),
            expected_elements: Some(4),
            expected_properties: vec![
                ("padding".to_string(), PropertyValue::Integer(16)),
                ("border_radius".to_string(), PropertyValue::Integer(8)),
            ],
            expected_errors: vec![],
            description: "Custom component definition and usage".to_string(),
            category: FixtureCategory::Components,
        });
    }

    pub fn create_temporary_fixture(&self, name: &str, source: &str, temp_dir: &Path) -> Result<PathBuf> {
        let fixture_path = temp_dir.join(format!("{}.kry", name));
        fs::write(&fixture_path, source)?;
        Ok(fixture_path)
    }

    pub fn load_fixtures_from_directory(&mut self, dir: &Path) -> Result<usize> {
        let mut loaded = 0;
        
        for entry in walkdir::WalkDir::new(dir) {
            let entry = entry?;
            if entry.file_type().is_file() && 
               entry.path().extension().map_or(false, |ext| ext == "kry") {
                
                let name = entry.path().file_stem()
                    .and_then(|s| s.to_str())
                    .unwrap_or("unknown")
                    .to_string();
                
                let source = fs::read_to_string(entry.path())?;
                
                let fixture = TestFixture {
                    name: name.clone(),
                    source,
                    expected_elements: None,
                    expected_properties: vec![],
                    expected_errors: vec![],
                    description: format!("Loaded from {}", entry.path().display()),
                    category: FixtureCategory::Basic,
                };
                
                self.add_fixture(fixture);
                loaded += 1;
            }
        }
        
        Ok(loaded)
    }
}

impl TestFixture {
    pub fn new(name: impl Into<String>, source: impl Into<String>) -> Self {
        Self {
            name: name.into(),
            source: source.into(),
            expected_elements: None,
            expected_properties: vec![],
            expected_errors: vec![],
            description: String::new(),
            category: FixtureCategory::Basic,
        }
    }

    pub fn with_expected_elements(mut self, count: usize) -> Self {
        self.expected_elements = Some(count);
        self
    }

    pub fn with_expected_property(mut self, name: impl Into<String>, value: PropertyValue) -> Self {
        self.expected_properties.push((name.into(), value));
        self
    }

    pub fn with_expected_error(mut self, error: impl Into<String>) -> Self {
        self.expected_errors.push(error.into());
        self
    }

    pub fn with_description(mut self, description: impl Into<String>) -> Self {
        self.description = description.into();
        self
    }

    pub fn with_category(mut self, category: FixtureCategory) -> Self {
        self.category = category;
        self
    }

    pub fn should_succeed(&self) -> bool {
        self.expected_errors.is_empty()
    }

    pub fn should_fail(&self) -> bool {
        !self.expected_errors.is_empty()
    }
}
