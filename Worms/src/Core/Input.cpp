#include "Core/Input.h"

void Input::UpdateInputsDown(const SDL_Event& ev)
{
    switch (ev.key.keysym.scancode)
    {
    case SDL_SCANCODE_W:
        vertical = 1.f;
        cameraControl = false;
        break;
    case SDL_SCANCODE_S:
        vertical = -1.f;
        cameraControl = false;
        break;
    case SDL_SCANCODE_A:
        horizontal = -1.f;
        cameraControl = false;
        break;
    case SDL_SCANCODE_D:
        horizontal = 1.f;
        cameraControl = false;
        break;
    case SDL_SCANCODE_UP:
        camera_vertical = 1.f;
        cameraControl = true;
        break;
    case SDL_SCANCODE_DOWN:
        camera_vertical = -1.f;
        cameraControl = true;
        break;
    case SDL_SCANCODE_LEFT:
        camera_horizontal = -1.f;
        cameraControl = true;
        break;
    case SDL_SCANCODE_RIGHT:
        camera_horizontal = 1.f;
        cameraControl = true;
        break;
        // case SDL_SCANCODE_TAB: change_worm = true; break;
        // case SDL_SCANCODE_T: change_team = true; break;
    case SDL_SCANCODE_LSHIFT:
        use_action = true;
        break;
    case SDL_SCANCODE_SPACE:
        jump = true;
        break;
    case SDL_SCANCODE_E:
        changeWeapon = 1;
        break;
    case SDL_SCANCODE_Q:
        changeWeapon = -1;
        break;
    default:
        break;
    }
}

void Input::UpdateInputsUp(const SDL_Event& ev)
{
    switch (ev.key.keysym.scancode)
    {
    case SDL_SCANCODE_W:
    case SDL_SCANCODE_S:
        vertical = 0.f;
        break;
    case SDL_SCANCODE_A:
    case SDL_SCANCODE_D:
        horizontal = 0.f;
        break;
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_DOWN:
        Input::Get().camera_vertical = 0.f;
        break;
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_RIGHT:
        Input::Get().camera_horizontal = 0.f;
        break;
        // case SDL_SCANCODE_TAB: change_worm = false; break;
        // case SDL_SCANCODE_T: change_team = false; break;
    case SDL_SCANCODE_LSHIFT:
        use_action = false;
        break;
    case SDL_SCANCODE_SPACE:
        jump = false;
        break;
    case SDL_SCANCODE_Q:
    case SDL_SCANCODE_E:
        changeWeapon = 0;
        break;
    default:
        break;
    }
}
