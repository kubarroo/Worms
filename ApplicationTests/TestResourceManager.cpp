#include "Core/Initialization/App.h"
#include "Core/ResourceManager.h"
#include "TestPlatform.h"
#include "Game/GameScene.h"
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
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
    void InitWindow(const std::string& title, int width, int height) override
    {
        ConfigureTestDrivers();
        App::InitWindow(title, width, height);
    }
    using App::Resources;
    using App::Graphics;
    using App::AudioOutput;
    using App::Inputs;
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
    ScopedResourceHint video{SDL_HINT_VIDEO_DRIVER, "dummy"};
    ScopedResourceHint audio{SDL_HINT_AUDIO_DRIVER, "dummy"};
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

TEST_F(ResourceCache, CorruptFilesReportFilenameAndDoNotPoisonCache)
{
    auto& cache = app.Resources();
    for (const auto* asset : {"worms.png", "jump.wav", "Rick_Roll.ogg"})
    {
        SCOPED_TRACE(asset);
        const auto path = temporary / asset;
        {
            std::ofstream file(path, std::ios::binary);
            ASSERT_TRUE(file.is_open());
            file << "Not a valid image or audio file";
            ASSERT_TRUE(file.good());
        }
        const auto load = [&]
        {
            if (path.extension() == ".png") cache.GetTexture(path);
            else if (path.extension() == ".wav") cache.GetSound(path);
            else cache.GetMusic(path);
        };
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            try
            {
                load();
                FAIL() << "Corrupt asset was accepted";
            }
            catch (const SDL_Exception& error)
            {
                EXPECT_NE(std::string(error.what()).find(path.string()), std::string::npos);
            }
        }
        std::filesystem::copy_file(root / asset, path,
                                   std::filesystem::copy_options::overwrite_existing);
        ASSERT_NO_THROW(load());
        if (path.extension() == ".png")
            EXPECT_EQ(cache.GetTexture(path), cache.GetTexture(temporary / "." / asset));
        else if (path.extension() == ".wav")
            EXPECT_EQ(&cache.GetSound(path), &cache.GetSound(temporary / "." / asset));
        else
            EXPECT_EQ(&cache.GetMusic(path), &cache.GetMusic(temporary / "." / asset));
    }
}

TEST_F(ResourceCache, RecreatedScenesReuseCacheAndStopAudioBeforePlatformShutdown)
{
    auto& cache = app.Resources();
    auto* texture = cache.GetTexture("worms.png");
    auto& sound = cache.GetSound("jump.wav");
    auto& music = cache.GetMusic("Rick_Roll.ogg");
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        SCOPED_TRACE(cycle);
        {
            GameScene scene(app.Renderer(), cache, app.Graphics(), app.AudioOutput(), app.Inputs());
            ASSERT_NO_THROW(scene.Initialize());
            EXPECT_EQ(texture, scene.Context().resources.GetTexture("./worms.png"));
            EXPECT_EQ(&sound, &scene.Context().resources.GetSound("jump.wav"));
            EXPECT_EQ(&music, &scene.Context().resources.GetMusic("Rick_Roll.ogg"));
            ASSERT_GE(app.AudioOutput().Play(sound, -1), 0);
            ASSERT_NO_THROW(app.AudioOutput().Play(music, -1));
            EXPECT_GT(app.AudioOutput().PlayingSounds(), 0);
            EXPECT_EQ(app.AudioOutput().PlayingMusic(), 1);
        }
        EXPECT_EQ(app.AudioOutput().PlayingSounds(), 0);
        EXPECT_EQ(app.AudioOutput().PlayingMusic(), 0);
        EXPECT_NE(app.AudioOutput().IsOpen(), 0);
        EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO), 0u);
        EXPECT_NE(ImGui::GetCurrentContext(), nullptr);
        EXPECT_EQ(texture, cache.GetTexture("worms.png"));
        EXPECT_EQ(&sound, &cache.GetSound("jump.wav"));
        EXPECT_EQ(&music, &cache.GetMusic("Rick_Roll.ogg"));
    }
    ASSERT_GE(app.AudioOutput().Play(sound, -1), 0);
    ASSERT_NO_THROW(app.AudioOutput().Play(music, -1));
    EXPECT_NO_THROW(app.Clean());
    EXPECT_THROW(app.Resources(), std::logic_error);
    EXPECT_EQ(app.AudioOutput().IsOpen(), 0);
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
    EXPECT_EQ(SDL_WasInit(0), 0u);
    EXPECT_NO_THROW(app.Clean());
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
    GameScene scene(app.Renderer(), cache, app.Graphics(), app.AudioOutput(), app.Inputs());
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_NO_THROW(scene.Render());
    scene.CleanUp();
}

