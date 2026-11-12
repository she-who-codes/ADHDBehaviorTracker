#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "behavior_choice.h"
#include "behavior_log.h"
#include "history_view.h"
#include "trends_view.h"
#include "ui_layout.h"

// Custom behavior choices are stored separately from BehaviorLog's history file.
// This prevents the choice definitions from overwriting points and history.
static const char *CHOICES_FILE = "behavior_choices.txt";
static const int MIN_POINTS = 1;
static const int MAX_POINTS = 10;
static const float kBehaviorButtonHeight = 65.0f; // width follows the pane
static const float kCreateButtonHeight = 48.0f;
static const ImVec2 kChoicePanePadding(18, 18);
// The panes go side by side once each can be this wide (or narrower, if the
// labels are short). A longer label wraps onto a second line in its button
// instead of forcing the panes to stack.
static const float kChoicePaneSideBySideWidth = 420.0f;
static const char *kCreateGoodLabel = "+  CREATE A GOOD CHOICE";
static const char *kCreateWrongLabel = "+  CREATE A WRONG CHOICE";
// Smallest window the layout is designed for (well under half of a 1366x768
// screen). Below this, panes would be too small to use.
static const int kMinWindowWidth = 640;
static const int kMinWindowHeight = 400;

static int clampPoints(int points, bool isGood)
{
    int strength = (points < 0) ? -points : points;
    if (strength < MIN_POINTS) strength = MIN_POINTS;
    if (strength > MAX_POINTS) strength = MAX_POINTS;
    return isGood ? strength : -strength;
}

static int defaultPoints(bool isGood)
{
    return isGood ? MIN_POINTS : -MIN_POINTS;
}

static std::string makeChoiceLabel(const BehaviorChoice &choice)
{
    const int strength = (choice.points < 0) ? -choice.points : choice.points;
    return choice.name + " (" + (choice.points < 0 ? "-" : "+") +
           std::to_string(strength) + ")";
}

static std::vector<BehaviorChoice> getDefaultChoices()
{
    return {
        {"Completed Morning Routine", 5, true},
        {"Followed Directions", 2, true},
        {"Completed Schoolwork", 5, true},
        {"Negative Behavior", -3, false},
        {"Incomplete Schoolwork", -5, false}
    };
}

// File format: GOOD|strength|name or WRONG|strength|name.
// Names may contain spaces and additional '|' characters.
static void loadChoices(std::vector<BehaviorChoice> &choices)
{
    choices.clear();
    std::ifstream input(CHOICES_FILE);
    std::string line;

    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const size_t firstBar = line.find('|');
        const size_t secondBar = (firstBar == std::string::npos)
            ? std::string::npos : line.find('|', firstBar + 1);

        if (secondBar == std::string::npos) continue;

        const std::string type = line.substr(0, firstBar);
        const std::string name = line.substr(secondBar + 1);
        int strength = 0;

        if ((type != "GOOD" && type != "WRONG") || name.empty() ||
            !(std::istringstream(line.substr(firstBar + 1,
                secondBar - firstBar - 1)) >> strength))
            continue;

        const bool isGood = (type == "GOOD");
        choices.push_back({name, clampPoints(strength, isGood), isGood});
    }

    if (choices.empty())
        choices = getDefaultChoices();
}

static void saveChoices(const std::vector<BehaviorChoice> &choices)
{
    std::ofstream output(CHOICES_FILE);
    if (!output.is_open())
    {
        std::cerr << "Failed to save " << CHOICES_FILE << "!" << std::endl;
        return;
    }

    for (const BehaviorChoice &choice : choices)
    {
        const int strength = (choice.points < 0) ? -choice.points : choice.points;
        output << (choice.isGood ? "GOOD" : "WRONG") << "|"
               << strength << "|" << choice.name << "\n";
    }
}

// Splits 'label' into lines no wider than 'wrapWidth', breaking between words
// (or inside a word that is too long to fit on a line by itself).
static std::vector<std::string> wrapLabel(const std::string &label, float wrapWidth)
{
    std::vector<std::string> lines;
    ImFont *font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    const char *text = label.c_str();
    const char *end = text + label.size();

    while (text < end)
    {
        const char *lineEnd = font->CalcWordWrapPosition(fontSize, text, end, wrapWidth);
        if (lineEnd <= text) lineEnd = text + 1; // always make progress

        const char *trimmedEnd = lineEnd;
        while (trimmedEnd > text && trimmedEnd[-1] == ' ') --trimmedEnd;
        lines.emplace_back(text, trimmedEnd);

        text = lineEnd;
        while (text < end && *text == ' ') ++text;
    }
    return lines;
}

