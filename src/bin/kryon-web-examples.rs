use std::path::{Path, PathBuf};
use std::fs;
use std::net::{TcpListener, TcpStream};
use std::io::{BufRead, BufReader, Read as _, Write as _};
use anyhow::{Context, Result};
use clap::Parser;
use tracing::info;
use walkdir::WalkDir;
use kryon_core::{load_krb_file, Vec2};
use kryon_runtime::KryonApp;
use kryon_html::{HtmlRenderer, HtmlConfig};
use kryon_compiler::compile_kry_to_krb;

#[derive(Parser)]
#[command(name = "kryon-web-examples")]
#[command(about = "Generate web examples from all KRB files in the examples directory")]
struct Args {
    /// Examples directory path
    #[arg(short, long, default_value = "./examples")]
    examples_dir: String,

    /// Output directory for generated web files
    #[arg(short, long, default_value = "./web-examples")]
    output: String,

    /// Whether to inline CSS
    #[arg(long, default_value = "true")]
    inline_css: bool,

    /// Whether to inline JavaScript
    #[arg(long, default_value = "true")]
    inline_js: bool,

    /// Whether to minify output
    #[arg(long)]
    minify: bool,

    /// Enable debug logging
    #[arg(short, long)]
    debug: bool,

    /// Generate responsive styles
    #[arg(long, default_value = "true")]
    responsive: bool,

    /// Generate accessibility styles
    #[arg(long, default_value = "true")]
    accessibility: bool,

    /// Generate dark mode styles
    #[arg(long, default_value = "true")]
    dark_mode: bool,

    /// Index page title
    #[arg(long, default_value = "Kryon Examples")]
    title: String,

    /// Clean output directory before generating
    #[arg(long, default_value = "true")]
    clean: bool,

    /// Start a development server after generating examples
    #[arg(long)]
    serve: bool,

    /// Port for the development server
    #[arg(long, default_value = "8080")]
    port: u16,
}

#[derive(Debug, Clone)]
struct ExampleInfo {
    name: String,
    path: PathBuf,
    relative_path: String,
    category: String,
    description: String,
}

fn main() -> Result<()> {
    let args = Args::parse();

    // Initialize logging
    let subscriber = tracing_subscriber::fmt()
        .with_max_level(if args.debug {
            tracing::Level::DEBUG
        } else {
            tracing::Level::INFO
        })
        .with_target(false)
        .compact()
        .finish();
    
    tracing::subscriber::set_global_default(subscriber)
        .context("Failed to set tracing subscriber")?;

    info!("🚀 Generating web examples from {}", args.examples_dir);

    // Create output directory
    if args.clean && Path::new(&args.output).exists() {
        info!("🧹 Cleaning output directory: {}", args.output);
        fs::remove_dir_all(&args.output)
            .context("Failed to clean output directory")?;
    }
    
    fs::create_dir_all(&args.output)
        .context("Failed to create output directory")?;

    // Find all KRY files
    let examples = find_kry_examples(&args.examples_dir)?;
    info!("📁 Found {} examples to process", examples.len());

    let mut processed_examples = Vec::new();

    // Process each example
    for example in &examples {
        match process_example(example, &args) {
            Ok(output_path) => {
                info!("✅ Generated: {} -> {}", example.name, output_path.display());
                processed_examples.push((example.clone(), output_path));
            }
            Err(e) => {
                eprintln!("❌ Failed to process {}: {}", example.name, e);
            }
        }
    }

    // Generate index page from Kryon file
    let index_info = match generate_kryon_index_page(&processed_examples, &args) {
        Ok(index_path) => {
            info!("📄 Generated index page: {}", index_path.display());
            Some(format!("🌐 Open {} to view all examples", index_path.display()))
        }
        Err(e) => {
            eprintln!("⚠️  Failed to generate index page: {}", e);
            eprintln!("🔄 Continuing without index page...");
            None
        }
    };

    // Copy assets if any
    copy_example_assets(&args.examples_dir, &args.output)?;

    info!("🎉 Generated {} web examples successfully!", processed_examples.len());
    if let Some(index_message) = index_info {
        info!("{}", index_message);
    }

    // Start development server if requested
    if args.serve {
        start_dev_server(&args.output, args.port)?;
    }

    Ok(())
}