TEST_F(ResourceCache, CacheSurvivesSceneCleanupAndRejectsAnotherRenderer)
{
    auto& cache = app.Resources();
    auto* texture = cache.GetTexture("worms.png");
    auto* sound = &cache.GetSound("jump.wav");
    auto* music = &cache.GetMusic("Rick_Roll.ogg");
    GameScene scene(app.Renderer(), cache, app.Graphics(), app.AudioOutput(), app.Inputs());
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(&scene.Context().resources, &cache);
    scene.CleanUp();
    EXPECT_EQ(texture, cache.GetTexture("worms.png"));
    EXPECT_EQ(sound, &cache.GetSound("jump.wav"));
    EXPECT_EQ(music, &cache.GetMusic("Rick_Roll.ogg"));
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(&scene.Context().resources, &cache);
    ResourceManager headless(nullptr, root);
    EXPECT_THROW(GameScene(app.Renderer(), headless, app.Graphics(), app.AudioOutput(), app.Inputs()), std::invalid_argument);
    EXPECT_THROW(headless.GetTexture("worms.png"), std::logic_error);
}

TEST_F(ResourceCache, CleanupStopsCachedPlaybackBeforeClosingPlatform)
{
    app.AudioOutput().Play(app.Resources().GetSound("jump.wav"));
    app.AudioOutput().Play(app.Resources().GetMusic("Rick_Roll.ogg"));
    EXPECT_GT(app.AudioOutput().PlayingSounds(), 0);
    EXPECT_EQ(app.AudioOutput().PlayingMusic(), 1);
    EXPECT_NO_THROW(app.Clean());
    EXPECT_EQ(app.AudioOutput().IsOpen(), 0);
    EXPECT_EQ(SDL_WasInit(0), 0u);
    EXPECT_NO_THROW(app.Clean());
}

TEST_F(ResourceCache, AudioAdapterPlaysAndStopsBorrowedAssets)
{
    auto& audio = app.AudioOutput();
    auto& sound = app.Resources().GetSound("jump.wav");
    auto& music = app.Resources().GetMusic("Rick_Roll.ogg");
    ASSERT_GE(audio.Play(sound, -1), 0);
    ASSERT_NO_THROW(audio.Play(music, -1));
    EXPECT_GT(app.AudioOutput().PlayingSounds(), 0);
    EXPECT_EQ(app.AudioOutput().PlayingMusic(), 1);
    audio.StopSounds();
    EXPECT_EQ(app.AudioOutput().PlayingSounds(), 0);
    EXPECT_EQ(app.AudioOutput().PlayingMusic(), 1);
    audio.StopAll();
    EXPECT_EQ(app.AudioOutput().PlayingMusic(), 0);
    EXPECT_THROW(audio.Play(sound, 0), std::invalid_argument);
    EXPECT_THROW(audio.Play(music, -2), std::invalid_argument);
    EXPECT_NO_THROW(audio.SetSoundVolume(0.5f));
    EXPECT_NO_THROW(audio.SetMusicVolume(0.5f));
    EXPECT_THROW(audio.SetSoundVolume(-0.1f), std::invalid_argument);
    EXPECT_THROW(audio.SetMusicVolume(1.1f), std::invalid_argument);
    audio.SetSoundVolume(1.f);
    audio.SetMusicVolume(1.f);
}
TEST_F(ResourceCache, SoundTracksAreBoundedReusableAndDoNotStopMusic)
{
    auto& audio = app.AudioOutput();
    auto& sound = app.Resources().GetSound("jump.wav");
    audio.Play(app.Resources().GetMusic("Rick_Roll.ogg"), -1);
    for (int channel = 0; channel < 8; ++channel)
        EXPECT_EQ(audio.Play(sound, -1), channel);
    EXPECT_EQ(audio.Play(sound, -1), -1);
    EXPECT_THROW(audio.Play(sound, 0), std::invalid_argument);
    EXPECT_EQ(audio.PlayingSounds(), 8);
    audio.StopSounds();
    EXPECT_EQ(audio.PlayingSounds(), 0);
    EXPECT_TRUE(audio.PlayingMusic());
    EXPECT_EQ(audio.Play(sound, -1), 0);
}

