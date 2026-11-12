#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
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
static const ImVec2 kBehaviorButtonSize(560, 65);
static const float kAddChoiceButtonHeight = 48.0f;
static const float kChoiceNameWidth = 560.0f;   // name box in the "New ... Choice" popup
static const float kPopupButtonWidth = 270.0f;  // "Add" / "Cancel"

// Good and Wrong Choices sit side by side when both columns fit, with this
// gap between them. A column is "too narrow" when it is narrower than its
// widest button label or than kMinChoiceColumnWidth; then they stack.
static const float kChoiceColumnGap = 60.0f;
static const float kMinChoiceColumnWidth = 320.0f;

// Vertical divider drawn in the middle of that gap when the columns are side
// by side: a rounded bar in the same muted blue-grey as the app's separators
// and panel borders, but solid and thicker so it reads clearly.
static const ImVec4 kChoiceColumnDivider(0.43f, 0.43f, 0.50f, 1.0f);
static const float kChoiceColumnDividerWidth = 3.0f;

// Each column's behavior buttons are in their own scroll area, so a long list
// of Good Choices never pushes Wrong Choices off the screen. The area is as
// tall as its buttons until the window runs out of room, then it scrolls.
// It always shows at least this many buttons (the screen scrolls if needed).
static const float kMinVisibleChoiceButtons = 1.5f;

// Set when a choice is added, so its list scrolls down to show it.
// Index 0 = Good Choices, 1 = Wrong Choices.
static bool g_scrollToNewChoice[2] = {false, false};

static const char *addChoiceLabel(bool isGood)
{
    return isGood ? "+  CREATE A GOOD CHOICE" : "+  CREATE A WRONG CHOICE";
}

// Width for the big buttons on the main screen: their normal width when the
// column is wide enough, narrower when it isn't, but never so narrow that the
// label is cut off (the window scrolls sideways instead).
static float mainButtonWidth(const char *label, float columnWidth)
{
    float width = columnWidth;
    if (width > kBehaviorButtonSize.x) width = kBehaviorButtonSize.x;
    const float labelWidth = UiLayout::ButtonLabelWidth(label);
    return (width < labelWidth) ? labelWidth : width;
}

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