fn find_kry_examples(examples_dir: &str) -> Result<Vec<ExampleInfo>> {
    let mut examples = Vec::new();
    let base_path = Path::new(examples_dir);

    for entry in WalkDir::new(examples_dir)
        .follow_links(true)
        .into_iter()
        .filter_map(|e| e.ok())
    {
        let path = entry.path();
        if path.extension().and_then(|s| s.to_str()) == Some("kry") {
            let relative_path = path.strip_prefix(base_path)
                .context("Failed to get relative path")?
                .to_string_lossy()
                .to_string();

            let name = path.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("unknown")
                .to_string();

            // Extract category from directory structure
            let category = if let Some(parent) = path.parent() {
                parent.file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or("misc")
                    .to_string()
            } else {
                "misc".to_string()
            };

            // Generate description from name
            let description = name
                .replace('_', " ")
                .replace('-', " ")
                .split_whitespace()
                .map(|word| {
                    let mut chars = word.chars();
                    match chars.next() {
                        None => String::new(),
                        Some(first) => first.to_uppercase().collect::<String>() + chars.as_str(),
                    }
                })
                .collect::<Vec<_>>()
                .join(" ");

            examples.push(ExampleInfo {
                name,
                path: path.to_path_buf(),
                relative_path,
                category,
                description,
            });
        }
    }

    // Sort by category and name
    examples.sort_by(|a, b| {
        a.category.cmp(&b.category)
            .then_with(|| a.name.cmp(&b.name))
    });

    Ok(examples)
}

