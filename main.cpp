#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <iostream>

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW!" << std::endl;
        return 1;
    }

    // Create the application window
    GLFWwindow* window = glfwCreateWindow(
        800,
        600,
        "Behavior Tracker",
        nullptr,
        nullptr
    );

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

    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    // Set the UI theme
    ImGui::StyleColorsDark();

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

        // Create our UI window
        ImGui::Begin("Behavior Tracker");

        ImGui::Text("Welcome to the Behavior Tracker!");
        ImGui::Text("Current Points: 0");

        ImGui::Button("Add 5 Points");

        ImGui::End();

        // Render the UI
        ImGui::Render();

        int displayWidth;
        int displayHeight;

        glfwGetFramebufferSize(
            window,
            &displayWidth,
            &displayHeight
        );

        glViewport(0, 0, displayWidth, displayHeight);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Shut down ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Shut down GLFW
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}