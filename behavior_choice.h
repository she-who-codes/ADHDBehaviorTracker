#pragma once

#include <string>

// One behavior button on the main screen (stored in behavior_choices.txt).
struct BehaviorChoice
{
    std::string name;
    int points;       // Positive for good choices, negative for wrong choices.
    bool isGood;
};