fn process_example(example: &ExampleInfo, args: &Args) -> Result<PathBuf> {
    // First compile KRY to KRB
    let krb_data = compile_kry_to_krb(&example.path)
        .context("Failed to compile KRY to KRB")?;
    
    // Create a temporary KRB file
    let temp_krb_path = example.path.with_extension("krb");
    fs::write(&temp_krb_path, &krb_data)
        .context("Failed to write temporary KRB file")?;
    
    // Load the KRB file
    let krb_file = load_krb_file(temp_krb_path.to_str().unwrap())
        .context("Failed to load KRB file")?;

    // Get viewport size from KRB file
    let mut viewport_size = Vec2::new(800.0, 600.0);
    if let Some(root_id) = krb_file.root_element_id {
        if let Some(root_element) = krb_file.elements.get(&root_id) {
            let size_x = root_element.layout_size.width.to_pixels(1.0);
            let size_y = root_element.layout_size.height.to_pixels(1.0);
            if size_x > 0.0 && size_y > 0.0 {
                viewport_size = Vec2::new(size_x, size_y);
            }
        }
    }

    // Create HTML configuration
    let html_config = HtmlConfig {
        inline_css: args.inline_css,
        inline_js: args.inline_js,
        minify: args.minify,
        title: format!("{} - Kryon Example", example.description),
        additional_css: String::new(),
        additional_js: String::new(),
        meta_tags: vec![
            r#"<meta charset="UTF-8">"#.to_string(),
            r#"<meta name="viewport" content="width=device-width, initial-scale=1.0">"#.to_string(),
            format!(r#"<meta name="description" content="Kryon example: {}">"#, example.description),
        ],
    };

    // Create HTML renderer
    let mut html_renderer = HtmlRenderer::new(viewport_size);

    // Create and render the application using the temporary KRB file
    let mut app = KryonApp::new(temp_krb_path.to_string_lossy().as_ref(), html_renderer)
        .context("Failed to create Kryon application")?;

    // Render one frame to generate HTML
    app.render().context("Failed to render application")?;

    // Get the rendered HTML
    let html_renderer = app.renderer_mut().backend_mut();

    // Generate additional styles
    if args.responsive {
        html_renderer.generate_responsive_styles()
            .context("Failed to generate responsive styles")?;
    }

    if args.accessibility {
        html_renderer.generate_accessibility_styles()
            .context("Failed to generate accessibility styles")?;
    }

    if args.dark_mode {
        html_renderer.generate_dark_mode_styles()
            .context("Failed to generate dark mode styles")?;
    }

    // Generate utility classes
    html_renderer.generate_utility_classes()
        .context("Failed to generate utility classes")?;

    html_renderer.generate_css_variables()
        .context("Failed to generate CSS variables")?;

    // Create example-specific output directory
    let example_output_dir = Path::new(&args.output).join(&example.category).join(&example.name);
    fs::create_dir_all(&example_output_dir)
        .context("Failed to create example output directory")?;

    // Generate HTML file
    let html_content = html_renderer.generate_html(&html_config);
    let html_path = example_output_dir.join("index.html");
    fs::write(&html_path, html_content)
        .context("Failed to write HTML file")?;

    // Write separate CSS file if not inlined
    if !args.inline_css {
        let css_content = html_renderer.generate_css(&html_config);
        let css_path = example_output_dir.join(format!("kryon-{}.css", html_renderer.instance_id()));
        fs::write(&css_path, css_content)
            .context("Failed to write CSS file")?;
    }

    // Write separate JS file if not inlined
    if !args.inline_js {
        let js_content = html_renderer.generate_js(&html_config);
        let js_path = example_output_dir.join(format!("kryon-{}.js", html_renderer.instance_id()));
        fs::write(&js_path, js_content)
            .context("Failed to write JS file")?;
    }

    Ok(html_path)
}

fn generate_kryon_index_page(examples: &[(ExampleInfo, PathBuf)], args: &Args) -> Result<PathBuf> {
    // Generate a simple HTML index page without Kryon compilation
    let html_content = generate_simple_html_index(examples, args);
    let index_path = Path::new(&args.output).join("index.html");
    fs::write(&index_path, html_content)
        .context("Failed to write index HTML file")?;
    Ok(index_path)
}

fn generate_simple_html_index(examples: &[(ExampleInfo, PathBuf)], args: &Args) -> String {
    let mut html = String::new();
    
    // HTML head
    html.push_str("<!DOCTYPE html>\n");
    html.push_str("<html lang=\"en\">\n");
    html.push_str("<head>\n");
    html.push_str("    <meta charset=\"UTF-8\">\n");
    html.push_str("    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    html.push_str(&format!("    <title>{}</title>\n", args.title));
    html.push_str("    <meta name=\"description\" content=\"Interactive examples built with Kryon framework\">\n");
    html.push_str("    <style>\n");
    html.push_str(&generate_index_css());
    html.push_str("    </style>\n");
    html.push_str("</head>\n");
    html.push_str("<body>\n");
    
    // Header
    html.push_str("    <div class=\"header\">\n");
    html.push_str("        <h1>Kryon Examples</h1>\n");
    html.push_str("        <p>Interactive examples built with Kryon framework</p>\n");
    html.push_str(&format!("        <div class=\"stats\">\n"));
    html.push_str(&format!("            <div class=\"stat\"><span class=\"stat-number\">{}</span><span class=\"stat-label\">Examples</span></div>\n", examples.len()));
    
    // Count categories
    let mut categories = std::collections::HashSet::new();
    for (example, _) in examples {
        categories.insert(&example.category);
    }
    html.push_str(&format!("            <div class=\"stat\"><span class=\"stat-number\">{}</span><span class=\"stat-label\">Categories</span></div>\n", categories.len()));
    html.push_str("        </div>\n");
    html.push_str("    </div>\n");
    
    // Search
    html.push_str("    <div class=\"search-container\">\n");
    html.push_str("        <input type=\"text\" id=\"search\" placeholder=\"Search examples...\">\n");
    html.push_str("    </div>\n");
    
    // Examples by category
    html.push_str("    <div class=\"examples-container\">\n");
    
    let mut examples_by_category: std::collections::BTreeMap<String, Vec<&(ExampleInfo, PathBuf)>> = std::collections::BTreeMap::new();
    for example in examples {
        examples_by_category.entry(example.0.category.clone()).or_insert_with(Vec::new).push(example);
    }
    
    for (category, category_examples) in examples_by_category {
        html.push_str(&format!("        <div class=\"category\">\n"));
        html.push_str(&format!("            <h2>{}</h2>\n", category.replace('_', " ")));
        html.push_str("            <div class=\"examples-grid\">\n");
        
        for (example, _) in category_examples {
            let relative_path = format!("{}/{}", example.category, example.name);
            html.push_str(&format!("                <div class=\"example-card\" onclick=\"openExample('{}')\">\n", relative_path));
            html.push_str(&format!("                    <h3>{}</h3>\n", example.description));
            html.push_str(&format!("                    <code>{}</code>\n", example.relative_path));
            html.push_str(&format!("                    <span class=\"category-tag\">{}</span>\n", example.category.replace('_', " ")));
            html.push_str("                </div>\n");
        }
        
        html.push_str("            </div>\n");
        html.push_str("        </div>\n");
    }
    
    html.push_str("    </div>\n");
    
    // JavaScript
    html.push_str("    <script>\n");
    html.push_str(&generate_dynamic_js_for_examples(examples));
    html.push_str("    </script>\n");
    html.push_str("</body>\n");
    html.push_str("</html>\n");
    
    html
}

fn generate_index_css() -> String {
    r#"
        body {
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            color: #333;
        }
        
        .header {
            text-align: center;
            padding: 60px 20px;
            color: white;
        }
        
        .header h1 {
            font-size: 3rem;
            margin: 0 0 20px 0;
            font-weight: 700;
            text-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
        
        .header p {
            font-size: 1.2rem;
            margin: 0 0 40px 0;
            opacity: 0.9;
        }
        
        .stats {
            display: flex;
            justify-content: center;
            gap: 40px;
        }
        
        .stat {
            background: rgba(255,255,255,0.1);
            padding: 20px;
            border-radius: 12px;
            backdrop-filter: blur(10px);
            text-align: center;
        }
        
        .stat-number {
            display: block;
            font-size: 2rem;
            font-weight: bold;
            margin-bottom: 5px;
        }
        
        .stat-label {
            font-size: 0.9rem;
            opacity: 0.8;
        }
        
        .search-container {
            max-width: 500px;
            margin: 0 auto 40px auto;
            padding: 0 20px;
        }
        
        #search {
            width: 100%;
            padding: 15px 20px;
            font-size: 16px;
            border: none;
            border-radius: 25px;
            background: rgba(255,255,255,0.9);
            backdrop-filter: blur(10px);
            box-shadow: 0 8px 32px rgba(0,0,0,0.1);
        }
        
        .examples-container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 0 20px 60px 20px;
        }
        
        .category {
            margin-bottom: 60px;
            background: rgba(255,255,255,0.05);
            border-radius: 20px;
            padding: 30px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.1);
        }
        
        .category h2 {
            color: white;
            text-align: center;
            margin: 0 0 30px 0;
            font-size: 1.5rem;
            font-weight: 600;
        }
        
        .examples-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 20px;
        }
        
        .example-card {
            background: rgba(255,255,255,0.9);
            border-radius: 12px;
            padding: 25px;
            cursor: pointer;
            transition: all 0.3s ease;
            border: 1px solid rgba(0,0,0,0.1);
        }
        
        .example-card:hover {
            transform: translateY(-5px) scale(1.02);
            box-shadow: 0 20px 40px rgba(0,0,0,0.2);
        }
        
        .example-card h3 {
            margin: 0 0 10px 0;
            color: #2c3e50;
            font-size: 1.1rem;
            font-weight: 600;
        }
        
        .example-card code {
            display: block;
            background: #f8f9fa;
            padding: 8px;
            border-radius: 4px;
            font-size: 0.8rem;
            color: #666;
            margin-bottom: 15px;
            font-family: 'Monaco', 'Menlo', monospace;
        }
        
        .category-tag {
            background: #667eea;
            color: white;
            padding: 4px 12px;
            border-radius: 12px;
            font-size: 0.8rem;
            font-weight: 500;
        }
        
        @media (max-width: 768px) {
            .header h1 { font-size: 2rem; }
            .stats { flex-direction: column; gap: 20px; }
            .examples-grid { grid-template-columns: 1fr; }
            .category { padding: 20px; }
        }
    "#.to_string()
}

