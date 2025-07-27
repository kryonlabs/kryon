#[cfg(feature = "server")]
use crate::{HtmlRenderer, HtmlConfig, HtmlResult, HtmlError};
#[cfg(feature = "server")]
use std::path::PathBuf;
#[cfg(feature = "server")]
use std::sync::Arc;
#[cfg(feature = "server")]
use tokio::sync::RwLock;
#[cfg(feature = "server")]
use warp::Filter;

#[cfg(feature = "server")]
/// Configuration for the Kryon HTML server
#[derive(Debug, Clone)]
pub struct ServerConfig {
    pub port: u16,
    pub host: String,
    pub serve_static: bool,
    pub static_dir: PathBuf,
    pub reload_on_change: bool,
    pub cors_enabled: bool,
    pub max_file_size: usize,
}

#[cfg(feature = "server")]
impl Default for ServerConfig {
    fn default() -> Self {
        Self {
            port: 8080,
            host: "127.0.0.1".to_string(),
            serve_static: true,
            static_dir: PathBuf::from("./static"),
            reload_on_change: true,
            cors_enabled: true,
            max_file_size: 10 * 1024 * 1024, // 10MB
        }
    }
}

#[cfg(feature = "server")]
/// Kryon HTML development server
pub struct KryonHtmlServer {
    config: ServerConfig,
    html_config: HtmlConfig,
    renderer: Arc<RwLock<Option<HtmlRenderer>>>,
}

#[cfg(feature = "server")]
impl KryonHtmlServer {
    /// Create a new server instance
    pub fn new(config: ServerConfig, html_config: HtmlConfig) -> Self {
        Self {
            config,
            html_config,
            renderer: Arc::new(RwLock::new(None)),
        }
    }

    /// Start the development server
    pub async fn start(&self) -> HtmlResult<()> {
        let addr = format!("{}:{}", self.config.host, self.config.port);
        
        println!("🚀 Kryon HTML Server starting on http://{}", addr);
        
        // Create routes
        let routes = self.create_routes();
        
        // Start server
        let server_addr: std::net::SocketAddr = addr.parse()
            .map_err(|e| HtmlError::ServerError(format!("Invalid server address: {}", e)))?;
        
        warp::serve(routes)
            .run(server_addr)
            .await;
        
        Ok(())
    }
    
    /// Set the renderer for the server
    pub async fn set_renderer(&self, renderer: HtmlRenderer) -> HtmlResult<()> {
        let mut renderer_guard = self.renderer.write().await;
        *renderer_guard = Some(renderer);
        Ok(())
    }
    
    /// Create warp routes for the server
    fn create_routes(&self) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
        let renderer = self.renderer.clone();
        let html_config = self.html_config.clone();
        
        // Clone for each route to avoid move errors
        let renderer_html = renderer.clone();
        let html_config_html = html_config.clone();
        let renderer_css = renderer.clone();
        let html_config_css = html_config.clone();
        let renderer_js = renderer.clone();
        let html_config_js = html_config.clone();
        let renderer_api = renderer.clone();
        
        // Main HTML route
        let html_route = warp::path::end()
            .and(warp::get())
            .and(warp::any().map(move || renderer_html.clone()))
            .and(warp::any().map(move || html_config_html.clone()))
            .and_then(serve_html);
        
        // CSS route
        let css_route = warp::path!("kryon" / String)
            .and(warp::path::end())
            .and(warp::get())
            .and(warp::header::<String>("accept"))
            .and(warp::any().map(move || renderer_css.clone()))
            .and(warp::any().map(move || html_config_css.clone()))
            .and_then(serve_css);
        
        // JavaScript route
        let js_route = warp::path!("kryon" / String)
            .and(warp::path::end())
            .and(warp::get())
            .and(warp::header::<String>("accept"))
            .and(warp::any().map(move || renderer_js.clone()))
            .and(warp::any().map(move || html_config_js.clone()))
            .and_then(serve_js);
        
        // API route for live reload
        let api_route = warp::path("api")
            .and(warp::path("reload"))
            .and(warp::post())
            .and(warp::body::json())
            .and(warp::any().map(move || renderer_api.clone()))
            .and_then(handle_reload);
        
        // Static files route
        let static_route = if self.config.serve_static {
            warp::path("static")
                .and(warp::fs::dir(self.config.static_dir.clone()))
                .boxed()
        } else {
            warp::path("static")
                .and(warp::any())
                .and_then(|| async { Err(warp::reject::not_found()) })
                .boxed()
        };
        
        // Combine routes
        let routes = html_route
            .or(css_route)
            .or(js_route)
            .or(api_route)
            .or(static_route);
        
        // Add CORS if enabled
        let cors = if self.config.cors_enabled {
            warp::cors()
                .allow_any_origin()
                .allow_headers(vec!["content-type"])
                .allow_methods(vec!["GET", "POST"])
        } else {
            warp::cors()
                .allow_origin("http://localhost:3000") // Restrict to localhost if CORS disabled
                .allow_headers(vec!["content-type"])
                .allow_methods(vec!["GET", "POST"])
        };
        
        routes.with(cors).boxed()
    }
}