// Height of a behavior button: the usual height, or taller when its label
// wraps onto more lines than fit.
static float choiceButtonHeight(size_t lineCount)
{
    const float textHeight = ImGui::GetTextLineHeight() * static_cast<float>(lineCount);
    return std::max(kBehaviorButtonHeight,
                    textHeight + ImGui::GetStyle().FramePadding.y * 2.0f);
}

// Lines of a behavior button's label when the button is 'buttonWidth' wide.
static std::vector<std::string> choiceButtonLines(const BehaviorChoice &choice,
                                                  float buttonWidth)
{
    return wrapLabel(makeChoiceLabel(choice),
                     buttonWidth - ImGui::GetStyle().FramePadding.x * 2.0f);
}

// Total height of the buttons for one kind of choice, with spacing between.
static float choiceListContentHeight(const std::vector<BehaviorChoice> &choices,
                                     bool isGood, float buttonWidth)
{
    float height = 0.0f;
    int count = 0;
    for (const BehaviorChoice &choice : choices)
    {
        if (choice.isGood != isGood) continue;
        height += choiceButtonHeight(choiceButtonLines(choice, buttonWidth).size());
        ++count;
    }
    if (count > 1)
        height += ImGui::GetStyle().ItemSpacing.y * static_cast<float>(count - 1);
    return height;
}

// Every button is exactly 'buttonWidth' wide (the same in both panes). A label
// too long for one line wraps onto a second line (the button grows taller if
// it needs more), so labels are never cut off and the list never has to
// scroll sideways.
static void drawChoiceButtons(BehaviorLog &log,
                              const std::vector<BehaviorChoice> &choices,
                              bool isGood, float buttonWidth)
{
    const float lineHeight = ImGui::GetTextLineHeight();
    const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

    for (size_t i = 0; i < choices.size(); ++i)
    {
        const BehaviorChoice &choice = choices[i];
        if (choice.isGood != isGood) continue;

        const std::vector<std::string> lines = choiceButtonLines(choice, buttonWidth);
        const float buttonHeight = choiceButtonHeight(lines.size());

        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button("##choice", ImVec2(buttonWidth, buttonHeight)))
            log.record(choice.name, choice.points);
        ImGui::PopID();

        // The label, each line centered, the block centered vertically
        // (the same placement ImGui uses for a one-line button label).
        const ImVec2 buttonMin = ImGui::GetItemRectMin();
        ImDrawList *draw = ImGui::GetWindowDrawList();
        float y = buttonMin.y +
                  (buttonHeight - lineHeight * static_cast<float>(lines.size())) * 0.5f;
        for (const std::string &line : lines)
        {
            const float lineWidth = ImGui::CalcTextSize(line.c_str()).x;
            draw->AddText(ImVec2(buttonMin.x + (buttonWidth - lineWidth) * 0.5f, y),
                          textColor, line.c_str());
            y += lineHeight;
        }
    }
}

static void drawPointsStepper(int &points, bool isGood)
{
    const float buttonSize = ImGui::GetFrameHeight();
    const float valueWidth = 120.0f;
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    const float startX = ImGui::GetCursorPosX();
    int strength = (points < 0) ? -points : points;

    ImGui::BeginDisabled(strength <= MIN_POINTS);
    if (ImGui::Button("-", ImVec2(buttonSize, buttonSize))) --strength;
    ImGui::EndDisabled();

    char valueText[16];
    std::snprintf(valueText, sizeof(valueText), "%+d",
                  isGood ? strength : -strength);

    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + buttonSize + spacing +
        (valueWidth - ImGui::CalcTextSize(valueText).x) * 0.5f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(
        isGood ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f)
               : ImVec4(0.95f, 0.45f, 0.45f, 1.0f),
        "%s", valueText);

    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + buttonSize + spacing + valueWidth + spacing);
    ImGui::BeginDisabled(strength >= MAX_POINTS);
    if (ImGui::Button("+", ImVec2(buttonSize, buttonSize))) ++strength;
    ImGui::EndDisabled();

    points = clampPoints(strength, isGood);
}

