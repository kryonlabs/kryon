//! Snapshot testing utilities for visual regression testing

use crate::prelude::*;
use std::process::Command;

/// Snapshot test manager for visual regression testing
#[derive(Debug)]
pub struct SnapshotManager {
    pub output_dir: PathBuf,
    pub update_snapshots: bool,
    pub backend: SnapshotBackend,
}

/// Available snapshot backends
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SnapshotBackend {
    Ratatui,  // Text-based snapshots (primary)
    Raylib,   // Image-based snapshots (future)
}

/// Snapshot test result
#[derive(Debug, Clone)]
pub struct SnapshotResult {
    pub test_name: String,
    pub snapshot_path: PathBuf,
    pub matches: bool,
    pub diff_path: Option<PathBuf>,
    pub backend: SnapshotBackend,
}

/// Snapshot test configuration
#[derive(Debug, Clone)]
pub struct SnapshotTestConfig {
    pub name: String,
    pub source: String,
    pub backend: SnapshotBackend,
    pub window_size: (u32, u32),
    pub timeout: Duration,
}

impl SnapshotManager {
    pub fn new(output_dir: impl Into<PathBuf>, backend: SnapshotBackend) -> Self {
        Self {
            output_dir: output_dir.into(),
            update_snapshots: std::env::var("KRYON_UPDATE_SNAPSHOTS").is_ok(),
            backend,
        }
    }

    /// Create snapshot test from KRY source
    pub async fn create_snapshot_test(&self, config: SnapshotTestConfig) -> Result<SnapshotResult> {
        // Create output directory if it doesn't exist
        fs::create_dir_all(&self.output_dir)?;
        
        // Compile the source
        let krb_data = self.compile_source(&config.source)?;
        
        // Create temporary KRB file
        let temp_dir = tempfile::tempdir()?;
        let krb_path = temp_dir.path().join("test.krb");
        fs::write(&krb_path, &krb_data)?;
        
        // Generate snapshot based on backend
        match config.backend {
            SnapshotBackend::Ratatui => self.create_ratatui_snapshot(&config, &krb_path).await,
            SnapshotBackend::Raylib => self.create_raylib_snapshot(&config, &krb_path).await,
        }
    }

    async fn create_ratatui_snapshot(
        &self,
        config: &SnapshotTestConfig,
        krb_path: &Path,
    ) -> Result<SnapshotResult> {
        // Use kryon-ratatui to generate text-based snapshot
        let output = Command::new("cargo")
            .args(&[
                "run",
                "--package", "kryon-ratatui",
                "--bin", "kryon-ratatui",
                "--",
                krb_path.to_str().unwrap(),
                "--snapshot",
            ])
            .output()
            .context("Failed to run kryon-ratatui for snapshot")?;

        if !output.status.success() {
            bail!(
                "Ratatui snapshot generation failed: {}",
                String::from_utf8_lossy(&output.stderr)
            );
        }

        let snapshot_content = String::from_utf8(output.stdout)
            .context("Invalid UTF-8 in snapshot output")?;

        // Save snapshot and compare
        let snapshot_path = self.output_dir.join(format!("{}.ratatui.snap", config.name));
        let matches = if snapshot_path.exists() && !self.update_snapshots {
            let existing_content = fs::read_to_string(&snapshot_path)?;
            existing_content == snapshot_content
        } else {
            // Create or update snapshot
            fs::write(&snapshot_path, &snapshot_content)?;
            true
        };

        let diff_path = if !matches {
            Some(self.create_diff_file(&config.name, &snapshot_path, &snapshot_content)?)
        } else {
            None
        };

        Ok(SnapshotResult {
            test_name: config.name.clone(),
            snapshot_path,
            matches,
            diff_path,
            backend: SnapshotBackend::Ratatui,
        })
    }

    async fn create_raylib_snapshot(
        &self,
        config: &SnapshotTestConfig,
        krb_path: &Path,
    ) -> Result<SnapshotResult> {
        // Future implementation for image-based snapshots
        todo!("Raylib snapshot testing not yet implemented")
    }

    fn compile_source(&self, source: &str) -> Result<Vec<u8>> {
        let options = CompilerOptions::default();
        compile_string(source, options)
            .map_err(|e| anyhow::anyhow!("Compilation failed: {}", e))
    }

    fn create_diff_file(
        &self,
        test_name: &str,
        snapshot_path: &Path,
        new_content: &str,
    ) -> Result<PathBuf> {
        let existing_content = fs::read_to_string(snapshot_path)?;
        let diff_path = self.output_dir.join(format!("{}.diff", test_name));
        
        // Create a simple diff format
        let mut diff_lines = Vec::new();
        diff_lines.push(format!("--- Expected ({})", snapshot_path.display()));
        diff_lines.push(format!("+++ Actual"));
        diff_lines.push("".to_string());
        
        let existing_lines: Vec<&str> = existing_content.lines().collect();
        let new_lines: Vec<&str> = new_content.lines().collect();
        
        // Simple line-by-line diff
        let max_lines = existing_lines.len().max(new_lines.len());
        for i in 0..max_lines {
            let existing_line = existing_lines.get(i).unwrap_or(&"");
            let new_line = new_lines.get(i).unwrap_or(&"");
            
            if existing_line != new_line {
                diff_lines.push(format!("-{}", existing_line));
                diff_lines.push(format!("+{}", new_line));
            } else {
                diff_lines.push(format!(" {}", existing_line));
            }
        }
        
        fs::write(&diff_path, diff_lines.join("\n"))?;
        Ok(diff_path)
    }

