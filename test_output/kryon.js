// Kryon Web Runtime
// Auto-generated JavaScript for IR component interaction

let kryon_components = new Map();
let kryon_handlers = new Map();

function kryon_handle_click(component_id, handler_id) {
    console.log('Click on component', component_id, 'handler:', handler_id);
    // TODO: Call WASM handler when implemented
}

function kryon_handle_hover(component_id, handler_id) {
    console.log('Hover on component', component_id, 'handler:', handler_id);
    // TODO: Call WASM handler when implemented
}

function kryon_handle_leave(component_id) {
    console.log('Leave component', component_id);
}

function kryon_handle_focus(component_id) {
    console.log('Focus component', component_id);
}

function kryon_handle_blur(component_id) {
    console.log('Blur component', component_id);
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', function() {
    console.log('Kryon Web Runtime initialized');
    
    // Find all Kryon components
    const elements = document.querySelectorAll('[id^="kryon-"]');
    elements.forEach(element => {
        const id = parseInt(element.id.replace('kryon-', ''));
        kryon_components.set(id, element);
    });
    
    console.log('Found', kryon_components.size, 'Kryon components');
});