fn generate_dynamic_css_for_examples(_examples: &[(ExampleInfo, PathBuf)]) -> String {
    let mut css = String::new();
    
    // Add hover effects for example cards
    css.push_str("\n/* Dynamic example styles */\n");
    css.push_str(".example-card:hover {\n");
    css.push_str("  transform: translateY(-5px) scale(1.02);\n");
    css.push_str("  box-shadow: 0 20px 40px rgba(0,0,0,0.2);\n");
    css.push_str("  transition: all 0.3s ease;\n");
    css.push_str("}\n");
    
    // Add search functionality styles
    css.push_str(".hidden { display: none !important; }\n");
    css.push_str(".highlight { background-color: yellow; }\n");
    
    css
}

fn generate_dynamic_js_for_examples(_examples: &[(ExampleInfo, PathBuf)]) -> String {
    let mut js = String::new();
    
    // Generate example navigation
    js.push_str("\n// Dynamic example navigation\n");
    js.push_str("function openExample(examplePath) {\n");
    js.push_str("  window.location.href = examplePath + '/index.html';\n");
    js.push_str("}\n\n");
    
    // Generate search functionality
    js.push_str("document.addEventListener('DOMContentLoaded', function() {\n");
    js.push_str("  const searchInput = document.querySelector('input[placeholder*=\"Search\"]');\n");
    js.push_str("  if (searchInput) {\n");
    js.push_str("    searchInput.addEventListener('input', function(e) {\n");
    js.push_str("      const query = e.target.value.toLowerCase();\n");
    js.push_str("      const cards = document.querySelectorAll('[onclick*=\"openExample\"]');\n");
    js.push_str("      cards.forEach(card => {\n");
    js.push_str("        const text = card.textContent.toLowerCase();\n");
    js.push_str("        if (text.includes(query)) {\n");
    js.push_str("          card.style.display = 'block';\n");
    js.push_str("        } else {\n");
    js.push_str("          card.style.display = 'none';\n");
    js.push_str("        }\n");
    js.push_str("      });\n");
    js.push_str("    });\n");
    js.push_str("  }\n");
    js.push_str("});\n");
    
    js
}