#[cfg(feature = "server")]
async fn serve_html(
    renderer: Arc<RwLock<Option<HtmlRenderer>>>,
    html_config: HtmlConfig,
) -> Result<impl warp::Reply, warp::Rejection> {
    let renderer_guard = renderer.read().await;
    
    if let Some(renderer) = renderer_guard.as_ref() {
        let html = renderer.generate_html(&html_config);
        Ok(warp::reply::html(html))
    } else {
        let default_html = r#"
<!DOCTYPE html>
<html>
<head>
    <title>Kryon HTML Server</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; padding: 50px; }
        .status { color: #666; }
    </style>
</head>
<body>
    <h1>Kryon HTML Server</h1>
    <p class="status">No renderer loaded. Please load a KRB file.</p>
</body>
</html>
        "#;
        Ok(warp::reply::html(default_html.to_string()))
    }
}

#[cfg(feature = "server")]
async fn serve_css(
    filename: String,
    accept: String,
    renderer: Arc<RwLock<Option<HtmlRenderer>>>,
    html_config: HtmlConfig,
) -> Result<impl warp::Reply, warp::Rejection> {
    if !filename.ends_with(".css") || !accept.contains("text/css") {
        return Err(warp::reject::not_found());
    }
    
    let renderer_guard = renderer.read().await;
    
    if let Some(renderer) = renderer_guard.as_ref() {
        let css = renderer.generate_css(&html_config);
        Ok(warp::reply::with_header(css, "content-type", "text/css".to_string()))
    } else {
        let default_css = "/* No renderer loaded */".to_string();
        Ok(warp::reply::with_header(default_css, "content-type", "text/css".to_string()))
    }
}

#[cfg(feature = "server")]
async fn serve_js(
    filename: String,
    accept: String,
    renderer: Arc<RwLock<Option<HtmlRenderer>>>,
    html_config: HtmlConfig,
) -> Result<impl warp::Reply, warp::Rejection> {
    if !filename.ends_with(".js") || !accept.contains("application/javascript") {
        return Err(warp::reject::not_found());
    }
    
    let renderer_guard = renderer.read().await;
    
    if let Some(renderer) = renderer_guard.as_ref() {
        let js = renderer.generate_js(&html_config);
        Ok(warp::reply::with_header(js, "content-type", "application/javascript".to_string()))
    } else {
        let default_js = "/* No renderer loaded */".to_string();
        Ok(warp::reply::with_header(default_js, "content-type", "application/javascript".to_string()))
    }
}

#[cfg(feature = "server")]
#[derive(serde::Deserialize)]
struct ReloadRequest {
    path: String,
}

#[cfg(feature = "server")]
async fn handle_reload(
    request: ReloadRequest,
    _renderer: Arc<RwLock<Option<HtmlRenderer>>>,
) -> Result<impl warp::Reply, warp::Rejection> {
    println!("🔄 Reload request for: {}", request.path);
    
    // Here you would reload the KRB file and update the renderer
    // For now, just return success
    Ok(warp::reply::json(&serde_json::json!({
        "success": true,
        "message": "Reload requested"
    })))
}

#[cfg(feature = "server")]
/// Builder for creating a Kryon HTML server
pub struct ServerBuilder {
    config: ServerConfig,
    html_config: HtmlConfig,
}

#[cfg(feature = "server")]
impl ServerBuilder {
    /// Create a new server builder
    pub fn new() -> Self {
        Self {
            config: ServerConfig::default(),
            html_config: HtmlConfig::default(),
        }
    }
    
    /// Set the server port
    pub fn port(mut self, port: u16) -> Self {
        self.config.port = port;
        self
    }
    
    /// Set the server host
    pub fn host(mut self, host: impl Into<String>) -> Self {
        self.config.host = host.into();
        self
    }
    
    /// Enable/disable static file serving
    pub fn serve_static(mut self, enabled: bool) -> Self {
        self.config.serve_static = enabled;
        self
    }
    
    /// Set the static files directory
    pub fn static_dir(mut self, dir: impl Into<PathBuf>) -> Self {
        self.config.static_dir = dir.into();
        self
    }
    
    /// Enable/disable CORS
    pub fn cors(mut self, enabled: bool) -> Self {
        self.config.cors_enabled = enabled;
        self
    }
    
    /// Set HTML configuration
    pub fn html_config(mut self, config: HtmlConfig) -> Self {
        self.html_config = config;
        self
    }
    
    /// Build the server
    pub fn build(self) -> KryonHtmlServer {
        KryonHtmlServer::new(self.config, self.html_config)
    }
}

#[cfg(feature = "server")]
impl Default for ServerBuilder {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(not(feature = "server"))]
use crate::{HtmlResult, HtmlError};

#[cfg(not(feature = "server"))]
/// Server functionality is not available without the "server" feature
pub struct KryonHtmlServer;

#[cfg(not(feature = "server"))]
impl KryonHtmlServer {
    pub fn new(_config: (), _html_config: ()) -> Self {
        Self
    }
    
    pub async fn start(&self) -> HtmlResult<()> {
        Err(HtmlError::ServerError("Server feature not enabled".to_string()))
    }
}

#[cfg(not(feature = "server"))]
/// Server builder is not available without the "server" feature
pub struct ServerBuilder;

#[cfg(not(feature = "server"))]
impl ServerBuilder {
    pub fn new() -> Self {
        Self
    }
    
    pub fn build(self) -> KryonHtmlServer {
        KryonHtmlServer
    }
}