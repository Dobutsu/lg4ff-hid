# lg4ff-hid

Logitech **G29** and **G923 for PlayStation/PC** racing wheels with force feedback, for games built on SDL2 or SDL3. It works without root on Android, including standalone **Meta Quest 3**.

The library is two C files (`lg4ff_hid.c`, `lg4ff_hid.h`). It talks to the wheel through SDL's public HIDAPI (`SDL_hid_*`), so you don't need a patched SDL. On Android, SDL's own Java `HIDDeviceManager` does the USB work and shows the system permission dialog.

What you get:

- The G923 for PlayStation powers up as `046d:c267`. The library switches it to native mode (`046d:c266`), the same as SDL3's lg4ff driver does on desktop.
- Steering is a `-1..1` float over the configured rotation. Throttle, brake and clutch are `0..1`. You also get the 25 buttons and the D-pad.
- Force feedback: rotation range (40–900°), the wheel's centering spring, a constant force, and rumble emulation for games that only know gamepad rumble.

Tested on a G923 for PlayStation/PC on Meta Quest 3 (Horizon OS, Android 14). The G29 shares the protocol and report layout, but nobody has tested it with this library yet. The Xbox G923 (`046d:c26e`, `c26d`) uses a different protocol and isn't supported.

## Use

```c
#include "lg4ff_hid.h"          /* define LG4FF_SDL3 when building against SDL3 */

lg4ff_init();
LG4FF_Wheel *wheel = lg4ff_open();          /* NULL: no wheel yet (see below) */
lg4ff_set_range(wheel, 900);
lg4ff_set_autocenter(wheel, 0.3f);

/* every frame */
LG4FF_State s;
if (lg4ff_update(wheel, &s) < 0) { lg4ff_close(wheel); wheel = NULL; }
car_steer(s.steering);
car_pedals(s.throttle, s.brake);
if (s.buttons & LG4FF_BUTTON_PADDLE_RIGHT) shift_up();
lg4ff_set_constant_force(wheel, -0.2f);      /* pull left, -1..1 */
```

Things to know:

- **`lg4ff_open()` blocks** on Android until the user answers the USB permission dialog, so call it from a worker thread. `examples/wheeldemo` shows one way.
- **Call it again later.** After the G923 mode switch the wheel re-enumerates, so a NULL return can mean "try again in a second". If the user denies permission, that wheel is skipped until it is reconnected, so polling never re-prompts.
- **One thread per wheel.** Make every call for a given wheel from one thread.
- **Force direction.** Positive force pulls the rim to the right.

Add `lg4ff_hid.c` to your build and put this directory on the include path.

## Android and Meta Quest

- **Activity:** use SDL's `SDLActivity` (or set up `HIDDeviceManager` the same way). Add `<uses-feature android:name="android.hardware.usb.host" android:required="false"/>`.
- **SDL3 focus loss:** SDL3 releases USB devices every time the window loses focus, and Quest panels lose focus constantly. Set `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` to `"1"`, otherwise writes fail and the wheel stops reading. SDL2 only releases devices on pause.
- **Interface 0:** the G923 exposes three HID interfaces. Only interface 0 has an output endpoint, and the library picks it.
- **`SDL_hid_set_nonblocking()`** always fails on Android; the library ignores that.
- **USB_DEVICE_ATTACHED filter:** don't add one for Logitech to your manifest unless you want the "open with" chooser every time the wheel is plugged in.
- **One USB-C port:** the Quest 3 has only one. While the wheel uses it, debug over `adb tcpip` / Wi-Fi.

## Desktop

The code builds for Linux, macOS and Windows, but it has only been run on Android so far.

- **SDL3's own driver:** on desktop, SDL3's HIDAPI lg4ff driver can claim the same wheel. Set `SDL_HINT_JOYSTICK_HIDAPI_LG4FF` to `"0"` when you use this library.
- **Linux:** the library needs read/write access to the wheel's hidraw node (a udev rule). The kernel's Logitech driver may already offer the wheel with force feedback through evdev.
- **Windows:** `hid.dll` won't send the wheel's 7-byte output reports, so force feedback writes fail there (SDL3 notes the same). Input works.

## Example

`examples/wheeldemo` is a small SDL3 app built against an **unmodified** SDL3. It shows steering, pedals and buttons, and runs the force feedback from the wheel's own buttons. To build the Android APK without Gradle:

```sh
SDL3=/path/to/SDL examples/wheeldemo/android/build.sh
adb install examples/wheeldemo/android/wheeldemo.apk
```

## SDL patches

`patches/` holds the alternative this library replaced: drivers inside SDL that make the wheel look like an ordinary joystick or game controller.

- `sdl3-lg4ff-g923-android.diff` (SDL 3.4.x) extends SDL3's lg4ff driver with the G923 for PlayStation and enables it on Android.
- `sdl2-lg4ff-hidapi-driver.diff` (SDL 2.32.x) ports a reduced lg4ff driver to SDL2. It presents the wheel as a game controller and turns rumble into wheel vibration.

These patches are **not** submissions to SDL, and SDL does not accept them. Prefer the library unless your game can only read SDL joysticks.

## Credits and license

zlib, see `LICENSE.txt`. The identification, mode switch, range and autocenter commands come from SDL3's `SDL_hidapi_lg4ff.c` by Simon Wood, Michal Malý and Katharine Chui. They in turn ported them from the Linux kernel's `hid-lg4ff` driver by Michal Malý and Simon Wood. The G923 for PlayStation support (mode switch as output report `0x30`, interface selection) and the Quest notes were worked out and verified on hardware for this library.
