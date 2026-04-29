#pragma once

#include "JetsonQtApp/StateMachine/StateDefinition.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace jetsonqt::statemachine {

class StateMachine {
public:
    explicit StateMachine(std::vector<StateDefinition> states);

    [[nodiscard]] const StateDefinition& currentState() const;
    [[nodiscard]] std::size_t currentStateIndex() const;
    [[nodiscard]] std::size_t selectedActionIndex() const;
    [[nodiscard]] const std::vector<StateDefinition>& states() const;

    void selectPreviousAction();
    void selectNextAction();
    void reset();
    bool triggerSelectedAction();

private:
    [[nodiscard]] std::optional<std::string> nextStateForAction(const std::string& stateName,
                                                                const std::string& actionName) const;
    [[nodiscard]] std::size_t findStateIndex(const std::string& name) const;
    void clampSelectedAction();

    std::vector<StateDefinition> states_;
    std::size_t stateIndex_ = 0;
    std::size_t actionIndex_ = 0;
};

std::vector<StateDefinition> buildCookingUiStates();

}  // namespace jetsonqt::statemachine
