#include "Core/Audio/Audio.h"
#include "Core/Input.h"
#include "Core/Renderer2D.h"
#include "Core/SDLHandles.h"
#include "Core/SdlInputAdapter.h"
#include <SDL2/SDL.h>
#include <gtest/gtest.h>
#include <stdexcept>

namespace
{
SDL_Event Key(SDL_Scancode code, bool down, bool repeat = false)
{
    SDL_Event event{};
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.scancode = code;
    event.key.repeat = repeat;
    return event;
}
SDL_Event Focus(bool gained)
{
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.event = gained ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST;
    return event;
}
} // namespace

TEST(InputActions, OppositeKeysCancelAndReleasingOneRestoresTheOther)
{
    Input input;
    SdlInputAdapter adapter(input);
    adapter.BeginFrame(false);
    adapter.ProcessEvent(Key(SDL_SCANCODE_A, true), false);
    EXPECT_FLOAT_EQ(input.Horizontal(), -1);
    adapter.ProcessEvent(Key(SDL_SCANCODE_D, true), false);
    EXPECT_FLOAT_EQ(input.Horizontal(), 0);
    adapter.ProcessEvent(Key(SDL_SCANCODE_D, false), false);
    EXPECT_FLOAT_EQ(input.Horizontal(), -1);
    adapter.ProcessEvent(Key(SDL_SCANCODE_A, false), false);
    EXPECT_FLOAT_EQ(input.Horizontal(), 0);
}

TEST(InputActions, QuickTapKeepsBothEdgesAndCanBeConsumedOnlyOnce)
{
    Input input;
    SdlInputAdapter adapter(input);
    adapter.BeginFrame(false);
    adapter.ProcessEvent(Key(SDL_SCANCODE_E, true), false);
    adapter.ProcessEvent(Key(SDL_SCANCODE_E, false), false);
    EXPECT_FALSE(input.Held(InputAction::NextWeapon));
    EXPECT_TRUE(input.Pressed(InputAction::NextWeapon));
    EXPECT_TRUE(input.Released(InputAction::NextWeapon));
    EXPECT_EQ(input.ChangeWeapon(), 1);
    EXPECT_EQ(input.ChangeWeapon(), 0);
    adapter.BeginFrame(false);
    EXPECT_FALSE(input.Pressed(InputAction::NextWeapon));
    EXPECT_FALSE(input.Released(InputAction::NextWeapon));
}

TEST(InputActions, HeldActionsSurviveFramesButKeyboardRepeatDoesNotRetrigger)
{
    Input input;
    SdlInputAdapter adapter(input);
    adapter.ProcessEvent(Key(SDL_SCANCODE_E, true), false);
    EXPECT_EQ(input.ChangeWeapon(), 1);
    adapter.BeginFrame(false);
    adapter.ProcessEvent(Key(SDL_SCANCODE_E, true, true), false);
    EXPECT_TRUE(input.Held(InputAction::NextWeapon));
    EXPECT_EQ(input.ChangeWeapon(), 0);
    adapter.ProcessEvent(Key(SDL_SCANCODE_E, false), false);
    adapter.ProcessEvent(Key(SDL_SCANCODE_E, true), false);
    EXPECT_EQ(input.ChangeWeapon(), 1);
}

TEST(InputActions, FocusLossAndCaptureClearHeldAndPendingActions)
{
    Input input;
    SdlInputAdapter adapter(input);
    adapter.ProcessEvent(Key(SDL_SCANCODE_LSHIFT, true), false);
    adapter.ProcessEvent(Key(SDL_SCANCODE_E, true), false);
    adapter.ProcessEvent(Focus(false), false);
    EXPECT_FALSE(input.Enabled());
    EXPECT_FALSE(input.UseAction());
    EXPECT_EQ(input.ChangeWeapon(), 0);
    adapter.ProcessEvent(Focus(true), false);
    EXPECT_TRUE(input.Enabled());
    EXPECT_TRUE(input.Interrupted());
    EXPECT_FALSE(input.Released(InputAction::Fire));
    adapter.BeginFrame(false);
    EXPECT_FALSE(input.Interrupted());
    adapter.ProcessEvent(Key(SDL_SCANCODE_LSHIFT, true), false);
    adapter.BeginFrame(true);
    EXPECT_FALSE(input.Enabled());
    EXPECT_FALSE(input.UseAction());
    adapter.ProcessEvent(Key(SDL_SCANCODE_D, true), true);
    EXPECT_FLOAT_EQ(input.Horizontal(), 0);
    adapter.BeginFrame(false);
    adapter.ProcessEvent(Key(SDL_SCANCODE_LSHIFT, true, true), false);
    EXPECT_FALSE(input.UseAction());
}

