#pragma once

#include <string>
#include <vector>

namespace jetsonqt::statemachine {

struct StateDefinition {
    std::string name;
    std::vector<std::string> actions;
};

}  // namespace jetsonqt::statemachine