static void drawAddChoiceButton(bool isGood,
                               std::vector<BehaviorChoice> &choices)
{
    // Uses the app's standard blue button colours, like History and Trends.
    const char *buttonLabel = isGood ? kCreateGoodLabel : kCreateWrongLabel;
    const char *popupTitle = isGood ? "New Good Choice" : "New Wrong Choice";

    static char nameInput[2][128] = {};
    static int pointsInput[2] = {defaultPoints(true), defaultPoints(false)};
    char *name = nameInput[isGood ? 0 : 1];
    int &points = pointsInput[isGood ? 0 : 1];

    // Fills the width of its pane, which follows the window size.
    if (ImGui::Button(buttonLabel, ImVec2(-FLT_MIN, kCreateButtonHeight)))
    {
        name[0] = '\0';
        points = defaultPoints(isGood);
        ImGui::OpenPopup(popupTitle);
    }

    ui::CenterNextPopup();

    if (ImGui::BeginPopupModal(popupTitle, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        // 560 wide normally; narrower when the app window is too small for
        // that, so the popup always fits on screen.
        const ImGuiStyle &style = ImGui::GetStyle();
        const float contentWidth =
            ui::FitPopupWidth(560.0f + style.WindowPadding.x * 2.0f, 320.0f) -
            style.WindowPadding.x * 2.0f;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + contentWidth);

        ImGui::Text("Choice name:");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(contentWidth);
        ImGui::InputTextWithHint("##name",
    isGood ? "e.g. Cleaned Room" : "e.g. Lying",
    name, sizeof(nameInput[0]));

        ImGui::Text(isGood ? "Points to add:" : "Points to subtract:");
        drawPointsStepper(points, isGood);
        ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("%s",
            isGood ? "+ rewards more, - rewards less (+1 to +10)"
                   : "+ penalises more, - penalises less (-1 to -10)");
        ImGui::PopStyleColor();

        std::string trimmedName = name;
        // Trim spaces and tabs. (This must be "\t", a tab: the old "\\t"
        // also stripped the letter 't', turning "pestering cat" into "pestering ca".)
        const size_t first = trimmedName.find_first_not_of(" \t");
        const size_t last = trimmedName.find_last_not_of(" \t");
        trimmedName = (first == std::string::npos)
            ? "" : trimmedName.substr(first, last - first + 1);
        const bool canAdd = !trimmedName.empty();

        ImGui::Spacing();
        ImGui::BeginDisabled(!canAdd);
        const float actionWidth = (contentWidth - style.ItemSpacing.x) * 0.5f;
        const bool addClicked = ImGui::Button("Add", ImVec2(actionWidth, 0));
        ImGui::EndDisabled();

        if (canAdd && addClicked)
        {
            choices.push_back({trimmedName, clampPoints(points, isGood), isGood});
            saveChoices(choices);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(actionWidth, 0)) ||
            (!ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape)))
            ImGui::CloseCurrentPopup();

        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }
}

// Width a pane needs to show all of its button labels on one line.
static float choicePaneMinWidth(const std::vector<BehaviorChoice> &choices,
                                bool isGood)
{
    float widest = ui::ButtonWidthFor(isGood ? kCreateGoodLabel : kCreateWrongLabel);
    for (const BehaviorChoice &choice : choices)
        if (choice.isGood == isGood)
            widest = std::max(widest, ui::ButtonWidthFor(makeChoiceLabel(choice).c_str()));
    return widest + kChoicePanePadding.x * 2.0f + ImGui::GetStyle().ScrollbarSize;
}

// Smallest pane height that still shows the title, two behavior buttons and
// the "create" button.
static float choicePaneMinHeight()
{
    const ImGuiStyle &style = ImGui::GetStyle();
    return kChoicePanePadding.y * 2.0f + ImGui::GetTextLineHeight() +
           kBehaviorButtonHeight * 2.0f + kCreateButtonHeight +
           style.ItemSpacing.y * 3.0f;
}

// Height of the scrolling list in a pane 'paneHeight' tall: everything below
// the title and above the "create" button.
static float choiceListHeight(float paneHeight)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    return std::max(paneHeight - kChoicePanePadding.y * 2.0f -
                        ImGui::GetTextLineHeight() - style.ItemSpacing.y -
                        kCreateButtonHeight - style.ItemSpacing.y,
                    kBehaviorButtonHeight);
}

