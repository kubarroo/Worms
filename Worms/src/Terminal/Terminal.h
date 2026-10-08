#pragma once
#include <fstream>
#include <string>
#include <vector>

enum LogLevel
{
    INFO,
    WARNING,
    ERROR
};

class Terminal
{
public:
    Terminal(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;

    static Terminal& Get()
    {
        static Terminal terminal{};
        return terminal;
    }

    void Log(std::string&& message, LogLevel level);

    void TurnOn();
    void Update();
    void Render();

private:
    std::string fileName = "logs.txt";
    std::vector<std::string> Lines;

    std::string getCurrentTime();
    std::string getLogLevelString(LogLevel level);
    std::ofstream logFile;

    bool open = false;

    Terminal()
    {
        Lines.reserve(10000);
        logFile.open(fileName);
    }
    ~Terminal()
    {
        logFile.close();
    }
};
