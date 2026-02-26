/** @brief Stores enable-state for the secondary NVIDIA Jetson template library. */
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TemplateLibrary1 {
    /** @brief True when enabled; false otherwise. */
    enabled: bool,
}

impl TemplateLibrary1 {
    /** @brief Creates a new TemplateLibrary1 instance initialized to disabled. */
    pub fn new() -> Self {
        Self { enabled: false }
    }

    /**
     * @brief Updates the enabled state.
     *
     * @param enabled New enabled state.
     */
    pub fn set_enabled(&mut self, enabled: bool) {
        self.enabled = enabled;
    }

    /**
     * @brief Returns the current enabled state.
     *
     * @return bool True when enabled, otherwise false.
     */
    pub fn is_enabled(&self) -> bool {
        self.enabled
    }
}

#[cfg(test)]
mod template_library1_tests {
    use super::TemplateLibrary1;

    /** @test @brief Verifies that a new TemplateLibrary1 starts disabled. */
    #[test]
    fn test_new_defaults_to_disabled() {
        let obj = TemplateLibrary1::new();
        assert!(!obj.is_enabled());
    }

    /** @test @brief Verifies that set_enabled(true) enables the instance. */
    #[test]
    fn test_set_enabled_updates_state() {
        let mut obj = TemplateLibrary1::new();
        obj.set_enabled(true);
        assert!(obj.is_enabled());
    }

    /** @test @brief Verifies that is_enabled() reflects the current state. */
    #[test]
    fn test_is_enabled_returns_current_state() {
        let mut obj = TemplateLibrary1::new();
        obj.set_enabled(false);
        let enabled = obj.is_enabled();
        assert!(!enabled);
    }
}
