#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

const char *DATA_FILE = "behavior_data.txt";

// Range for the point value picked in the "Add Choice" popup.
// Good choices go from +1 to +10, wrong choices from -1 to -10.
const int MIN_CHOICE_POINTS = 1;
const int MAX_CHOICE_POINTS = 10;

// A single behavior button shown under "Good Choices" or "Wrong Choices".
// "points" is always stored as a positive amount; "isGood" decides whether
// clicking the button adds or subtracts it from currentPoints.
struct BehaviorChoice
{
    std::string name;
    int points;
    bool isGood;
};

// Buttons shown the first time the app runs (or when loading an older
// behavior_data.txt that only contains the point total).
std::vector<BehaviorChoice> getDefaultChoices()
{
    return {
        {"Completed Morning Routine", 5, true},
        {"Followed Directions", 2, true},
        {"Completed Schoolwork", 5, true},
        {"Negative Behavior", 3, false},
        {"Incomplete Schoolwork", 5, false},
    };
}

// behavior_data.txt format:
//   line 1:        current points
//   each line after: GOOD|<points>|<name>   or   WRONG|<points>|<name>
void loadBehaviorData(int &currentPoints, std::vector<BehaviorChoice> &choices)
{
    currentPoints = 0;
    choices.clear();

    std::ifstream inputFile(DATA_FILE);
    if (inputFile.is_open())
    {
        std::string line;
        if (std::getline(inputFile, line))
        {
            std::istringstream(line) >> currentPoints;
        }

        while (std::getline(inputFile, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            size_t firstBar = line.find('|');
            size_t secondBar = (firstBar == std::string::npos) ? std::string::npos : line.find('|', firstBar + 1);
            if (secondBar == std::string::npos)
            {
                continue; // Not a choice line, skip it
            }

            std::string type = line.substr(0, firstBar);
            std::string name = line.substr(secondBar + 1);
            int points = 0;
            if ((type != "GOOD" && type != "WRONG") || name.empty() ||
                !(std::istringstream(line.substr(firstBar + 1, secondBar - firstBar - 1)) >> points))
            {
                continue; // Malformed line, skip it
            }

            choices.push_back({name, points, type == "GOOD"});
        }
        inputFile.close();
    }

    if (choices.empty())
    {
        choices = getDefaultChoices();
    }
}

void saveBehaviorData(int currentPoints, const std::vector<BehaviorChoice> &choices)
{
    std::ofstream outputFile(DATA_FILE);
    if (!outputFile.is_open())
    {
        std::cout << "Failed to save " << DATA_FILE << "!" << std::endl;
        return;
    }

    outputFile << currentPoints << "\n";
    for (const BehaviorChoice &choice : choices)
    {
        outputFile << (choice.isGood ? "GOOD" : "WRONG") << "|"
                   << choice.points << "|"
                   << choice.name << "\n";
    }
    outputFile.close();
}

// Draws every button for one category (good or wrong) and applies its
// points to currentPoints when clicked.
void drawChoiceButtons(std::vector<BehaviorChoice> &choices, bool isGood, int &currentPoints)
{
    for (size_t i = 0; i < choices.size(); i++)
    {
        const BehaviorChoice &choice = choices[i];
        if (choice.isGood != isGood)
        {
            continue;
        }

        // PushID keeps buttons unique even if two share the same name
        ImGui::PushID((int)i);
        std::string label = choice.name + (isGood ? " (+" : " (-") + std::to_string(choice.points) + ")";
        if (ImGui::Button(label.c_str(), ImVec2(560, 65)))
        {
            currentPoints += isGood ? choice.points : -choice.points;
        }
        ImGui::PopID();
    }
}

// Draws a "[-]  value  [+]" control for picking how many points a new choice
// is worth. "points" is the amount (1-10). For good choices the value is
// shown as +points, for wrong choices as -points. In both cases "+" makes the
// choice worth more (more reward / more penalty) and "-" makes it worth less.
void drawPointsStepper(int &points, bool isGood, float totalWidth)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const float buttonSize = ImGui::GetFrameHeight();
    const float gap = style.ItemInnerSpacing.x;
    const float valueWidth = totalWidth - (buttonSize + gap) * 2;

    // "-" button: less reward / less penalty, down to 1
    ImGui::BeginDisabled(points <= MIN_CHOICE_POINTS);
    if (ImGui::Button("-##lessPoints", ImVec2(buttonSize, buttonSize)))
    {
        points--;
    }
    ImGui::EndDisabled();

    // Value display, colored green (+) or red (-). It's a button with all of
    // its colors set to the field background so it looks like a read-only box.
    ImGui::SameLine(0, gap);
    std::string valueText = (isGood ? "+" : "-") + std::to_string(points) + "##pointsValue";
    ImVec4 fieldColor = style.Colors[ImGuiCol_FrameBg];
    ImGui::PushStyleColor(ImGuiCol_Button, fieldColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, fieldColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, fieldColor);
    ImGui::PushStyleColor(
        ImGuiCol_Text,
        isGood ? ImVec4(0.35f, 0.85f, 0.55f, 1.0f) : ImVec4(1.00f, 0.45f, 0.45f, 1.0f));
    ImGui::Button(valueText.c_str(), ImVec2(valueWidth, buttonSize));
    ImGui::PopStyleColor(4);

    // "+" button: more reward / more penalty, up to 10
    ImGui::SameLine(0, gap);
    ImGui::BeginDisabled(points >= MAX_CHOICE_POINTS);
    if (ImGui::Button("+##morePoints", ImVec2(buttonSize, buttonSize)))
    {
        points++;
    }
    ImGui::EndDisabled();

    // Safety clamp in case the value was ever set out of range
    if (points < MIN_CHOICE_POINTS)
    {
        points = MIN_CHOICE_POINTS;
    }
    if (points > MAX_CHOICE_POINTS)
    {
        points = MAX_CHOICE_POINTS;
    }
}

