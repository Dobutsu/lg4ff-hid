/*
  wheeldemo: lg4ff-hid with an unmodified SDL3, for a Logitech G29 / G923 on Meta Quest
  (2D panel) or a desktop. Public domain (CC0).

  Shows steering, pedals and buttons, and tries the force feedback from the wheel's buttons:
    Cross / Circle   pull left / right while held
    Square           toggle the centering spring (30%)
    Triangle         rumble while held
    Plus / Minus     rotation range +90 / -90 degrees
  Everything is also logged with SDL_Log (logcat tag "SDL/APP").
*/
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "lg4ff_hid.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static LG4FF_Wheel *wheel;
static void *opened; /* handed over by the open thread */
static SDL_AtomicInt opener_running;
static LG4FF_State state, last;
static bool spring_on;
static char status[160] = "waiting for a wheel...";

/* lg4ff_open() blocks while Android shows the USB permission dialog, so poll from here */
static int SDLCALL open_thread(void *unused)
{
    (void)unused;
    for (;;) {
        LG4FF_Wheel *w = lg4ff_open();
        if (w) {
            SDL_SetAtomicPointer(&opened, w);
            break;
        }
        SDL_Delay(1000);
    }
    SDL_SetAtomicInt(&opener_running, 0);
    return 0;
}

static void start_opener(void)
{
    if (SDL_CompareAndSwapAtomicInt(&opener_running, 0, 1)) {
        SDL_DetachThread(SDL_CreateThread(open_thread, "lg4ff_open", NULL));
    }
}

static void on_press(uint32_t pressed, uint32_t released)
{
    if (pressed & LG4FF_BUTTON_CROSS) lg4ff_set_constant_force(wheel, -0.4f);
    if (pressed & LG4FF_BUTTON_CIRCLE) lg4ff_set_constant_force(wheel, 0.4f);
    if (released & (LG4FF_BUTTON_CROSS | LG4FF_BUTTON_CIRCLE)) lg4ff_set_constant_force(wheel, 0.0f);
    if (pressed & LG4FF_BUTTON_TRIANGLE) lg4ff_set_rumble(wheel, 0.4f);
    if (released & LG4FF_BUTTON_TRIANGLE) lg4ff_set_rumble(wheel, 0.0f);
    if (pressed & LG4FF_BUTTON_SQUARE) {
        spring_on = !spring_on;
        lg4ff_set_autocenter(wheel, spring_on ? 0.3f : 0.0f);
    }
    if (pressed & (LG4FF_BUTTON_PLUS | LG4FF_BUTTON_MINUS)) {
        lg4ff_set_range(wheel, lg4ff_get_range(wheel) + ((pressed & LG4FF_BUTTON_PLUS) ? 90 : -90));
    }
    for (int i = 0; i < 25; i++) {
        if ((pressed | released) & (1u << i)) {
            SDL_Log("button bit %d %s", i, (pressed & (1u << i)) ? "down" : "up");
        }
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)appstate; (void)argc; (void)argv;
    SDL_SetAppMetadata("lg4ff wheeldemo", "1.0", "io.github.lg4ffhid.wheeldemo");
    /* SDL3 on Android releases USB devices whenever the window loses focus, which Quest
       panels do constantly; keep the wheel while the app runs. */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    /* Keep SDL's own HIDAPI wheel driver (desktop) from opening the same device */
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_LG4FF, "0");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (lg4ff_init() != 0) {
        SDL_Log("lg4ff_init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (!SDL_CreateWindowAndRenderer("lg4ff wheeldemo", 1280, 720, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(renderer, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    start_opener();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *ev)
{
    (void)appstate;
    return ev->type == SDL_EVENT_QUIT ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

static void bar(float y, float frac, const char *label, bool centered)
{
    SDL_FRect bg = { 40, y, 1200, 50 };
    SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
    SDL_RenderFillRect(renderer, &bg);
    SDL_FRect fg = centered ? (SDL_FRect){ 640 + SDL_min(frac, 0.0f) * 600, y, SDL_fabsf(frac) * 600, 50 }
                            : (SDL_FRect){ 40, y, 1200 * frac, 50 };
    SDL_SetRenderDrawColor(renderer, 80, 160, 255, 255);
    SDL_RenderFillRect(renderer, &fg);
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderDebugText(renderer, 50, y + 21, label);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;
    char buf[160];

    if (!wheel) {
        wheel = (LG4FF_Wheel *)SDL_GetAtomicPointer(&opened);
        if (wheel) {
            SDL_SetAtomicPointer(&opened, NULL);
            lg4ff_set_range(wheel, 900);
            SDL_snprintf(status, sizeof(status), "%s open, range 900", lg4ff_name(wheel));
            SDL_Log("%s", status);
        }
    }
    if (wheel) {
        if (lg4ff_update(wheel, &state) < 0) {
            lg4ff_close(wheel);
            wheel = NULL;
            SDL_zero(state);
            SDL_strlcpy(status, "wheel gone, waiting...", sizeof(status));
            start_opener();
        } else if (state.buttons != last.buttons) {
            on_press(state.buttons & ~last.buttons, last.buttons & ~state.buttons);
        }
        last = state;
    }

    SDL_SetRenderDrawColor(renderer, 20, 22, 28, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderDebugText(renderer, 40, 30, "lg4ff-hid wheeldemo (stock SDL3)");
    SDL_RenderDebugText(renderer, 40, 50, status);
    SDL_RenderDebugText(renderer, 40, 70, "Cross/Circle pull L/R, Square spring, Triangle rumble, +/- range");
    if (wheel) {
        SDL_snprintf(buf, sizeof(buf), "steering %+.3f (%+.0f deg of %d)", state.steering,
                     state.steering * lg4ff_get_range(wheel) / 2, lg4ff_get_range(wheel));
        bar(110, state.steering, buf, true);
        SDL_snprintf(buf, sizeof(buf), "throttle %.2f", state.throttle);
        bar(180, state.throttle, buf, false);
        SDL_snprintf(buf, sizeof(buf), "brake %.2f", state.brake);
        bar(250, state.brake, buf, false);
        SDL_snprintf(buf, sizeof(buf), "clutch %.2f", state.clutch);
        bar(320, state.clutch, buf, false);
        SDL_snprintf(buf, sizeof(buf), "buttons 0x%07x  dpad %d  spring %s", state.buttons, state.dpad,
                     spring_on ? "on" : "off");
        SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
        SDL_RenderDebugText(renderer, 40, 400, buf);
    }
    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    lg4ff_close(wheel);
    lg4ff_quit();
}
