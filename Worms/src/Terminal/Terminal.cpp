#include "Terminal.h"

#include <fstream>
#include <imgui.h>

void Terminal::Log(std::string&& message, LogLevel level)
{
    if (!logFile) [[unlikely]]
        return;
    static int i = 0;
    i++;
    const std::string logMessage = std::to_string(i) + ". " + getCurrentTime() + " [" +
                                   getLogLevelString(level) + "] " + message;
    Lines.emplace_back(logMessage);
    logFile << logMessage << '\n';
}

void Terminal::TurnOn()
{
    open = true;
}

void Terminal::Update() {}

void Terminal::Render()
{
    char input[20] = "";
    if (open)
    {
        ImGui::Begin("Terminal", &open);
        ImGui::BeginChild("history", {0, 140}, false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const std::string& line : Lines)
            ImGui::TextUnformatted(line.c_str());

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(0.0f);
        ImGui::EndChild();
        if (ImGui::InputText("Terminal", input, 20, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            Terminal::Get().Log(input, LogLevel::INFO);
            strcpy_s(input, "");
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::End();
    }
}

std::string Terminal::getCurrentTime()
{
    time_t now = time(0);
    struct tm timeInfo;
    char timeBuffer[20];

#ifdef _WIN32
    localtime_s(&timeInfo, &now);
#else
    localtime_r(&now, &timeInfo);
#endif

    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return std::string(timeBuffer);
}

std::string Terminal::getLogLevelString(LogLevel level)
{
    switch (level)
    {
    case INFO:
        return "INFO";
    case WARNING:
        return "WARNING";
    case ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}