TEST(InputActions, ApplicationsHaveIndependentInputState)
{
    Input first, second;
    first.SetAction(InputAction::Fire, true);
    first.SetAction(InputAction::CameraRight, true);
    EXPECT_TRUE(first.UseAction());
    EXPECT_TRUE(first.CameraControll());
    EXPECT_FALSE(second.UseAction());
    EXPECT_FALSE(second.CameraControll());
    first.SetAction(InputAction::Left, true);
    EXPECT_FALSE(first.CameraControll());
    first.SetAction(InputAction::Count, true);
    EXPECT_FALSE(first.Held(InputAction::Count));
}

TEST(InputActions, InterruptionCountSurvivesFramesAndReset)
{
    Input input;
    const auto initial = input.InterruptionCount();
    input.SetEnabled(false);
    const auto interrupted = input.InterruptionCount();
    EXPECT_EQ(interrupted, initial + 1);
    input.SetEnabled(false);
    EXPECT_EQ(input.InterruptionCount(), interrupted);
    input.SetEnabled(true);
    input.BeginFrame();
    input.Reset();
    EXPECT_EQ(input.InterruptionCount(), interrupted);
    input.SetEnabled(false);
    EXPECT_EQ(input.InterruptionCount(), interrupted + 1);
}

TEST(RenderAdapter, DrawsSlicesRotationAndLinesWithoutChangingTextureState)
{
    Sdl::SurfacePtr canvas(SDL_CreateRGBSurfaceWithFormat(0, 16, 16, 32, SDL_PIXELFORMAT_RGBA32));
    ASSERT_NE(canvas, nullptr);
    Sdl::RendererPtr native(SDL_CreateSoftwareRenderer(canvas.get()));
    ASSERT_NE(native, nullptr);
    Renderer2D renderer(native.get());
    Sdl::SurfacePtr source(SDL_CreateRGBSurfaceWithFormat(0, 2, 1, 32, SDL_PIXELFORMAT_RGBA32));
    ASSERT_NE(source, nullptr);
    SDL_Rect left{0, 0, 1, 1}, right{1, 0, 1, 1};
    ASSERT_EQ(SDL_FillRect(source.get(), &left, SDL_MapRGBA(source->format, 255, 0, 0, 255)), 0);
    ASSERT_EQ(SDL_FillRect(source.get(), &right, SDL_MapRGBA(source->format, 0, 255, 0, 255)), 0);
    Sdl::TexturePtr texture(SDL_CreateTextureFromSurface(native.get(), source.get()));
    ASSERT_NE(texture, nullptr);
    renderer.Clear({0, 0, 0, 255});
    EXPECT_EQ(renderer.TextureSize(texture.get()).x, 2);
    renderer.DrawSprite(texture.get(), {0, 0, 4, 4}, RenderRect{1, 0, 1, 1});
    renderer.DrawSprite(texture.get(), {8, 8, 4, 4}, {}, 180);
    renderer.DrawSprite(texture.get(), {0, 0, 0, 4});
    renderer.DrawLine({0, 6}, {4, 6}, {0, 0, 255, 255});
    renderer.Present();
    auto pixel = [&](int x, int y)
    {
        Uint8 r, g, b, a;
        const auto* row = reinterpret_cast<const Uint32*>(
            static_cast<const Uint8*>(canvas->pixels) + y * canvas->pitch);
        SDL_GetRGBA(row[x], canvas->format, &r, &g, &b, &a);
        return RenderColor{r, g, b, a};
    };
    EXPECT_EQ(pixel(1, 1).g, 255);
    EXPECT_EQ(pixel(1, 1).r, 0);
    EXPECT_EQ(pixel(1, 6).b, 255);
    EXPECT_EQ(pixel(8, 8).g, 255);
    EXPECT_EQ(pixel(11, 8).r, 255);
    Uint8 r, g, b, a;
    ASSERT_EQ(SDL_GetTextureColorMod(texture.get(), &r, &g, &b), 0);
    ASSERT_EQ(SDL_GetTextureAlphaMod(texture.get(), &a), 0);
    EXPECT_EQ(r, 255);
    EXPECT_EQ(g, 255);
    EXPECT_EQ(b, 255);
    EXPECT_EQ(a, 255);
}

TEST(RenderAdapter, HeadlessDrawingAndNullTexturesAreRejected)
{
    Renderer2D renderer(nullptr);
    EXPECT_THROW(renderer.Clear({}), std::logic_error);
    EXPECT_THROW(renderer.TextureSize(nullptr), std::invalid_argument);
    EXPECT_EQ(renderer.Native(), nullptr);
}

TEST(AudioAdapter, StoppingWithoutAnOpenDeviceIsSafe)
{
    Audio audio;
    EXPECT_NO_THROW(audio.StopAll());
    EXPECT_NO_THROW(audio.StopAll());
}