fn copy_example_assets(examples_dir: &str, output_dir: &str) -> Result<()> {
    let assets_to_copy = &["png", "jpg", "jpeg", "gif", "svg", "ttf", "woff", "woff2"];
    
    for entry in WalkDir::new(examples_dir)
        .follow_links(true)
        .into_iter()
        .filter_map(|e| e.ok())
    {
        let path = entry.path();
        if let Some(extension) = path.extension().and_then(|s| s.to_str()) {
            if assets_to_copy.contains(&extension.to_lowercase().as_str()) {
                let relative_path = path.strip_prefix(examples_dir)
                    .context("Failed to get relative asset path")?;
                
                let output_path = Path::new(output_dir).join("assets").join(relative_path);
                
                if let Some(parent) = output_path.parent() {
                    fs::create_dir_all(parent)
                        .context("Failed to create asset directory")?;
                }
                
                fs::copy(path, &output_path)
                    .context("Failed to copy asset file")?;
                
                info!("📋 Copied asset: {}", relative_path.display());
            }
        }
    }
    
    Ok(())
}

fn start_dev_server(output_dir: &str, port: u16) -> Result<()> {

    let address = format!("127.0.0.1:{}", port);
    let listener = TcpListener::bind(&address)
        .context(format!("Failed to bind to {}", address))?;

    info!("🚀 Development server running at http://{}", address);
    info!("📂 Serving files from: {}", output_dir);
    info!("🌐 Open http://127.0.0.1:{} in your browser", port);
    info!("🔄 Press Ctrl+C to stop the server");

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                if let Err(e) = handle_request(stream, output_dir) {
                    eprintln!("❌ Error handling request: {}", e);
                }
            }
            Err(e) => {
                eprintln!("❌ Error accepting connection: {}", e);
            }
        }
    }

    Ok(())
}

