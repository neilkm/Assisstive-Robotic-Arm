mod lib1;

/** @brief Re-export of the secondary template type for external consumers. */
pub use lib1::TemplateLibrary1;

/** @brief Stores integer state for the primary NVIDIA Jetson template library. */
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TemplateLibrary {
    /** @brief Backing integer value managed by the library. */
    value: i32,
}

impl TemplateLibrary {
    /** @brief Creates a new template library instance initialized to zero. */
    pub fn new() -> Self {
        Self { value: 0 }
    }

    /**
     * @brief Updates the stored integer value.
     *
     * @param value New value to store.
     */
    pub fn set(&mut self, value: i32) {
        self.value = value;
    }

    /**
     * @brief Returns the current stored integer value.
     *
     * @return i32 Current value.
     */
    pub fn get(&self) -> i32 {
        self.value
    }
}

#[cfg(test)]
mod template_library_tests {
    use super::TemplateLibrary;

    /** @test @brief Verifies that a new TemplateLibrary starts with value 0. */
    #[test]
    fn test_new_defaults_to_zero() {
        let obj = TemplateLibrary::new();
        assert_eq!(obj.get(), 0);
    }

    /** @test @brief Verifies that set() updates the stored value. */
    #[test]
    fn test_set_updates_value() {
        let mut obj = TemplateLibrary::new();
        obj.set(42);
        assert_eq!(obj.get(), 42);
    }

    /** @test @brief Verifies that get() returns the currently stored value. */
    #[test]
    fn test_get_returns_current_value() {
        let mut obj = TemplateLibrary::new();
        obj.set(-7);
        let value = obj.get();
        assert_eq!(value, -7);
    }
}
