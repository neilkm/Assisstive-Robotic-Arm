#include "JetsonQtApp/StateMachine/StateMachine.h"
#include "JetsonQtApp/StateMachine/StateDefinition.h"

#include <gtest/gtest.h>
#include <stdexcept>

using namespace jetsonqt::statemachine;

// ─── Construction ─────────────────────────────────────────────────────────────

TEST(StateMachineConstruct, EmptyStatesThrows)
{
    EXPECT_THROW(StateMachine(std::vector<StateDefinition>{}), std::invalid_argument);
}

TEST(StateMachineConstruct, SingleStateIsValid)
{
    EXPECT_NO_THROW(StateMachine(std::vector<StateDefinition>{{"Only", {}}}));
}

// ─── Initial state (cooking UI) ───────────────────────────────────────────────

class CookingStateMachine : public ::testing::Test {
protected:
    CookingStateMachine() : sm(buildCookingUiStates()) {}
    StateMachine sm;
};

TEST_F(CookingStateMachine, InitialStateIsInit)
{
    EXPECT_EQ(sm.currentState().name, "Init");
    EXPECT_EQ(sm.currentStateIndex(), 0u);
}

TEST_F(CookingStateMachine, InitialSelectedActionIsZero)
{
    EXPECT_EQ(sm.selectedActionIndex(), 0u);
}

TEST_F(CookingStateMachine, InitHasTwoActions)
{
    EXPECT_EQ(sm.currentState().actions.size(), 2u);
    EXPECT_EQ(sm.currentState().actions[0], "select spice");
    EXPECT_EQ(sm.currentState().actions[1], "select utensil");
}

TEST_F(CookingStateMachine, InitialSpiceLevel)
{
    EXPECT_EQ(sm.spiceLevel(), 1);
}

TEST_F(CookingStateMachine, InitialStirSpeed)
{
    EXPECT_EQ(sm.stirSpeed(), 1);
}

// ─── Action navigation ────────────────────────────────────────────────────────

TEST_F(CookingStateMachine, SelectNextWrapsAround)
{
    ASSERT_EQ(sm.selectedActionIndex(), 0u);
    sm.selectNextAction(); // → 1
    EXPECT_EQ(sm.selectedActionIndex(), 1u);
    sm.selectNextAction(); // → wraps to 0
    EXPECT_EQ(sm.selectedActionIndex(), 0u);
}

TEST_F(CookingStateMachine, SelectPreviousWrapsAround)
{
    ASSERT_EQ(sm.selectedActionIndex(), 0u);
    sm.selectPreviousAction(); // → wraps to last (1)
    EXPECT_EQ(sm.selectedActionIndex(), 1u);
    sm.selectPreviousAction(); // → 0
    EXPECT_EQ(sm.selectedActionIndex(), 0u);
}

TEST_F(CookingStateMachine, TriggerOnEmptyActionsReturnsFalse)
{
    StateMachine sm2({{"Idle", {}}});
    EXPECT_FALSE(sm2.triggerSelectedAction());
}

// ─── Spice path transitions ───────────────────────────────────────────────────

TEST_F(CookingStateMachine, SelectSpiceTransition)
{
    // action 0 = "select spice"
    ASSERT_EQ(sm.selectedActionIndex(), 0u);
    ASSERT_TRUE(sm.triggerSelectedAction());
    EXPECT_EQ(sm.currentState().name, "Selecting spice");
    EXPECT_EQ(sm.selectedActionIndex(), 0u);
}

TEST_F(CookingStateMachine, SelectingSpiceHasNineOptions)
{
    sm.triggerSelectedAction(); // Init → Selecting spice
    EXPECT_EQ(sm.currentState().actions.size(), 9u);
}

TEST_F(CookingStateMachine, SelectingSpiceToSpiceSelected)
{
    sm.triggerSelectedAction(); // Init → Selecting spice
    sm.triggerSelectedAction(); // Selecting spice → Spice selected
    EXPECT_EQ(sm.currentState().name, "Spice selected");
}

TEST_F(CookingStateMachine, SpiceSelectedShakeTransition)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    // action 0 = "shake into pot"
    ASSERT_EQ(sm.currentState().actions[0], "shake into pot");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.currentState().name, "Shaking spice into pot");
}

