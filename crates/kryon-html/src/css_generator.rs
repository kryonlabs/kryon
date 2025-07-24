use crate::{HtmlRenderer, HtmlResult};
use glam::Vec4;

impl HtmlRenderer {
    /// Generate responsive CSS styles
    pub fn generate_responsive_styles(&mut self) -> HtmlResult<()> {
        self.css_content.push_str("\n/* Responsive styles */\n");
        
        // Mobile styles
        self.css_content.push_str("@media (max-width: 768px) {\n");
        self.css_content.push_str("  .kryon-root {\n");
        self.css_content.push_str("    width: 100% !important;\n");
        self.css_content.push_str("    height: 100vh !important;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("}\n\n");
        
        // Tablet styles
        self.css_content.push_str("@media (min-width: 769px) and (max-width: 1024px) {\n");
        self.css_content.push_str("  .kryon-root {\n");
        self.css_content.push_str("    max-width: 100%;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("}\n\n");
        
        Ok(())
    }
    
    /// Generate accessibility CSS
    pub fn generate_accessibility_styles(&mut self) -> HtmlResult<()> {
        self.css_content.push_str("\n/* Accessibility styles */\n");
        
        // Focus styles
        self.css_content.push_str("input:focus, button:focus, [tabindex]:focus {\n");
        self.css_content.push_str("  outline: 2px solid #4A90E2;\n");
        self.css_content.push_str("  outline-offset: 2px;\n");
        self.css_content.push_str("}\n\n");
        
        // High contrast mode
        self.css_content.push_str("@media (prefers-contrast: high) {\n");
        self.css_content.push_str("  .kryon-text {\n");
        self.css_content.push_str("    text-shadow: 0 0 1px currentColor;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("}\n\n");
        
        // Reduced motion
        self.css_content.push_str("@media (prefers-reduced-motion: reduce) {\n");
        self.css_content.push_str("  * {\n");
        self.css_content.push_str("    animation-duration: 0.01ms !important;\n");
        self.css_content.push_str("    animation-iteration-count: 1 !important;\n");
        self.css_content.push_str("    transition-duration: 0.01ms !important;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("}\n\n");
        
        Ok(())
    }
    
    /// Generate animation CSS
    pub fn generate_animation_styles(&mut self) -> HtmlResult<()> {
        self.css_content.push_str("\n/* Animation styles */\n");
        
        // Fade in animation
        self.css_content.push_str("@keyframes kryonFadeIn {\n");
        self.css_content.push_str("  from { opacity: 0; }\n");
        self.css_content.push_str("  to { opacity: 1; }\n");
        self.css_content.push_str("}\n\n");
        
        // Slide in animation
        self.css_content.push_str("@keyframes kryonSlideIn {\n");
        self.css_content.push_str("  from { transform: translateX(-100%); }\n");
        self.css_content.push_str("  to { transform: translateX(0); }\n");
        self.css_content.push_str("}\n\n");
        
        // Scale animation
        self.css_content.push_str("@keyframes kryonScale {\n");
        self.css_content.push_str("  from { transform: scale(0.8); }\n");
        self.css_content.push_str("  to { transform: scale(1); }\n");
        self.css_content.push_str("}\n\n");
        
        // Utility classes
        self.css_content.push_str(".kryon-fade-in {\n");
        self.css_content.push_str("  animation: kryonFadeIn 0.3s ease-out;\n");
        self.css_content.push_str("}\n\n");
        
        self.css_content.push_str(".kryon-slide-in {\n");
        self.css_content.push_str("  animation: kryonSlideIn 0.3s ease-out;\n");
        self.css_content.push_str("}\n\n");
        
        self.css_content.push_str(".kryon-scale {\n");
        self.css_content.push_str("  animation: kryonScale 0.3s ease-out;\n");
        self.css_content.push_str("}\n\n");
        
        Ok(())
    }
    