static void drawChoiceButtons(BehaviorLog &log,
                              const std::vector<BehaviorChoice> &choices,
                              bool isGood, float columnWidth)
{
    for (size_t i = 0; i < choices.size(); ++i)
    {
        const BehaviorChoice &choice = choices[i];
        if (choice.isGood != isGood) continue;

        ImGui::PushID(static_cast<int>(i));
        const std::string label = makeChoiceLabel(choice);
        const ImVec2 size(mainButtonWidth(label.c_str(), columnWidth), kBehaviorButtonSize.y);
        if (ImGui::Button(label.c_str(), size))
            log.record(choice.name, choice.points);
        ImGui::PopID();
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
                               std::vector<BehaviorChoice> &choices,
                               float columnWidth)
{
    // These are management controls, not behavior buttons. Give them a
    // smaller, outlined, neutral look so they stand apart from the large
    // green/red buttons that record behavior.
    const char *buttonLabel = addChoiceLabel(isGood);
    const char *popupTitle = isGood ? "New Good Choice" : "New Wrong Choice";

    static char nameInput[2][128] = {};
    static int pointsInput[2] = {defaultPoints(true), defaultPoints(false)};
    char *name = nameInput[isGood ? 0 : 1];
    int &points = pointsInput[isGood ? 0 : 1];

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.34f, 0.48f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.17f, 0.27f, 0.39f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.65f, 0.78f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);

    if (ImGui::Button(buttonLabel,
                      ImVec2(mainButtonWidth(buttonLabel, columnWidth), kAddChoiceButtonHeight)))
    {
        name[0] = '\0';
        points = defaultPoints(isGood);
        ImGui::OpenPopup(popupTitle);
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // Keep the popup inside the app window, even a small one: centre it again
    // if the window is resized while it is open, narrow its contents to fit,
    // and let it scroll if the window is too short to show all of it.
    static ImVec2 lastDisplaySize[2] = {};
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImGuiStyle &style = ImGui::GetStyle();
    float contentWidth = viewport->WorkSize.x - style.WindowPadding.x * 2.0f -
                         style.ScrollbarSize - 2.0f * style.ItemSpacing.x;
    if (contentWidth > kChoiceNameWidth) contentWidth = kChoiceNameWidth;
    if (contentWidth < 200.0f) contentWidth = 200.0f;
    float popupButtonWidth = (contentWidth - style.ItemSpacing.x) * 0.5f;
    if (popupButtonWidth > kPopupButtonWidth) popupButtonWidth = kPopupButtonWidth;

    ImGui::SetNextWindowPos(viewport->GetCenter(),
                            UiLayout::CenterCondition(lastDisplaySize[isGood ? 0 : 1]),
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), viewport->WorkSize);

    if (ImGui::BeginPopupModal(popupTitle, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Choice name:");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(contentWidth);
        ImGui::InputTextWithHint("##name",
    isGood ? "e.g. Cleaned Room" : "e.g. Lying",
    name, sizeof(nameInput[0]));

        ImGui::Text(isGood ? "Points to add:" : "Points to subtract:");
        drawPointsStepper(points, isGood);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + contentWidth);
        ImGui::TextDisabled(
            isGood ? "+ rewards more, - rewards less (+1 to +10)"
                   : "+ penalises more, - penalises less (-1 to -10)");
        ImGui::PopTextWrapPos();

        std::string trimmedName = name;
        const size_t first = trimmedName.find_first_not_of(" \\t");
        const size_t last = trimmedName.find_last_not_of(" \\t");
        trimmedName = (first == std::string::npos)
            ? "" : trimmedName.substr(first, last - first + 1);
        const bool canAdd = !trimmedName.empty();

        ImGui::Spacing();
        ImGui::BeginDisabled(!canAdd);
        const bool addClicked = ImGui::Button("Add", ImVec2(popupButtonWidth, 0));
        ImGui::EndDisabled();

        if (canAdd && addClicked)
        {
            choices.push_back({trimmedName, clampPoints(points, isGood), isGood});
            saveChoices(choices);
            g_scrollToNewChoice[isGood ? 0 : 1] = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(popupButtonWidth, 0)) ||
            (!ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// Width the widest button on the main screen needs to show its whole label.
static float widestChoiceButton(const std::vector<BehaviorChoice> &choices)
{
    float widest = UiLayout::ButtonLabelWidth(addChoiceLabel(true));
    const float wrongAdd = UiLayout::ButtonLabelWidth(addChoiceLabel(false));
    if (wrongAdd > widest) widest = wrongAdd;
    for (const BehaviorChoice &choice : choices)
    {
        const float width = UiLayout::ButtonLabelWidth(makeChoiceLabel(choice).c_str());
        if (width > widest) widest = width;
    }
    return widest;
}

// Height of a column's behavior buttons when none are scrolled out of view
// (0 when it has none).
static float choiceListHeight(const std::vector<BehaviorChoice> &choices, bool isGood)
{
    int count = 0;
    for (const BehaviorChoice &choice : choices)
        if (choice.isGood == isGood) ++count;
    if (count == 0) return 0.0f;
    return static_cast<float>(count) * kBehaviorButtonSize.y +
           static_cast<float>(count - 1) * ImGui::GetStyle().ItemSpacing.y;
}

// Smallest height a column's scroll area gets (unless it needs less).
static float minChoiceListHeight()
{
    return kMinVisibleChoiceButtons * (kBehaviorButtonSize.y + ImGui::GetStyle().ItemSpacing.y);
}

// "Good Choices" or "Wrong Choices": title, behavior buttons (green/red) in
// their own scroll area at most maxListHeight tall, and the
// "+ CREATE A ... CHOICE" button, which stays visible below the list.
// Everything is at most columnWidth wide.
static void drawChoiceColumn(BehaviorLog &log, std::vector<BehaviorChoice> &choices,
                             bool isGood, float columnWidth, bool spacingAfterTitle,
                             float maxListHeight)
{
    ImGui::Text(isGood ? "Good Choices" : "Wrong Choices");
    if (spacingAfterTitle)
        ImGui::Spacing();

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

    const float listHeight = choiceListHeight(choices, isGood);
    if (listHeight > 0.0f)
    {
        // Room for the buttons plus a scrollbar beside them (it only shows
        // when the list is taller than the space for it); never so narrow
        // that a label is cut off.
        const ImGuiStyle &style = ImGui::GetStyle();
        float listWidth = kBehaviorButtonSize.x + style.ScrollbarSize;
        if (listWidth > columnWidth) listWidth = columnWidth;
        const float widest = widestChoiceButton(choices) + style.ScrollbarSize;
        if (listWidth < widest) listWidth = widest;

        const float height = (listHeight < maxListHeight) ? listHeight : maxListHeight;
        ImGui::BeginChild(isGood ? "GoodChoiceList" : "WrongChoiceList",
                          ImVec2(listWidth, height));
        // Buttons fill the list's width (minus its scrollbar when shown),
        // up to their normal width
        drawChoiceButtons(log, choices, isGood, ImGui::GetContentRegionAvail().x);
        bool &scrollToNew = g_scrollToNewChoice[isGood ? 0 : 1];
        if (scrollToNew)
        {
            ImGui::SetScrollHereY(1.0f); // show the choice that was just added
            scrollToNew = false;
        }
        ImGui::EndChild();
    }

    ImGui::PopStyleColor(2);

    // Drawn after popping the green/red, so the "New ... Choice" popup uses
    // the standard (blue) button colour.
    drawAddChoiceButton(isGood, choices, columnWidth);
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
    Screen shownScreen = screen; // screen drawn last frame
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

        // Each screen opens at the top, even if the previous one was
        // scrolled (screens can scroll when the window is small).
        if (screen != shownScreen)
        {
            ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
            shownScreen = screen;
        }

        ImGui::Begin(
            "Behavior Tracker",
            nullptr,
            ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_HorizontalScrollbar); // when the window is too narrow

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
            ImGui::Text("Welcome to the Behavior Tracker!");
            ImGui::Text("Current Points: %d", log.currentPoints());

            // History button sits on the same row as the points display
            // (the buttons move to the next row if the window is too narrow).
            // Decided as if the vertical scrollbar were showing, so whether
            // the row wraps can't depend on (and flip) the scrollbar.
            const float headerRight = UiLayout::RowRight() -
                ((ImGui::GetScrollMaxY() > 0.0f) ? 0.0f : style.ScrollbarSize);
            UiLayout::SameLineOrWrap(200.0f, 60.0f, -1.0f, headerRight);
            if (ImGui::Button("History", ImVec2(200, 55)))
            {
                screen = Screen::History;
            }
            UiLayout::SameLineOrWrap(200.0f, -1.0f, -1.0f, headerRight);
            if (ImGui::Button("Trends", ImVec2(200, 55)))
            {
                OpenTrendsScreen(screen);
            }

            ImGui::Separator();

            // Good Choices and Wrong Choices side by side when both columns
            // fit; one above the other when the window is too narrow.
            // The decision is made as if the vertical scrollbar were showing,
            // so switching layouts (which can add or remove the scrollbar)
            // can't make it flip back and forth.
            const float availableWidth = ImGui::GetContentRegionAvail().x;
            const float layoutWidth = availableWidth -
                ((ImGui::GetScrollMaxY() > 0.0f) ? 0.0f : style.ScrollbarSize);
            // (room for the widest label plus the list's scrollbar)
            float minColumnWidth = widestChoiceButton(choices) + style.ScrollbarSize;
            if (minColumnWidth < kMinChoiceColumnWidth) minColumnWidth = kMinChoiceColumnWidth;
            const bool sideBySide =
                (layoutWidth - kChoiceColumnGap) * 0.5f >= minColumnWidth;

            // Height available for the two lists of behavior buttons: the
            // rest of the window, minus everything else in the column(s):
            // titles, spacing, "+ CREATE" buttons and separators.
            const float spacingY = style.ItemSpacing.y;
            const float titleHeight = ImGui::GetTextLineHeight();
            const float availableHeight = ImGui::GetContentRegionAvail().y;
            const float minListHeight = minChoiceListHeight();
            const float goodListHeight = choiceListHeight(choices, true);
            const float wrongListHeight = choiceListHeight(choices, false);

            if (sideBySide)
            {
                float columnWidth = (availableWidth - kChoiceColumnGap) * 0.5f;
                const float listColumnWidth = columnWidth; // the list's scrollbar may use the gap
                if (columnWidth > kBehaviorButtonSize.x) columnWidth = kBehaviorButtonSize.x;
                const ImVec2 start = ImGui::GetCursorPos();
                const ImVec2 startOnScreen = ImGui::GetCursorScreenPos();

                // Each column: title, spacing, list, spacing x2, "+ CREATE",
                // then the separator below both columns.
                float maxListHeight = availableHeight -
                    (titleHeight + 5.0f * spacingY + kAddChoiceButtonHeight + 2.0f);
                if (maxListHeight < minListHeight) maxListHeight = minListHeight;

                ImGui::BeginGroup();
                drawChoiceColumn(log, choices, true, listColumnWidth, true, maxListHeight);
                ImGui::EndGroup();
                const float goodBottom = ImGui::GetCursorPosY();
                float columnsBottomOnScreen = ImGui::GetItemRectMax().y;

                // Second column starts level with the first (SetCursorPos
                // rather than SameLine, so both titles line up exactly)
                ImGui::SetCursorPos(ImVec2(start.x + columnWidth + kChoiceColumnGap, start.y));
                ImGui::BeginGroup();
                drawChoiceColumn(log, choices, false, listColumnWidth, true, maxListHeight);
                ImGui::EndGroup();

                if (ImGui::GetItemRectMax().y > columnsBottomOnScreen)
                    columnsBottomOnScreen = ImGui::GetItemRectMax().y;

                // Divider between the columns, from the titles down to the
                // bottom of the longer column
                const float dividerX =
                    startOnScreen.x + columnWidth + kChoiceColumnGap * 0.5f;
                const float halfWidth = kChoiceColumnDividerWidth * 0.5f;
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(dividerX - halfWidth, startOnScreen.y),
                    ImVec2(dividerX + halfWidth, columnsBottomOnScreen),
                    ImGui::GetColorU32(kChoiceColumnDivider), halfWidth);

                // Continue below whichever column is longer
                if (ImGui::GetCursorPosY() < goodBottom)
                    ImGui::SetCursorPosY(goodBottom);
            }
            else
            {
                // Stacked: the two lists share the height. Each gets half,
                // but a list that needs less than half gives the rest to the
                // other one.
                const float budget = availableHeight -
                    (2.0f * titleHeight + 11.0f * spacingY + 2.0f * kAddChoiceButtonHeight + 4.0f);
                float goodMax = goodListHeight;
                float wrongMax = wrongListHeight;
                if (goodListHeight + wrongListHeight > budget)
                {
                    const float half = budget * 0.5f;
                    if (goodListHeight <= half)
                        wrongMax = budget - goodListHeight;
                    else if (wrongListHeight <= half)
                        goodMax = budget - wrongListHeight;
                    else
                        goodMax = wrongMax = half;
                }
                if (goodMax < minListHeight) goodMax = minListHeight;
                if (wrongMax < minListHeight) wrongMax = minListHeight;

                drawChoiceColumn(log, choices, true, availableWidth, true, goodMax);

                ImGui::Spacing();
                ImGui::Separator();

                drawChoiceColumn(log, choices, false, availableWidth, false, wrongMax);
            }
            ImGui::Separator();
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