    /// Run all snapshot tests from a directory
    pub async fn run_snapshot_tests(&self, test_dir: &Path) -> Result<Vec<SnapshotResult>> {
        let mut results = Vec::new();
        
        for entry in walkdir::WalkDir::new(test_dir) {
            let entry = entry?;
            if entry.file_type().is_file() && 
               entry.path().extension().map_or(false, |ext| ext == "kry") {
                
                let test_name = entry.path().file_stem()
                    .and_then(|s| s.to_str())
                    .unwrap_or("unknown")
                    .to_string();
                
                let source = fs::read_to_string(entry.path())?;
                
                let config = SnapshotTestConfig {
                    name: test_name,
                    source,
                    backend: self.backend.clone(),
                    window_size: (800, 600),
                    timeout: Duration::from_secs(30),
                };
                
                match self.create_snapshot_test(config).await {
                    Ok(result) => results.push(result),
                    Err(e) => eprintln!("Snapshot test failed for {}: {}", entry.path().display(), e),
                }
            }
        }
        
        Ok(results)
    }

    /// Print snapshot test results
    pub fn print_snapshot_results(&self, results: &[SnapshotResult]) {
        println!("\n=== Snapshot Test Results ===");
        
        let total = results.len();
        let passed = results.iter().filter(|r| r.matches).count();
        let failed = total - passed;
        
        for result in results {
            let status = if result.matches {
                "PASS".green()
            } else {
                "FAIL".red()
            };
            
            println!("[{}] {} ({})", status, result.test_name, format!("{:?}", result.backend).to_lowercase());
            
            if let Some(diff_path) = &result.diff_path {
                println!("  📄 Diff: {}", diff_path.display());
            }
        }
        
        println!("\nSnapshot Summary: {} total, {} passed, {} failed", total, passed, failed);
        
        if failed > 0 {
            println!("\n⚠️  Use KRYON_UPDATE_SNAPSHOTS=1 to update failing snapshots");
        }
    }
}

/// Insta integration for existing snapshot workflows
pub mod insta_integration {
    use super::*;
    
    /// Create insta snapshot from KRY source
    pub fn assert_kryon_snapshot(test_name: &str, source: &str) {
        let krb_data = compile_string(source, CompilerOptions::default())
            .expect("Compilation should succeed for snapshot test");
        
        // Parse KRB for structured snapshot
        let krb_file = KrbFile::parse(&krb_data)
            .expect("KRB parsing should succeed for snapshot test");
        
        // Create structured snapshot data
        let snapshot_data = SnapshotData {
            element_count: krb_file.elements.len(),
            elements: krb_file.elements.iter().map(|e| ElementSnapshot {
                element_type: e.element_type.clone(),
                property_count: e.custom_properties.len(),
                has_text: e.custom_properties.contains_key("text"),
                has_layout: e.custom_properties.contains_key("layout"),
                child_count: e.children.len(),
            }).collect(),
        };
        
        insta::assert_yaml_snapshot!(test_name, snapshot_data);
    }
    
    /// Snapshot data structure for insta
    #[derive(Debug, Serialize)]
    struct SnapshotData {
        element_count: usize,
        elements: Vec<ElementSnapshot>,
    }
    
    #[derive(Debug, Serialize)]
    struct ElementSnapshot {
        element_type: String,
        property_count: usize,
        has_text: bool,
        has_layout: bool,
        child_count: usize,
    }
}

/// Utility functions for snapshot testing
pub fn create_snapshot_test_config(
    name: impl Into<String>,
    source: impl Into<String>,
) -> SnapshotTestConfig {
    SnapshotTestConfig {
        name: name.into(),
        source: source.into(),
        backend: SnapshotBackend::Ratatui,
        window_size: (800, 600),
        timeout: Duration::from_secs(30),
    }
}

/// Macro for easy snapshot testing
#[macro_export]
macro_rules! kryon_snapshot_test {
    ($test_name:expr, $source:expr) => {
        #[tokio::test]
        async fn snapshot_test() -> anyhow::Result<()> {
            let manager = crate::snapshots::SnapshotManager::new(
                "target/snapshots",
                crate::snapshots::SnapshotBackend::Ratatui,
            );
            
            let config = crate::snapshots::create_snapshot_test_config($test_name, $source);
            let result = manager.create_snapshot_test(config).await?;
            
            if !result.matches {
                anyhow::bail!("Snapshot test '{}' failed", $test_name);
            }
            
            Ok(())
        }
    };
}

pub use kryon_snapshot_test;