// True if either list (good or wrong) in panes of 'paneSize' has more buttons
// than fit, so it needs a vertical scrollbar. Both panes then leave room for
// a scrollbar, which keeps their buttons exactly the same width.
static bool choiceListsNeedScrollbar(const std::vector<BehaviorChoice> &choices,
                                     const ImVec2 &paneSize)
{
    const float buttonWidth = paneSize.x - kChoicePanePadding.x * 2.0f -
                              ImGui::GetStyle().ScrollbarSize;
    const float listHeight = choiceListHeight(paneSize.y);
    return choiceListContentHeight(choices, true, buttonWidth) > listHeight ||
           choiceListContentHeight(choices, false, buttonWidth) > listHeight;
}

// One bordered pane: the title, the behavior buttons in their own scroll view,
// and the "create" button pinned underneath so it is always reachable.
static void drawChoicePane(const char *id, const char *title, bool isGood,
                           const ImVec2 &size, bool reserveScrollbar,
                           BehaviorLog &log, std::vector<BehaviorChoice> &choices)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kChoicePanePadding);
    // The contents are always submitted (even when the pane is scrolled out
    // of view) so the pane's "New ... Choice" popup keeps working.
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImGui::TextUnformatted(title);

    // The list fills the pane above the "create" button and scrolls when
    // there are more buttons than fit. (Long labels wrap, so it never needs
    // to scroll sideways.)
    const float buttonWidth = std::floor(
        ImGui::GetContentRegionAvail().x -
        (reserveScrollbar ? ImGui::GetStyle().ScrollbarSize : 0.0f));
    ImGui::BeginChild("List", ImVec2(0.0f, choiceListHeight(size.y)));

    if (isGood)
    {
        ImGui::PushStyleColor(
            ImGuiCol_Button, ImVec4(0.12f, 0.55f, 0.35f, 1.0f));
        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.70f, 0.45f, 1.0f));
    }
    else
    {
        ImGui::PushStyleColor(
            ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.22f, 1.0f));
        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
    }

    drawChoiceButtons(log, choices, isGood, buttonWidth);

    // Popped before the "create" button and its popup, so the popup's
    // buttons keep the app's standard blue instead of turning green/red.
    ImGui::PopStyleColor(2);
    ImGui::EndChild();

    drawAddChoiceButton(isGood, choices);

    ImGui::EndChild();
}

static void drawMainScreen(BehaviorLog &log,
                           std::vector<BehaviorChoice> &choices,
                           Screen &screen)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const ImVec2 navButtonSize(200, 55);

    ImGui::TextWrapped("Welcome to the Behavior Tracker!");
    ImGui::Text("Current Points: %d", log.currentPoints());

    // History and Trends sit on the same row as the points display, or
    // together on the next row when the window is too narrow for that.
    ui::SameLineIfFits(navButtonSize.x * 2.0f + style.ItemSpacing.x, 60.0f);
    if (ImGui::Button("History", navButtonSize))
    {
        screen = Screen::History;
    }
    ImGui::SameLine();
    if (ImGui::Button("Trends", navButtonSize))
    {
        OpenTrendsScreen(screen);
    }

    ImGui::Separator();

    // Good and wrong choices each get their own scrolling pane: side by side
    // when the window is wide enough for both, otherwise stacked.
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float paneMinWidth = std::min(
        kChoicePaneSideBySideWidth,
        std::max(choicePaneMinWidth(choices, true), choicePaneMinWidth(choices, false)));
    const float paneMinHeight = choicePaneMinHeight();
    const bool sideBySide = avail.x >= paneMinWidth * 2.0f + style.ItemSpacing.x;

    // Both panes are always exactly the same size.
    const ImVec2 paneSize =
        sideBySide
            ? ImVec2(std::floor((avail.x - style.ItemSpacing.x) * 0.5f),
                     std::max(avail.y, paneMinHeight))
            : ImVec2(std::floor(avail.x),
                     std::max(std::floor((avail.y - style.ItemSpacing.y) * 0.5f),
                              paneMinHeight));
    const bool reserveScrollbar = choiceListsNeedScrollbar(choices, paneSize);

    drawChoicePane("GoodChoices", "Good Choices", true, paneSize, reserveScrollbar,
                   log, choices);
    if (sideBySide)
        ImGui::SameLine();
    drawChoicePane("WrongChoices", "Wrong Choices", false, paneSize, reserveScrollbar,
                   log, choices);
}