fn handle_request(mut stream: TcpStream, output_dir: &str) -> Result<()> {

    let mut reader = BufReader::new(&stream);
    let mut request_line = String::new();
    reader.read_line(&mut request_line)?;

    let parts: Vec<&str> = request_line.split_whitespace().collect();
    if parts.len() < 2 {
        return Ok(());
    }

    let method = parts[0];
    let mut path = parts[1];

    // Only handle GET requests
    if method != "GET" {
        let response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        stream.write_all(response.as_bytes())?;
        return Ok(());
    }

    // Remove query parameters
    if let Some(query_pos) = path.find('?') {
        path = &path[..query_pos];
    }

    // Default to index.html for root
    if path == "/" {
        path = "/index.html";
    }

    // Remove leading slash
    let file_path = if path.starts_with('/') {
        &path[1..]
    } else {
        path
    };

    let full_path = Path::new(output_dir).join(file_path);

    // Security check: ensure the path is within output_dir
    let canonical_output = fs::canonicalize(output_dir)?;
    let canonical_full = match fs::canonicalize(&full_path) {
        Ok(p) => p,
        Err(_) => {
            let response = "HTTP/1.1 404 Not Found\r\n\r\n404 - File not found";
            stream.write_all(response.as_bytes())?;
            return Ok(());
        }
    };

    if !canonical_full.starts_with(&canonical_output) {
        let response = "HTTP/1.1 403 Forbidden\r\n\r\n403 - Access denied";
        stream.write_all(response.as_bytes())?;
        return Ok(());
    }

    // Read and serve the file
    match fs::read(&full_path) {
        Ok(contents) => {
            let content_type = get_content_type(&full_path);
            let response = format!(
                "HTTP/1.1 200 OK\r\nContent-Type: {}\r\nContent-Length: {}\r\n\r\n",
                content_type,
                contents.len()
            );
            stream.write_all(response.as_bytes())?;
            stream.write_all(&contents)?;
            
            info!("📄 Served: {}", file_path);
        }
        Err(_) => {
            let response = "HTTP/1.1 404 Not Found\r\n\r\n404 - File not found";
            stream.write_all(response.as_bytes())?;
        }
    }

    Ok(())
}

fn get_content_type(path: &Path) -> &'static str {
    match path.extension().and_then(|s| s.to_str()) {
        Some("html") => "text/html; charset=utf-8",
        Some("css") => "text/css",
        Some("js") => "application/javascript",
        Some("json") => "application/json",
        Some("png") => "image/png",
        Some("jpg") | Some("jpeg") => "image/jpeg",
        Some("gif") => "image/gif",
        Some("svg") => "image/svg+xml",
        Some("ico") => "image/x-icon",
        Some("ttf") => "font/ttf",
        Some("woff") => "font/woff",
        Some("woff2") => "font/woff2",
        _ => "text/plain",
    }
}