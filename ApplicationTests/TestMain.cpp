#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>

namespace
{
class HeapIntegrityListener : public testing::EmptyTestEventListener
{
    void OnTestStart(const testing::TestInfo&) override
    {
        EXPECT_TRUE(_CrtCheckMemory()) << "CRT heap corruption before test";
    }
    void OnTestEnd(const testing::TestInfo&) override
    {
        EXPECT_TRUE(_CrtCheckMemory()) << "CRT heap corruption after test";
    }
};
}
#endif

int main(int argc, char** argv)
{
    SDL_SetMainReady();
    testing::InitGoogleTest(&argc, argv);
    const char* diagnostics = std::getenv("WORMS_CRT_DIAGNOSTICS");
    if (diagnostics && diagnostics[0] == '1')
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        for (int type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT})
        {
            _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
        }
        _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) |
                      _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
        testing::UnitTest::GetInstance()->listeners().Append(new HeapIntegrityListener);
        std::cout << "CRT diagnostics enabled: heap checks and exit-time leak report.\n";
#else
        std::cerr << "CRT diagnostics require an MSVC Debug build.\n";
        return 1;
#endif
    }
    int result = RUN_ALL_TESTS();
    if (SDL_WasInit(0) != 0)
    {
        std::cerr << "A test left SDL subsystems initialized.\n";
        result = 1;
    }
    // Timer/error APIs can allocate SDL state without initializing its main-thread lifecycle.
    // Starting the timer subsystem enables SDL_Quit to release that state, including TLS.
    if (SDL_Init(SDL_INIT_TIMER) != 0)
    {
        std::cerr << "Final SDL runtime initialization failed: " << SDL_GetError() << '\n';
        result = 1;
    }
    SDL_Quit();
    return result;
}
