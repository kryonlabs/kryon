// Simple executable to run hello world example
use std::env;
use std::process;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <krb_file>", args[0]);
        process::exit(1);
    }
    
    let krb_file = &args[1];
    
    // For now, we'll use the ratatui backend to display the KRB file
    match kryon_ratatui::run_krb_file(krb_file) {
        Ok(_) => {
            println!("Successfully ran {}", krb_file);
        }
        Err(e) => {
            eprintln!("Error running {}: {}", krb_file, e);
            process::exit(1);
        }
    }
}