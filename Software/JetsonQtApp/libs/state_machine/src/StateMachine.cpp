#include "JetsonQtApp/StateMachine/StateMachine.hpp"

#include <stdexcept>

namespace jetsonqt::statemachine {

StateMachine::StateMachine(std::vector<StateDefinition> states)
    : states_(std::move(states)) {
    if (states_.empty()) {
        throw std::invalid_argument("StateMachine requires at least one state");
    }
}

const StateDefinition& StateMachine::currentState() const {
    return states_.at(stateIndex_);
}

std::size_t StateMachine::currentStateIndex() const {
    return stateIndex_;
}

std::size_t StateMachine::selectedActionIndex() const {
    return actionIndex_;
}

int StateMachine::spiceLevel() const {
    return spiceLevel_;
}

int StateMachine::stirSpeed() const {
    return stirSpeed_;
}

const std::vector<StateDefinition>& StateMachine::states() const {
    return states_;
}

void StateMachine::selectPreviousAction() {
    const auto& actions = currentState().actions;
    if (actions.empty()) {
        actionIndex_ = 0;
        return;
    }

    actionIndex_ = (actionIndex_ + actions.size() - 1) % actions.size();
}

void StateMachine::selectNextAction() {
    const auto& actions = currentState().actions;
    if (actions.empty()) {
        actionIndex_ = 0;
        return;
    }

    actionIndex_ = (actionIndex_ + 1) % actions.size();
}

void StateMachine::reset() {
    stateIndex_ = 0;
    actionIndex_ = 0;
    spiceLevel_ = 1;
    stirSpeed_ = 1;
}

bool StateMachine::triggerSelectedAction() {
    const auto& state = currentState();
    if (state.actions.empty()) {
        return false;
    }

    clampSelectedAction();
    const std::optional<std::string> nextState = nextStateForAction(state.name, state.actions.at(actionIndex_));
    if (!nextState.has_value()) {
        return false;
    }

    const std::size_t previousStateIndex = stateIndex_;
    const std::size_t nextStateIndex = findStateIndex(nextState.value());
    applyActionSideEffects(state.name, state.actions.at(actionIndex_));

    stateIndex_ = nextStateIndex;
    if (stateIndex_ != previousStateIndex) {
        actionIndex_ = 0;
    }

    if (states_.at(stateIndex_).name == "Shaking spice into pot" && state.name != "Shaking spice into pot") {
        spiceLevel_ = 1;
    }

    if (states_.at(stateIndex_).name == "Using utensil" && state.name != "Using utensil") {
        stirSpeed_ = 1;
    }

    return true;
}

std::optional<std::string> StateMachine::nextStateForAction(const std::string& stateName,
                                                            const std::string& actionName) const {
    if (stateName == "Init") {
        if (actionName == "select spice") {
            return "Selecting spice";
        }
        if (actionName == "select utensil") {
            return "Selecting utensil";
        }
    }

    if (stateName == "Selecting spice") {
        return "Spice selected";
    }

    if (stateName == "Spice selected") {
        if (actionName == "shake into pot") {
            return "Shaking spice into pot";
        }
        if (actionName == "put down") {
            return "Init";
        }
    }

    if (stateName == "Shaking spice into pot") {
        if (actionName == "put down") {
            return "Init";
        }
        return "Shaking spice into pot";
    }

    if (stateName == "Selecting utensil") {
        return "Utensil selected";
    }

    if (stateName == "Utensil selected") {
        if (actionName == "use utensil") {
            return "Using utensil";
        }
        if (actionName == "put down") {
            return "Init";
        }
    }

    if (stateName == "Using utensil") {
        if (actionName == "put down") {
            return "Init";
        }
        return "Using utensil";
    }

    return std::nullopt;
}

std::size_t StateMachine::findStateIndex(const std::string& name) const {
    for (std::size_t i = 0; i < states_.size(); ++i) {
        if (states_[i].name == name) {
            return i;
        }
    }

    return 0;
}

void StateMachine::applyActionSideEffects(const std::string& stateName, const std::string& actionName) {
    if (stateName == "Shaking spice into pot") {
        if (actionName == "more spice" && spiceLevel_ < 5) {
            ++spiceLevel_;
            return;
        }

        if (actionName == "less spice" && spiceLevel_ > 1) {
            --spiceLevel_;
        }

        return;
    }

    if (stateName == "Using utensil") {
        if (actionName == "stir faster" && stirSpeed_ < 5) {
            ++stirSpeed_;
            return;
        }

        if (actionName == "stir slower" && stirSpeed_ > 1) {
            --stirSpeed_;
        }

        return;
    }
}

void StateMachine::clampSelectedAction() {
    const auto& actions = currentState().actions;
    if (actions.empty() || actionIndex_ >= actions.size()) {
        actionIndex_ = 0;
    }
}

std::vector<StateDefinition> buildCookingUiStates() {
    return {
        {"Init", {"select spice", "select utensil"}},
        {"Selecting spice", {"spice 1", "spice 2", "spice 3", "spice 4", "spice 5", "spice 6", "spice 7", "spice 8", "spice 9"}},
        {"Spice selected", {"shake into pot", "put down"}},
        {"Shaking spice into pot", {"more spice", "less spice", "put down"}},
        {"Selecting utensil", {"utensil 1", "utensil 2", "utensil 3", "utensil 4", "utensil 5"}},
        {"Utensil selected", {"use utensil", "put down"}},
        {"Using utensil", {"stir faster", "stir slower", "put down"}},
    };
}

}  // namespace jetsonqt::statemachine
