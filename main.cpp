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

// Custom behavior choices are stored separately from BehaviorLog's history file.
// This prevents the choice definitions from overwriting points and history.
static const char *CHOICES_FILE = "behavior_choices.txt";
static const int MIN_POINTS = 1;
static const int MAX_POINTS = 10;
static const ImVec2 kBehaviorButtonSize(560, 65);

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
                              bool isGood)
{
    for (size_t i = 0; i < choices.size(); ++i)
    {
        const BehaviorChoice &choice = choices[i];
        if (choice.isGood != isGood) continue;

        ImGui::PushID(static_cast<int>(i));
        const std::string label = makeChoiceLabel(choice);
        if (ImGui::Button(label.c_str(), kBehaviorButtonSize))
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
                               std::vector<BehaviorChoice> &choices)
{
    // These are management controls, not behavior buttons. Give them a
    // smaller, outlined, neutral look so they stand apart from the large
    // green/red buttons that record behavior.
    const char *buttonLabel = isGood
        ? "+  CREATE A GOOD CHOICE"
        : "+  CREATE A WRONG CHOICE";
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

    if (ImGui::Button(buttonLabel, ImVec2(560, 48)))
    {
        name[0] = '\0';
        points = defaultPoints(isGood);
        ImGui::OpenPopup(popupTitle);
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(popupTitle, nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Choice name:");
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(560);
        ImGui::InputTextWithHint("##name",
    isGood ? "e.g. Cleaned Room" : "e.g. Lying",
    name, sizeof(nameInput[0]));

        ImGui::Text(isGood ? "Points to add:" : "Points to subtract:");
        drawPointsStepper(points, isGood);
        ImGui::TextDisabled(
            isGood ? "+ rewards more, - rewards less (+1 to +10)"
                   : "+ penalises more, - penalises less (-1 to -10)");

        std::string trimmedName = name;
        const size_t first = trimmedName.find_first_not_of(" \\t");
        const size_t last = trimmedName.find_last_not_of(" \\t");
        trimmedName = (first == std::string::npos)
            ? "" : trimmedName.substr(first, last - first + 1);
        const bool canAdd = !trimmedName.empty();

        ImGui::Spacing();
        ImGui::BeginDisabled(!canAdd);
        const bool addClicked = ImGui::Button("Add", ImVec2(270, 0));
        ImGui::EndDisabled();

        if (canAdd && addClicked)
        {
            choices.push_back({trimmedName, clampPoints(points, isGood), isGood});
            saveChoices(choices);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(270, 0)) ||
            (!ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
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
            ImGui::Text("Welcome to the Behavior Tracker!");
            ImGui::Text("Current Points: %d", log.currentPoints());

            // History button sits on the same row as the points display
            ImGui::SameLine(0.0f, 60.0f);
            if (ImGui::Button("History", ImVec2(200, 55)))
            {
                screen = Screen::History;
            }
            ImGui::SameLine();
            if (ImGui::Button("Trends", ImVec2(200, 55)))
            {
                OpenTrendsScreen(screen);
            }

            ImGui::Separator();

            ImGui::Text("Good Choices");
            ImGui::Spacing();

            ImGui::PushStyleColor(
                ImGuiCol_Button, ImVec4(0.12f, 0.55f, 0.35f, 1.0f));
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.70f, 0.45f, 1.0f));

            drawChoiceButtons(log, choices, true);
            drawAddChoiceButton(true, choices);

            ImGui::PopStyleColor(2);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Wrong Choices");

            ImGui::PushStyleColor(
                ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.22f, 1.0f));
            ImGui::PushStyleColor(
                ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));

            drawChoiceButtons(log, choices, false);
            drawAddChoiceButton(false, choices);

            ImGui::PopStyleColor(2);
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