TEST_F(CookingStateMachine, SpiceSelectedPutDownReturnsInit)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.selectNextAction();      // → "put down"
    ASSERT_EQ(sm.currentState().actions[sm.selectedActionIndex()], "put down");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.currentState().name, "Init");
}

// ─── Shaking spice side-effects ───────────────────────────────────────────────

TEST_F(CookingStateMachine, MoreSpiceIncrementsLevel)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.triggerSelectedAction(); // → Shaking spice into pot
    ASSERT_EQ(sm.spiceLevel(), 1);
    // action 0 = "more spice"
    ASSERT_EQ(sm.currentState().actions[0], "more spice");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.spiceLevel(), 2);
}

TEST_F(CookingStateMachine, LessSpiceDecrementsLevel)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.triggerSelectedAction(); // → Shaking spice into pot
    sm.triggerSelectedAction(); // more spice → level 2
    sm.selectNextAction();      // → "less spice"
    ASSERT_EQ(sm.currentState().actions[sm.selectedActionIndex()], "less spice");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.spiceLevel(), 1);
}

TEST_F(CookingStateMachine, SpiceLevelClampsAtMax)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.triggerSelectedAction(); // → Shaking spice into pot
    for (int i = 0; i < 10; ++i) {
        sm.triggerSelectedAction(); // "more spice" repeatedly
    }
    EXPECT_EQ(sm.spiceLevel(), 5);
}

TEST_F(CookingStateMachine, SpiceLevelClampsAtMin)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.triggerSelectedAction(); // → Shaking spice into pot
    sm.selectNextAction();      // → "less spice"
    for (int i = 0; i < 10; ++i) {
        sm.triggerSelectedAction();
    }
    EXPECT_EQ(sm.spiceLevel(), 1);
}

TEST_F(CookingStateMachine, SpiceLevelResetsOnReentry)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.triggerSelectedAction(); // → Shaking spice into pot
    sm.triggerSelectedAction(); // more spice → level 2
    // put down → Init, then re-enter Shaking spice into pot
    sm.selectNextAction(); sm.selectNextAction(); // → "put down"
    sm.triggerSelectedAction(); // → Init
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.triggerSelectedAction(); // → Shaking spice into pot (re-entry)
    EXPECT_EQ(sm.spiceLevel(), 1);
}

TEST_F(CookingStateMachine, ShakingSpicePutDownReturnsInit)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.triggerSelectedAction(); // → Spice selected
    sm.triggerSelectedAction(); // → Shaking spice into pot
    // Navigate to "put down" (index 2)
    sm.selectNextAction(); sm.selectNextAction();
    ASSERT_EQ(sm.currentState().actions[sm.selectedActionIndex()], "put down");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.currentState().name, "Init");
}

// ─── Utensil path transitions ─────────────────────────────────────────────────

TEST_F(CookingStateMachine, SelectUtensilTransition)
{
    sm.selectNextAction(); // → "select utensil"
    ASSERT_EQ(sm.currentState().actions[sm.selectedActionIndex()], "select utensil");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.currentState().name, "Selecting utensil");
}

TEST_F(CookingStateMachine, SelectingUtensilHasFiveOptions)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    EXPECT_EQ(sm.currentState().actions.size(), 5u);
}

TEST_F(CookingStateMachine, SelectingUtensilToUtensilSelected)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    EXPECT_EQ(sm.currentState().name, "Utensil selected");
}

TEST_F(CookingStateMachine, UtensilSelectedUseUtensilTransition)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    ASSERT_EQ(sm.currentState().actions[0], "use utensil");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.currentState().name, "Using utensil");
}

TEST_F(CookingStateMachine, UtensilSelectedPutDownReturnsInit)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.selectNextAction();      // → "put down"
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.currentState().name, "Init");
}

// ─── Using utensil side-effects ───────────────────────────────────────────────

TEST_F(CookingStateMachine, StirFasterIncrementsSpeed)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.triggerSelectedAction(); // → Using utensil
    ASSERT_EQ(sm.stirSpeed(), 1);
    ASSERT_EQ(sm.currentState().actions[0], "stir faster");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.stirSpeed(), 2);
}

