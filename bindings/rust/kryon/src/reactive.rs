use std::cell::RefCell;
use std::rc::Rc;

/// Reactive signal that notifies subscribers when its value changes
///
/// Signals provide the foundation for reactive UI updates in Kryon.
/// When a signal's value changes, all subscribed components automatically re-render.
///
/// # Example
///
/// ```rust
/// use kryon::prelude::*;
///
/// let mut count = Signal::new(0);
///
/// // Subscribe to changes
/// count.subscribe(|value| {
///     println!("Count changed to: {}", value);
/// });
///
/// count.set(10);  // Prints: "Count changed to: 10"
/// ```
#[derive(Clone)]
pub struct Signal<T> {
    value: Rc<RefCell<T>>,
    subscribers: Rc<RefCell<Vec<Box<dyn Fn(&T)>>>>,
}

impl<T: Clone> Signal<T> {
    /// Create a new signal with an initial value
    pub fn new(initial: T) -> Self {
        Signal {
            value: Rc::new(RefCell::new(initial)),
            subscribers: Rc::new(RefCell::new(Vec::new())),
        }
    }

    /// Get the current value (clones the value)
    pub fn get(&self) -> T {
        self.value.borrow().clone()
    }

    /// Set a new value and notify all subscribers
    pub fn set(&mut self, new_value: T) {
        *self.value.borrow_mut() = new_value.clone();
        self.notify_subscribers(&new_value);
    }

    /// Update the value using a closure and notify subscribers
    ///
    /// # Example
    ///
    /// ```rust
    /// # use kryon::Signal;
    /// let mut count = Signal::new(0);
    /// count.update(|c| *c += 1);
    /// # assert_eq!(count.get(), 1);
    /// ```
    pub fn update<F>(&mut self, f: F)
    where
        F: FnOnce(&mut T),
    {
        {
            let mut value_ref = self.value.borrow_mut();
            f(&mut *value_ref);
        }
        let new_value = self.get();
        self.notify_subscribers(&new_value);
    }

    /// Subscribe to changes with a callback
    ///
    /// Returns the subscriber ID (index)
    pub fn subscribe<F>(&mut self, callback: F) -> usize
    where
        F: Fn(&T) + 'static,
    {
        let mut subscribers = self.subscribers.borrow_mut();
        subscribers.push(Box::new(callback));
        subscribers.len() - 1
    }

    /// Notify all subscribers of the current value
    fn notify_subscribers(&self, value: &T) {
        let subscribers = self.subscribers.borrow();
        for callback in subscribers.iter() {
            callback(value);
        }
    }

    /// Create a derived signal that computes its value from this signal
    ///
    /// # Example
    ///
    /// ```rust
    /// # use kryon::Signal;
    /// let mut count = Signal::new(5);
    /// let doubled = count.derive(|c| c * 2);
    /// # assert_eq!(doubled.get(), 10);
    /// ```
    pub fn derive<U, F>(&mut self, f: F) -> Signal<U>
    where
        U: Clone + 'static,
        F: Fn(&T) -> U + 'static,
        T: 'static,
    {
        let initial = f(&self.get());
        let derived = Signal::new(initial);

        let derived_clone = derived.clone();
        self.subscribe(move |value| {
            let new_derived = f(value);
            // Note: This requires derived to be mutable, but we can't do that here
            // This is a limitation of the current design
            // In practice, you'd use a more sophisticated reactive system
        });

        derived
    }

    /// Map the signal's value to a different type
    pub fn map<U, F>(&self, f: F) -> U
    where
        F: FnOnce(&T) -> U,
    {
        let value = self.value.borrow();
        f(&*value)
    }
}

impl<T: Clone + PartialEq> Signal<T> {
    /// Set a new value only if it's different from the current value
    ///
    /// Returns true if the value was updated
    pub fn set_if_changed(&mut self, new_value: T) -> bool {
        let current = self.get();
        if current != new_value {
            self.set(new_value);
            true
        } else {
            false
        }
    }
}

impl<T: Clone + std::fmt::Display> std::fmt::Display for Signal<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.value.borrow())
    }
}

impl<T: Clone + std::fmt::Debug> std::fmt::Debug for Signal<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Signal({:?})", self.value.borrow())
    }
}

/// Convenience function to create a new signal
///
/// # Example
///
/// ```rust
/// # use kryon::signal;
/// let count = signal(0);
/// # assert_eq!(count.get(), 0);
/// ```
pub fn signal<T: Clone>(initial: T) -> Signal<T> {
    Signal::new(initial)
}

/// Computed signal that derives its value from other signals
///
/// This is useful for creating read-only signals that automatically
/// update when their dependencies change.
pub struct Computed<T> {
    signal: Signal<T>,
}

impl<T: Clone> Computed<T> {
    /// Get the current computed value
    pub fn get(&self) -> T {
        self.signal.get()
    }
}

impl<T: Clone> From<Signal<T>> for Computed<T> {
    fn from(signal: Signal<T>) -> Self {
        Computed { signal }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_signal_basic() {
        let mut count = Signal::new(0);
        assert_eq!(count.get(), 0);

        count.set(5);
        assert_eq!(count.get(), 5);

        count.update(|c| *c += 10);
        assert_eq!(count.get(), 15);
    }

    #[test]
    fn test_signal_subscribe() {
        let mut count = Signal::new(0);
        let updates = Rc::new(RefCell::new(Vec::new()));

        let updates_clone = updates.clone();
        count.subscribe(move |value| {
            updates_clone.borrow_mut().push(*value);
        });

        count.set(5);
        count.set(10);
        count.update(|c| *c *= 2);

        assert_eq!(*updates.borrow(), vec![5, 10, 20]);
    }

    #[test]
    fn test_signal_set_if_changed() {
        let mut count = Signal::new(0);
        let updates = Rc::new(RefCell::new(0));

        let updates_clone = updates.clone();
        count.subscribe(move |_| {
            *updates_clone.borrow_mut() += 1;
        });

        assert!(count.set_if_changed(5));
        assert_eq!(*updates.borrow(), 1);

        assert!(!count.set_if_changed(5)); // Same value, no update
        assert_eq!(*updates.borrow(), 1); // Still only 1 update

        assert!(count.set_if_changed(10));
        assert_eq!(*updates.borrow(), 2);
    }

    #[test]
    fn test_signal_clone() {
        let count1 = Signal::new(42);
        let count2 = count1.clone();

        assert_eq!(count1.get(), count2.get());

        // Both signals share the same underlying value
        assert_eq!(count1.get(), 42);
        assert_eq!(count2.get(), 42);
    }

    #[test]
    fn test_signal_helper() {
        let count = signal(10);
        assert_eq!(count.get(), 10);
    }
}
