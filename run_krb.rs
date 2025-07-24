// Simple KRB runner using kryon-runtime and kryon-raylib
use std::env;
use std::fs;
use std::process;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <krb_file>", args[0]);
        process::exit(1);
    }
    
    let krb_file = &args[1];
    
    println!("🚀 Running KRB file: {}", krb_file);
    
    // Read the KRB file
    let krb_data = match fs::read(krb_file) {
        Ok(data) => data,
        Err(e) => {
            eprintln!("❌ Failed to read {}: {}", krb_file, e);
            process::exit(1);
        }
    };
    
    println!("✅ Loaded KRB file ({} bytes)", krb_data.len());
    
    // Parse the KRB file using kryon-core
    match kryon_core::parse_krb(&krb_data) {
        Ok(parsed_app) => {
            println!("✅ Parsed KRB successfully");
            println!("📋 App info:");
            println!("   - Elements: {}", parsed_app.elements.len());
            println!("   - Styles: {}", parsed_app.styles.len());
            
            // For demonstration, just show we successfully parsed
            println!("🎉 Successfully processed {}", krb_file);
            println!("💡 In a full implementation, this would render the UI");
        }
        Err(e) => {
            eprintln!("❌ Failed to parse KRB: {}", e);
            process::exit(1);
        }
    }
}