TEST_F(CookingStateMachine, StirSlowerDecrementsSpeed)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.triggerSelectedAction(); // → Using utensil
    sm.triggerSelectedAction(); // stir faster → speed 2
    sm.selectNextAction();      // → "stir slower"
    ASSERT_EQ(sm.currentState().actions[sm.selectedActionIndex()], "stir slower");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.stirSpeed(), 1);
}

TEST_F(CookingStateMachine, StirSpeedClampsAtMax)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.triggerSelectedAction(); // → Using utensil
    for (int i = 0; i < 10; ++i) {
        sm.triggerSelectedAction(); // stir faster
    }
    EXPECT_EQ(sm.stirSpeed(), 5);
}

TEST_F(CookingStateMachine, StirSpeedClampsAtMin)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.triggerSelectedAction(); // → Using utensil
    sm.selectNextAction();      // → "stir slower"
    for (int i = 0; i < 10; ++i) {
        sm.triggerSelectedAction();
    }
    EXPECT_EQ(sm.stirSpeed(), 1);
}

TEST_F(CookingStateMachine, StirSpeedResetsOnReentry)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.triggerSelectedAction(); // → Using utensil
    sm.triggerSelectedAction(); // stir faster → speed 2
    // put down
    sm.selectNextAction(); sm.selectNextAction();
    sm.triggerSelectedAction(); // → Init
    // Re-enter Using utensil
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.triggerSelectedAction(); // → Using utensil
    EXPECT_EQ(sm.stirSpeed(), 1);
}

TEST_F(CookingStateMachine, UsingUtensilPutDownReturnsInit)
{
    sm.selectNextAction();
    sm.triggerSelectedAction(); // → Selecting utensil
    sm.triggerSelectedAction(); // → Utensil selected
    sm.triggerSelectedAction(); // → Using utensil
    sm.selectNextAction(); sm.selectNextAction(); // → "put down"
    ASSERT_EQ(sm.currentState().actions[sm.selectedActionIndex()], "put down");
    sm.triggerSelectedAction();
    EXPECT_EQ(sm.currentState().name, "Init");
}

// ─── Reset ────────────────────────────────────────────────────────────────────

TEST_F(CookingStateMachine, ResetReturnsToInit)
{
    sm.triggerSelectedAction(); // → Selecting spice
    sm.reset();
    EXPECT_EQ(sm.currentState().name, "Init");
    EXPECT_EQ(sm.selectedActionIndex(), 0u);
    EXPECT_EQ(sm.spiceLevel(), 1);
    EXPECT_EQ(sm.stirSpeed(), 1);
}

// ─── States list ──────────────────────────────────────────────────────────────

TEST_F(CookingStateMachine, BuildCookingUiStatesHasSevenStates)
{
    EXPECT_EQ(sm.states().size(), 7u);
}

TEST_F(CookingStateMachine, StateNamesInOrder)
{
    const auto& states = sm.states();
    ASSERT_GE(states.size(), 7u);
    EXPECT_EQ(states[0].name, "Init");
    EXPECT_EQ(states[1].name, "Selecting spice");
    EXPECT_EQ(states[2].name, "Spice selected");
    EXPECT_EQ(states[3].name, "Shaking spice into pot");
    EXPECT_EQ(states[4].name, "Selecting utensil");
    EXPECT_EQ(states[5].name, "Utensil selected");
    EXPECT_EQ(states[6].name, "Using utensil");
}

// ─── Custom states ────────────────────────────────────────────────────────────

TEST(StateMachineCustom, NoActionsStateDoesNotTransition)
{
    StateMachine sm(std::vector<StateDefinition>{{"A", {}}, {"B", {"go"}}});
    EXPECT_FALSE(sm.triggerSelectedAction());
    EXPECT_EQ(sm.currentState().name, "A");
}

TEST(StateMachineCustom, UnknownActionReturnsNullopt)
{
    // A state whose action has no mapping → triggerSelectedAction returns false
    StateMachine sm(std::vector<StateDefinition>{{"X", {"unknown_action"}}});
    EXPECT_FALSE(sm.triggerSelectedAction()); // no nextState registered
}
