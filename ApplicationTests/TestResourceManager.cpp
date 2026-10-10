#include "Core/Initialization/App.h"
#include "Core/ResourceManager.h"
#include "Game/GameScene.h"
#include <gtest/gtest.h>
#include <chrono>
#include <optional>
#include <stdexcept>

namespace
{
class ScopedResourceHint
{
public:
    ScopedResourceHint(const char* name, const char* value) : name(name)
    {
        if (const char* previous = SDL_GetHint(name)) old = previous;
        SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE);
    }
    ~ScopedResourceHint()
    {
        SDL_ResetHint(name);
        if (old) SDL_SetHintWithPriority(name, old->c_str(), SDL_HINT_OVERRIDE);
    }
private:
    const char* name;
    std::optional<std::string> old;
};

class ResourceTestApp : public App
{
public:
    using App::Resources;
    SDL_Renderer* Renderer() const { return renderer.get(); }
};

class ResourceCache : public testing::Test
{
protected:
    void SetUp() override
    {
        root = std::filesystem::current_path();
        temporary = std::filesystem::temp_directory_path() /
            ("worms-resource-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(temporary);
        app.SetAssetRoot(root);
        ASSERT_NO_THROW(app.InitWindow("Resource test", 32, 32));
    }
    void TearDown() override
    {
        app.Clean();
        std::error_code error;
        std::filesystem::remove_all(temporary, error);
    }
    ScopedResourceHint video{SDL_HINT_VIDEODRIVER, "dummy"};
    ScopedResourceHint audio{SDL_HINT_AUDIODRIVER, "dummy"};
    ScopedResourceHint renderer{SDL_HINT_RENDER_DRIVER, "software"};
    ResourceTestApp app;
    std::filesystem::path root;
    std::filesystem::path temporary;
};

TEST_F(ResourceCache, ReusesTextureSoundAndMusicAcrossNormalizedAndAbsolutePaths)
{
    auto& cache = app.Resources();
    auto* texture = cache.GetTexture("worms.png");
    EXPECT_EQ(texture, cache.GetTexture("./unused/../worms.png"));
    EXPECT_EQ(texture, cache.GetTexture(root / "worms.png"));
    EXPECT_EQ(&cache.GetSound("jump.wav"), &cache.GetSound("./jump.wav"));
    EXPECT_EQ(&cache.GetMusic("Rick_Roll.ogg"), &cache.GetMusic(root / "Rick_Roll.ogg"));
    EXPECT_EQ(cache.Renderer(), app.Renderer());
    EXPECT_EQ(cache.AssetRoot(), root);
}

TEST_F(ResourceCache, FailedLoadsReportFilenameAndCanBeRetried)
{
    auto& cache = app.Resources();
    for (const auto* asset : {"worms.png", "jump.wav", "Rick_Roll.ogg"})
    {
        const auto missing = temporary / asset;
        try
        {
            if (missing.extension() == ".png") cache.GetTexture(missing);
            else if (missing.extension() == ".wav") cache.GetSound(missing);
            else cache.GetMusic(missing);
            FAIL() << "Missing asset was accepted: " << asset;
        }
        catch (const SDL_Exception& error)
        {
            EXPECT_NE(std::string(error.what()).find(asset), std::string::npos);
        }
        std::filesystem::copy_file(root / asset, missing);
        if (missing.extension() == ".png") EXPECT_NE(cache.GetTexture(missing), nullptr);
        else if (missing.extension() == ".wav") EXPECT_NO_THROW(cache.GetSound(missing));
        else EXPECT_NO_THROW(cache.GetMusic(missing));
    }
}

TEST_F(ResourceCache, RejectsEmptyPathsAndChangingLiveConfiguration)
{
    EXPECT_THROW(app.Resources().GetTexture(""), std::invalid_argument);
    EXPECT_THROW(app.Resources().GetSound(""), std::invalid_argument);
    EXPECT_THROW(app.Resources().GetMusic(""), std::invalid_argument);
    EXPECT_THROW(app.SetAssetRoot(temporary), std::logic_error);
    app.Clean();
    EXPECT_THROW(app.Resources(), std::logic_error);
    EXPECT_THROW(app.SetAssetRoot(""), std::invalid_argument);
    EXPECT_NO_THROW(app.SetAssetRoot(root));
    ASSERT_NO_THROW(app.InitWindow("Restart", 32, 32));
    EXPECT_NE(app.Resources().GetTexture("worms.png"), nullptr);
}

TEST_F(ResourceCache, AssetRootDoesNotFollowWorkingDirectoryChanges)
{
    struct RestoreDirectory
    {
        std::filesystem::path previous = std::filesystem::current_path();
        ~RestoreDirectory()
        {
            std::error_code error;
            std::filesystem::current_path(previous, error);
        }
    } restore;
    std::filesystem::current_path(temporary);
    auto& cache = app.Resources();
    EXPECT_NE(cache.GetTexture("worms.png"), nullptr);
    EXPECT_NO_THROW(cache.GetSound("jump.wav"));
    EXPECT_NO_THROW(cache.GetMusic("Rick_Roll.ogg"));
}

TEST_F(ResourceCache, CacheSurvivesSceneCleanupAndRejectsAnotherRenderer)
{
    auto& cache = app.Resources();
    auto* texture = cache.GetTexture("worms.png");
    auto* sound = &cache.GetSound("jump.wav");
    auto* music = &cache.GetMusic("Rick_Roll.ogg");
    GameScene scene(app.Renderer(), cache);
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(&scene.Context().resources, &cache);
    scene.CleanUp();
    EXPECT_EQ(texture, cache.GetTexture("worms.png"));
    EXPECT_EQ(sound, &cache.GetSound("jump.wav"));
    EXPECT_EQ(music, &cache.GetMusic("Rick_Roll.ogg"));
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(&scene.Context().resources, &cache);
    ResourceManager headless(nullptr, root);
    EXPECT_THROW(GameScene(app.Renderer(), headless), std::invalid_argument);
    EXPECT_THROW(headless.GetTexture("worms.png"), std::logic_error);
}

TEST_F(ResourceCache, CleanupStopsCachedPlaybackBeforeClosingPlatform)
{
    app.Resources().GetSound("jump.wav").Play();
    app.Resources().GetMusic("Rick_Roll.ogg").Play();
    EXPECT_GT(Mix_Playing(-1), 0);
    EXPECT_EQ(Mix_PlayingMusic(), 1);
    EXPECT_NO_THROW(app.Clean());
    EXPECT_EQ(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(SDL_WasInit(0), 0u);
    EXPECT_NO_THROW(app.Clean());
}
}