// Draws an "Add Good Choice" / "Add Wrong Choice" button. Clicking it opens a
// popup where the user enters the choice name and its point value. The new
// button is added to the list and immediately saved to behavior_data.txt.
void drawAddChoiceButton(bool isGood, std::vector<BehaviorChoice> &choices, int currentPoints)
{
    const char *buttonLabel = isGood ? "Add Good Choice" : "Add Wrong Choice";
    const char *popupTitle = isGood ? "New Good Choice" : "New Wrong Choice";

    // Each category keeps its own input buffers
    static char nameInput[2][128] = {};
    static int pointsInput[2] = {MIN_CHOICE_POINTS, MIN_CHOICE_POINTS};
    char *name = nameInput[isGood ? 0 : 1];
    int &points = pointsInput[isGood ? 0 : 1];

    if (ImGui::Button(buttonLabel, ImVec2(560, 65)))
    {
        name[0] = '\0';
        points = MIN_CHOICE_POINTS; // Starts at +1 for good choices, -1 for wrong choices
        ImGui::OpenPopup(popupTitle);
    }

    // Center the popup on screen
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(),
        ImGuiCond_Appearing,
        ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Choice name:");
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(560);
        // Note: pressing Enter here does NOT add the choice; only the "Add"
        // button does, so the user always gets a chance to set the points.
        ImGui::InputTextWithHint(
            "##name", isGood ? "e.g. Cleaned Room" : "e.g. Talked Back", name, sizeof(nameInput[0]));

        ImGui::Text(isGood ? "Points to add:" : "Points to subtract:");
        drawPointsStepper(points, isGood, 560);

        // Trim leading/trailing spaces from the name
        std::string trimmedName = name;
        size_t start = trimmedName.find_first_not_of(" \t");
        size_t end = trimmedName.find_last_not_of(" \t");
        trimmedName = (start == std::string::npos) ? "" : trimmedName.substr(start, end - start + 1);
        bool canAdd = !trimmedName.empty();

        ImGui::Spacing();
        ImGui::BeginDisabled(!canAdd);
        bool addClicked = ImGui::Button("Add", ImVec2(270, 0));
        ImGui::EndDisabled();

        if (canAdd && addClicked)
        {
            choices.push_back({trimmedName, points, isGood});
            saveBehaviorData(currentPoints, choices);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(270, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

int main()
{
    int currentPoints = 0;
    std::vector<BehaviorChoice> choices;
    loadBehaviorData(currentPoints, choices);
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

    // Connect ImGui to GLFW and OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Main application loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

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

        ImGui::Text("Welcome to the Behavior Tracker!");
        ImGui::Text("Current Points: %d", currentPoints);

        ImGui::Separator();

        ImGui::Text("Good Choices");
        ImGui::Spacing();

        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.12f, 0.55f, 0.35f, 1.0f));
        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.18f, 0.70f, 0.45f, 1.0f));

        drawChoiceButtons(choices, true, currentPoints);

        ImGui::PopStyleColor(2);

        drawAddChoiceButton(true, choices, currentPoints);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Wrong Choices");

        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(0.75f, 0.20f, 0.22f, 1.0f));

        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
        drawChoiceButtons(choices, false, currentPoints);

        ImGui::PopStyleColor(2);

        drawAddChoiceButton(false, choices, currentPoints);
        ImGui::Separator();
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
    // Save points and choice buttons to file
    saveBehaviorData(currentPoints, choices);

    // Shut down ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Shut down GLFW
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}