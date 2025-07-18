use std::path::Path;
use std::fs;
use anyhow::{Context, Result};
use clap::Parser;
use tracing::{info, error};
use kryon_core::load_krb_file;
use kryon_render::Renderer;
use kryon_runtime::KryonApp;
use kryon_html::{HtmlRenderer, HtmlConfig};

#[derive(Parser)]
#[command(name = "kryon-renderer-html")]
#[command(about = "HTML/CSS renderer for Kryon .krb files")]
struct Args {
    /// Path to the .krb file to render
    krb_file: String,

    /// Output directory for generated files
    #[arg(short, long, default_value = "./output")]
    output: String,

    /// Output filename (without extension)
    #[arg(short, long)]
    filename: Option<String>,

    /// Whether to inline CSS
    #[arg(long)]
    inline_css: bool,

    /// Whether to inline JavaScript
    #[arg(long)]
    inline_js: bool,

    /// Whether to minify output
    #[arg(long)]
    minify: bool,

    /// Additional CSS to include
    #[arg(long)]
    css: Option<String>,

    /// Additional JavaScript to include
    #[arg(long)]
    js: Option<String>,

    /// HTML document title
    #[arg(long)]
    title: Option<String>,

    /// Enable debug logging
    #[arg(short, long)]
    debug: bool,

    /// Generate responsive styles
    #[arg(long)]
    responsive: bool,

    /// Generate accessibility styles
    #[arg(long)]
    accessibility: bool,

    /// Generate dark mode styles
    #[arg(long)]
    dark_mode: bool,

    /// Start development server
    #[arg(long)]
    serve: bool,

    /// Server port (when using --serve)
    #[arg(long, default_value = "8080")]
    port: u16,

    /// Server host (when using --serve)
    #[arg(long, default_value = "127.0.0.1")]
    host: String,

    /// Enable hot reload (when using --serve)
    #[arg(long)]
    hot_reload: bool,
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

    // Validate file path
    if !Path::new(&args.krb_file).exists() {
        anyhow::bail!("KRB file not found: {}", args.krb_file);
    }

    info!("Loading KRB file: {}", args.krb_file);
    
    // Load the KRB file
    let krb_file = load_krb_file(&args.krb_file)
        .context("Failed to load KRB file")?;

    // Get viewport size from KRB file
    let mut viewport_size = glam::Vec2::new(800.0, 600.0);
    if let Some(root_id) = krb_file.root_element_id {
        if let Some(root_element) = krb_file.elements.get(&root_id) {
            if root_element.size.x > 0.0 && root_element.size.y > 0.0 {
                viewport_size = root_element.size;
            }
        }
    }

    // Create HTML configuration
    let mut html_config = HtmlConfig {
        inline_css: args.inline_css,
        inline_js: args.inline_js,
        minify: args.minify,
        additional_css: args.css.unwrap_or_default(),
        additional_js: args.js.unwrap_or_default(),
        title: args.title.unwrap_or_else(|| "Kryon Application".to_string()),
        ..Default::default()
    };

    // Create HTML renderer
    let html_renderer = HtmlRenderer::new(viewport_size);

    // Create and render the application
    let mut app = KryonApp::new(&args.krb_file, html_renderer)
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

    // Determine output filename
    let base_filename = args.filename.unwrap_or_else(|| {
        Path::new(&args.krb_file)
            .file_stem()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string()
    });

    // Create output directory
    fs::create_dir_all(&args.output)
        .context("Failed to create output directory")?;

    if args.serve {
        #[cfg(feature = "html-server")]
        {
            info!("Starting development server on http://{}:{}", args.host, args.port);
            
            use kryon_html::{ServerBuilder, ServerConfig};
            
            let server = ServerBuilder::new()
                .host(args.host)
                .port(args.port)
                .html_config(html_config)
                .build();
            
            // Set the renderer
            tokio::runtime::Runtime::new()
                .unwrap()
                .block_on(async {
                    server.set_renderer(html_renderer.clone()).await?;
                    server.start().await
                })?;
        }
        #[cfg(not(feature = "html-server"))]
        {
            error!("Server feature not enabled. Please compile with --features html-server");
            anyhow::bail!("Server feature not available");
        }
    } else {
        // Generate files
        let html_content = html_renderer.generate_html(&html_config);
        
        // Write HTML file
        let html_path = Path::new(&args.output).join(format!("{}.html", base_filename));
        fs::write(&html_path, html_content)
            .context("Failed to write HTML file")?;
        
        info!("Generated HTML: {}", html_path.display());
        
        // Write separate CSS file if not inlined
        if !args.inline_css {
            let css_content = html_renderer.generate_css(&html_config);
            let css_path = Path::new(&args.output).join(format!("kryon-{}.css", html_renderer.instance_id()));
            fs::write(&css_path, css_content)
                .context("Failed to write CSS file")?;
            
            info!("Generated CSS: {}", css_path.display());
        }
        
        // Write separate JS file if not inlined
        if !args.inline_js {
            let js_content = html_renderer.generate_js(&html_config);
            let js_path = Path::new(&args.output).join(format!("kryon-{}.js", html_renderer.instance_id()));
            fs::write(&js_path, js_content)
                .context("Failed to write JS file")?;
            
            info!("Generated JS: {}", js_path.display());
        }
        
        // Log canvas elements if any
        let canvas_elements = html_renderer.canvas_elements();
        if !canvas_elements.is_empty() {
            info!("Canvas elements generated: {}", canvas_elements.len());
            for canvas in canvas_elements {
                info!("  - Canvas '{}' ({}x{})", canvas.id, canvas.size.x, canvas.size.y);
            }
        }
        
        info!("HTML generation complete!");
    }

    Ok(())
}