TEST_F(ResourceCache, FinitePlaybackCompletesAndItsTrackCanBeReused)
{
    auto& audio = app.AudioOutput();
    auto& sound = app.Resources().GetSound("jump.wav");
    ASSERT_EQ(audio.Play(sound, 2), 0);
    const auto deadline = SDL_GetTicks() + 5000;
    while (audio.PlayingSounds() != 0 && SDL_GetTicks() < deadline)
        SDL_Delay(10);
    EXPECT_EQ(audio.PlayingSounds(), 0);
    EXPECT_EQ(audio.Play(sound, -1), 0);
    SDL_Delay(300);
    EXPECT_EQ(audio.PlayingSounds(), 1);
}

TEST_F(ResourceCache, SinglePlaybackResetsLoopingOptionsOnReusedTracks)
{
    auto& audio = app.AudioOutput();
    auto& sound = app.Resources().GetSound("jump.wav");
    auto& music = app.Resources().GetMusic("jump.wav");
    ASSERT_EQ(audio.Play(sound, -1), 0);
    audio.Play(music, -1);
    audio.StopAll();
    ASSERT_EQ(audio.Play(sound, 1), 0);
    audio.Play(music, 1);
    const auto deadline = SDL_GetTicks() + 5000;
    while ((audio.PlayingSounds() != 0 || audio.PlayingMusic()) && SDL_GetTicks() < deadline)
        SDL_Delay(10);
    EXPECT_EQ(audio.PlayingSounds(), 0);
    EXPECT_FALSE(audio.PlayingMusic());
    EXPECT_EQ(audio.Play(sound, -1), 0);
    audio.Play(music, -1);
    SDL_Delay(300);
    EXPECT_EQ(audio.PlayingSounds(), 1);
    EXPECT_TRUE(audio.PlayingMusic());
}

TEST_F(ResourceCache, MixersAreIndependentAndScenesRejectForeignAudio)
{
    auto& audio = app.AudioOutput();
    audio.Play(app.Resources().GetSound("jump.wav"), -1);
    {
        Audio other;
        other.Open();
        EXPECT_NE(other.Native(), audio.Native());
        EXPECT_THROW(other.Open(), std::logic_error);
        ResourceManager otherCache(app.Renderer(), root, other.Native());
        other.Play(otherCache.GetSound("jump.wav"), -1);
        EXPECT_THROW(GameScene(app.Renderer(), otherCache, app.Graphics(), audio, app.Inputs()),
                     std::invalid_argument);
        other.StopAll();
        EXPECT_EQ(other.PlayingSounds(), 0);
        EXPECT_GT(audio.PlayingSounds(), 0);
    }
    EXPECT_TRUE(audio.IsOpen());
    EXPECT_GT(audio.PlayingSounds(), 0);
    audio.StopAll();
    EXPECT_GE(audio.Play(app.Resources().GetSound("jump.wav"), -1), 0);
}

TEST_F(ResourceCache, SharedTexturesRetainNearestFiltering)
{
    SDL_ScaleMode mode;
    ASSERT_TRUE(SDL_GetTextureScaleMode(app.Resources().GetTexture("worms.png"), &mode));
    EXPECT_EQ(mode, SDL_SCALEMODE_NEAREST);
}
} // namespace
