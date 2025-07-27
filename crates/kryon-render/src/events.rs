// crates/kryon-render/src/events.rs
use glam::Vec2;

#[derive(Debug, Clone)]
pub enum InputEvent {
    MouseMove { position: Vec2 },
    MousePress { position: Vec2, button: MouseButton },
    MouseRelease { position: Vec2, button: MouseButton },
    KeyPress { key: KeyCode, modifiers: KeyModifiers },
    KeyRelease { key: KeyCode, modifiers: KeyModifiers },
    TextInput { text: String },
    Scroll { delta: Vec2 },
    Resize { size: Vec2 },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseButton {
    Left,
    Right,
    Middle,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyCode {
    Enter,
    Escape,
    Space,
    Backspace,
    Delete,
    Tab,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    Home,
    End,
    Character(char),
    // Add more as needed
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct KeyModifiers {
    pub ctrl: bool,
    pub shift: bool,
    pub alt: bool,
    pub meta: bool,
}

impl KeyModifiers {
    pub fn none() -> Self {
        Self {
            ctrl: false,
            shift: false,
            alt: false,
            meta: false,
        }
    }
    
    pub fn shift(mut self) -> Self {
        self.shift = true;
        self
    }
    
    pub fn ctrl(mut self) -> Self {
        self.ctrl = true;
        self
    }
    
    pub fn alt(mut self) -> Self {
        self.alt = true;
        self
    }
    
    pub fn meta(mut self) -> Self {
        self.meta = true;
        self
    }
}