int main()
{
    // Load the saved points and the behavior log; days older than the
    // 30-day history window are dropped automatically.
    BehaviorLog log("behavior_data.txt");
    log.load();

    std::vector<BehaviorChoice> choices;
    loadChoices(choices);
    // Create the separate choice file on first run, including default buttons.
    saveChoices(choices);

    Screen screen = Screen::Main;
    std::string selectedDate; // day shown on the DayDetail screen

    // When the clock passes midnight, re-save so the oldest day is pruned
    // from behavior_data.txt right away (even if no buttons are clicked).
    std::time_t nextMidnight = BehaviorLog::localMidnight(std::time(nullptr), 1);
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW!" << std::endl;
        return 1;
    }

    // Create the application window
    GLFWwindow *window = glfwCreateWindow(
        1920,
        1080,
        "Behavior Tracker",
        nullptr,
        nullptr);

    if (window == nullptr)
    {
        std::cout << "Failed to create window!" << std::endl;
        glfwTerminate();
        return 1;
    }

    // Don't let the window shrink below what the layout can handle.
    glfwSetWindowSizeLimits(window, kMinWindowWidth, kMinWindowHeight,
                            GLFW_DONT_CARE, GLFW_DONT_CARE);

    // Set the window as the current OpenGL context
    glfwMakeContextCurrent(window);

    // Enable V-Sync
    glfwSwapInterval(1);

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    // Set the UI theme
    ImGuiStyle &style = ImGui::GetStyle();

    style.WindowRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.ChildRounding = 10.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 8.0f;

    style.FramePadding = ImVec2(12, 10);
    style.ItemSpacing = ImVec2(12, 14);
    style.WindowPadding = ImVec2(24, 24);
    style.ScaleAllSizes(1.5f);

    ImVec4 *colors = style.Colors;

    colors[ImGuiCol_WindowBg] =
        ImVec4(0.08f, 0.09f, 0.16f, 1.00f);

    colors[ImGuiCol_TitleBg] =
        ImVec4(0.12f, 0.13f, 0.24f, 1.00f);

    colors[ImGuiCol_TitleBgActive] =
        ImVec4(0.18f, 0.16f, 0.35f, 1.00f);

    colors[ImGuiCol_Button] =
        ImVec4(0.30f, 0.35f, 0.75f, 1.00f);

    colors[ImGuiCol_ButtonHovered] =
        ImVec4(0.40f, 0.48f, 0.90f, 1.00f);

    colors[ImGuiCol_ButtonActive] =
        ImVec4(0.25f, 0.30f, 0.65f, 1.00f);

    colors[ImGuiCol_FrameBg] =
        ImVec4(0.16f, 0.18f, 0.29f, 1.00f);

    // Used by the collapsible day rows on the History screen
    colors[ImGuiCol_Header] =
        ImVec4(0.20f, 0.23f, 0.42f, 1.00f);

    colors[ImGuiCol_HeaderHovered] =
        ImVec4(0.28f, 0.33f, 0.58f, 1.00f);

    colors[ImGuiCol_HeaderActive] =
        ImVec4(0.25f, 0.30f, 0.65f, 1.00f);

    colors[ImGuiCol_TableHeaderBg] =
        ImVec4(0.14f, 0.16f, 0.28f, 1.00f);

    colors[ImGuiCol_TableRowBgAlt] =
        ImVec4(1.00f, 1.00f, 1.00f, 0.04f);

    // Connect ImGui to GLFW and OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Main application loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (std::time(nullptr) >= nextMidnight)
        {
            log.save(); // prunes by date, then writes the file
            nextMidnight = BehaviorLog::localMidnight(std::time(nullptr), 1);
        }

        // Start a new ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::GetIO().FontGlobalScale = 1.5f;
        // Create our UI window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

        ImGui::Begin(
            "Behavior Tracker",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse);

        if (screen == Screen::History)
        {
            DrawHistoryScreen(log, screen, selectedDate);
        }
        else if (screen == Screen::DayDetail)
        {
            DrawDayDetailScreen(log, screen, selectedDate);
        }
        else if (screen == Screen::Trends)
        {
            DrawTrendsScreen(log, choices, screen);
        }
        else
        {
            drawMainScreen(log, choices, screen);
        }

        ImGui::End();

        // Render the UI
        ImGui::Render();

        int displayWidth;
        int displayHeight;

        glfwGetFramebufferSize(
            window,
            &displayWidth,
            &displayHeight);

        glViewport(0, 0, displayWidth, displayHeight);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
    // Every click is already saved immediately; this is a final safety save.
    log.save();

    // Shut down ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Shut down GLFW
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}