    /// Generate dark mode CSS
    pub fn generate_dark_mode_styles(&mut self) -> HtmlResult<()> {
        self.css_content.push_str("\n/* Dark mode styles */\n");
        self.css_content.push_str("@media (prefers-color-scheme: dark) {\n");
        self.css_content.push_str("  body {\n");
        self.css_content.push_str("    background-color: #1a1a1a;\n");
        self.css_content.push_str("    color: #ffffff;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("  .kryon-input {\n");
        self.css_content.push_str("    background-color: #2a2a2a;\n");
        self.css_content.push_str("    color: #ffffff;\n");
        self.css_content.push_str("    border-color: #4a4a4a;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("  .kryon-checkbox {\n");
        self.css_content.push_str("    background-color: #2a2a2a;\n");
        self.css_content.push_str("    border-color: #4a4a4a;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("}\n\n");
        
        Ok(())
    }
    
    /// Generate utility CSS classes
    pub fn generate_utility_classes(&mut self) -> HtmlResult<()> {
        self.css_content.push_str("\n/* Utility classes */\n");
        
        // Display utilities
        self.css_content.push_str(".kryon-hidden { display: none !important; }\n");
        self.css_content.push_str(".kryon-invisible { visibility: hidden !important; }\n");
        self.css_content.push_str(".kryon-visible { visibility: visible !important; }\n");
        
        // Position utilities
        self.css_content.push_str(".kryon-relative { position: relative !important; }\n");
        self.css_content.push_str(".kryon-absolute { position: absolute !important; }\n");
        self.css_content.push_str(".kryon-fixed { position: fixed !important; }\n");
        
        // Flexbox utilities
        self.css_content.push_str(".kryon-flex { display: flex !important; }\n");
        self.css_content.push_str(".kryon-flex-column { flex-direction: column !important; }\n");
        self.css_content.push_str(".kryon-flex-row { flex-direction: row !important; }\n");
        self.css_content.push_str(".kryon-justify-center { justify-content: center !important; }\n");
        self.css_content.push_str(".kryon-align-center { align-items: center !important; }\n");
        
        // Text utilities
        self.css_content.push_str(".kryon-text-center { text-align: center !important; }\n");
        self.css_content.push_str(".kryon-text-left { text-align: left !important; }\n");
        self.css_content.push_str(".kryon-text-right { text-align: right !important; }\n");
        
        // Spacing utilities
        for i in 0..=5 {
            let value = i * 8;
            self.css_content.push_str(&format!(".kryon-m-{} {{ margin: {}px !important; }}\n", i, value));
            self.css_content.push_str(&format!(".kryon-p-{} {{ padding: {}px !important; }}\n", i, value));
        }
        
        self.css_content.push_str("\n");
        
        Ok(())
    }
    
    /// Generate CSS custom properties (variables)
    pub fn generate_css_variables(&mut self) -> HtmlResult<()> {
        self.css_content.push_str("\n/* CSS Custom Properties */\n");
        self.css_content.push_str(":root {\n");
        
        // Colors
        self.css_content.push_str("  --kryon-primary: #4A90E2;\n");
        self.css_content.push_str("  --kryon-secondary: #7B68EE;\n");
        self.css_content.push_str("  --kryon-success: #50E3C2;\n");
        self.css_content.push_str("  --kryon-warning: #F5A623;\n");
        self.css_content.push_str("  --kryon-error: #D0021B;\n");
        self.css_content.push_str("  --kryon-text: #333333;\n");
        self.css_content.push_str("  --kryon-background: #FFFFFF;\n");
        self.css_content.push_str("  --kryon-border: #E0E0E0;\n");
        
        // Spacing
        self.css_content.push_str("  --kryon-spacing-xs: 4px;\n");
        self.css_content.push_str("  --kryon-spacing-sm: 8px;\n");
        self.css_content.push_str("  --kryon-spacing-md: 16px;\n");
        self.css_content.push_str("  --kryon-spacing-lg: 24px;\n");
        self.css_content.push_str("  --kryon-spacing-xl: 32px;\n");
        
        // Typography
        self.css_content.push_str("  --kryon-font-family: 'Arial', sans-serif;\n");
        self.css_content.push_str("  --kryon-font-size-xs: 12px;\n");
        self.css_content.push_str("  --kryon-font-size-sm: 14px;\n");
        self.css_content.push_str("  --kryon-font-size-md: 16px;\n");
        self.css_content.push_str("  --kryon-font-size-lg: 18px;\n");
        self.css_content.push_str("  --kryon-font-size-xl: 24px;\n");
        
        // Borders
        self.css_content.push_str("  --kryon-border-width: 1px;\n");
        self.css_content.push_str("  --kryon-border-radius: 4px;\n");
        self.css_content.push_str("  --kryon-border-radius-lg: 8px;\n");
        
        // Shadows
        self.css_content.push_str("  --kryon-shadow-sm: 0 1px 2px rgba(0, 0, 0, 0.05);\n");
        self.css_content.push_str("  --kryon-shadow-md: 0 4px 6px rgba(0, 0, 0, 0.1);\n");
        self.css_content.push_str("  --kryon-shadow-lg: 0 10px 15px rgba(0, 0, 0, 0.15);\n");
        
        self.css_content.push_str("}\n\n");
        
        // Dark mode variables
        self.css_content.push_str("@media (prefers-color-scheme: dark) {\n");
        self.css_content.push_str("  :root {\n");
        self.css_content.push_str("    --kryon-text: #FFFFFF;\n");
        self.css_content.push_str("    --kryon-background: #1a1a1a;\n");
        self.css_content.push_str("    --kryon-border: #4a4a4a;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("}\n\n");
        
        Ok(())
    }
    
    /// Generate CSS for custom colors
    pub fn generate_color_classes(&mut self, colors: &[(String, Vec4)]) -> HtmlResult<()> {
        self.css_content.push_str("\n/* Custom color classes */\n");
        
        for (name, color) in colors {
            let safe_name = name.replace(|c: char| !c.is_alphanumeric() && c != '-', "-");
            
            // Background color class
            self.css_content.push_str(&format!(
                ".kryon-bg-{} {{ background-color: rgba({}, {}, {}, {}) !important; }}\n",
                safe_name,
                (color.x * 255.0) as u8,
                (color.y * 255.0) as u8,
                (color.z * 255.0) as u8,
                color.w
            ));
            
            // Text color class
            self.css_content.push_str(&format!(
                ".kryon-text-{} {{ color: rgba({}, {}, {}, {}) !important; }}\n",
                safe_name,
                (color.x * 255.0) as u8,
                (color.y * 255.0) as u8,
                (color.z * 255.0) as u8,
                color.w
            ));
            
            // Border color class
            self.css_content.push_str(&format!(
                ".kryon-border-{} {{ border-color: rgba({}, {}, {}, {}) !important; }}\n",
                safe_name,
                (color.x * 255.0) as u8,
                (color.y * 255.0) as u8,
                (color.z * 255.0) as u8,
                color.w
            ));
        }
        
        self.css_content.push_str("\n");
        
        Ok(())
    }
    
    /// Generate print styles
    pub fn generate_print_styles(&mut self) -> HtmlResult<()> {
        self.css_content.push_str("\n/* Print styles */\n");
        self.css_content.push_str("@media print {\n");
        self.css_content.push_str("  .kryon-root {\n");
        self.css_content.push_str("    background: white !important;\n");
        self.css_content.push_str("    color: black !important;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("  .kryon-canvas {\n");
        self.css_content.push_str("    border: 1px solid #ccc;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("  .kryon-input {\n");
        self.css_content.push_str("    border: 1px solid #000;\n");
        self.css_content.push_str("    background: white;\n");
        self.css_content.push_str("  }\n");
        self.css_content.push_str("}\n\n");
        
        Ok(())
    }
}