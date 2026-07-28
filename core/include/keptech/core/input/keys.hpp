#pragma once

#include <SDL3/SDL.h>

namespace kt {
  enum class Keys : uint16_t {
    Unknown = SDL_SCANCODE_UNKNOWN,

    A = SDL_SCANCODE_A,
    B = SDL_SCANCODE_B,
    C = SDL_SCANCODE_C,
    D = SDL_SCANCODE_D,
    E = SDL_SCANCODE_E,
    F = SDL_SCANCODE_F,
    G = SDL_SCANCODE_G,
    H = SDL_SCANCODE_H,
    I = SDL_SCANCODE_I,
    J = SDL_SCANCODE_J,
    K = SDL_SCANCODE_K,
    L = SDL_SCANCODE_L,
    M = SDL_SCANCODE_M,
    N = SDL_SCANCODE_N,
    O = SDL_SCANCODE_O,
    P = SDL_SCANCODE_P,
    Q = SDL_SCANCODE_Q,
    R = SDL_SCANCODE_R,
    S = SDL_SCANCODE_S,
    T = SDL_SCANCODE_T,
    U = SDL_SCANCODE_U,
    V = SDL_SCANCODE_V,
    W = SDL_SCANCODE_W,
    X = SDL_SCANCODE_X,
    Y = SDL_SCANCODE_Y,
    Z = SDL_SCANCODE_Z,

    N1 = SDL_SCANCODE_1,
    N2 = SDL_SCANCODE_2,
    N3 = SDL_SCANCODE_3,
    N4 = SDL_SCANCODE_4,
    N5 = SDL_SCANCODE_5,
    N6 = SDL_SCANCODE_6,
    N7 = SDL_SCANCODE_7,
    N8 = SDL_SCANCODE_8,
    N9 = SDL_SCANCODE_9,
    N0 = SDL_SCANCODE_0,

    Return = SDL_SCANCODE_RETURN,
    Escape = SDL_SCANCODE_ESCAPE,
    Backspace = SDL_SCANCODE_BACKSPACE,
    Tab = SDL_SCANCODE_TAB,
    Space = SDL_SCANCODE_SPACE,

    Minus = SDL_SCANCODE_MINUS,
    Equals = SDL_SCANCODE_EQUALS,
    LBracket = SDL_SCANCODE_LEFTBRACKET,
    RBracket = SDL_SCANCODE_RIGHTBRACKET,

    /// Located at the lower left of the return key on ISO keyboards and at the
    /// right end of the QWERTY row on ANSI keyboards. Produces REVERSE SOLIDUS
    /// (backslash) and VERTICAL LINE in a US layout, REVERSE SOLIDUS and
    /// VERTICAL LINE in a UK Mac layout, NUMBER SIGN and TILDE in a UK Windows
    /// layout, DOLLAR SIGN and POUND SIGN in a Swiss German layout, NUMBER SIGN
    /// and APOSTROPHE in a German layout, GRAVE ACCENT and POUND SIGN in a
    /// French Mac layout, and ASTERISK and MICRO SIGN in a French Windows
    /// layout.
    Backslash = SDL_SCANCODE_BACKSLASH,
    /// ISO USB keyboards actually use this code instead of 49 for the same key,
    /// but all OSes I've seen treat the two codes identically. So, as an
    /// implementor, unless your keyboard generates both of those codes and your
    /// OS treats them differently, you should generate SDL_SCANCODE_BACKSLASH
    /// instead of this code. As a user, you should not rely on this code
    /// because SDL will never generate it with most (all?) keyboards.
    NonUsHash = SDL_SCANCODE_NONUSHASH,
    Semicolon = SDL_SCANCODE_SEMICOLON,
    Apostrophe = SDL_SCANCODE_APOSTROPHE,
    Grave = SDL_SCANCODE_GRAVE,
    Comma = SDL_SCANCODE_COMMA,
    Period = SDL_SCANCODE_PERIOD,
    Slash = SDL_SCANCODE_SLASH,

    Capslock = SDL_SCANCODE_CAPSLOCK,

    F1 = SDL_SCANCODE_F1,
    F2 = SDL_SCANCODE_F2,
    F3 = SDL_SCANCODE_F3,
    F4 = SDL_SCANCODE_F4,
    F5 = SDL_SCANCODE_F5,
    F6 = SDL_SCANCODE_F6,
    F7 = SDL_SCANCODE_F7,
    F8 = SDL_SCANCODE_F8,
    F9 = SDL_SCANCODE_F9,
    F10 = SDL_SCANCODE_F10,
    F11 = SDL_SCANCODE_F11,
    F12 = SDL_SCANCODE_F12,

    Printscreen = SDL_SCANCODE_PRINTSCREEN,
    Scrolllock = SDL_SCANCODE_SCROLLLOCK,
    Pause = SDL_SCANCODE_PAUSE,
    /// Occasionally Help on some Mac keyboards (but does send code 73)
    Insert = SDL_SCANCODE_INSERT,
    Home = SDL_SCANCODE_HOME,
    Pageup = SDL_SCANCODE_PAGEUP,
    Delete = SDL_SCANCODE_DELETE,
    End = SDL_SCANCODE_END,
    Pagedown = SDL_SCANCODE_PAGEDOWN,
    Right = SDL_SCANCODE_RIGHT,
    Left = SDL_SCANCODE_LEFT,
    Down = SDL_SCANCODE_DOWN,
    Up = SDL_SCANCODE_UP,
    /// Numlock on PC, clear on Mac keyboards
    NumlockClear = SDL_SCANCODE_NUMLOCKCLEAR,
    NumpadDivide = SDL_SCANCODE_KP_DIVIDE,
    NumpadMultiply = SDL_SCANCODE_KP_MULTIPLY,
    NumpadMinus = SDL_SCANCODE_KP_MINUS,
    NumpadPlus = SDL_SCANCODE_KP_PLUS,
    NumpadEnter = SDL_SCANCODE_KP_ENTER,
    Numpad1 = SDL_SCANCODE_KP_1,
    Numpad2 = SDL_SCANCODE_KP_2,
    Numpad3 = SDL_SCANCODE_KP_3,
    Numpad4 = SDL_SCANCODE_KP_4,
    Numpad5 = SDL_SCANCODE_KP_5,
    Numpad6 = SDL_SCANCODE_KP_6,
    Numpad7 = SDL_SCANCODE_KP_7,
    Numpad8 = SDL_SCANCODE_KP_8,
    Numpad9 = SDL_SCANCODE_KP_9,
    Numpad0 = SDL_SCANCODE_KP_0,
    NumpadPeriod = SDL_SCANCODE_KP_PERIOD,

    /// This is the additional key that ISO keyboards have over ANSI ones
    /// located between left shift and Z. Produces GRAVE ACCENT and TILDE in a
    /// US or UK Mac layout, REVERSE SOLIDUS (backslash) and VERTICAL LINE in a
    /// US or UK Windows layout, and LESS-THAN SIGN and GREATER-THAN SIGN in a
    /// Swiss German, German, or French layout.
    NonUsBackslash = SDL_SCANCODE_NONUSBACKSLASH,
    NumpadEquals = SDL_SCANCODE_KP_EQUALS,
    F13 = SDL_SCANCODE_F13,
    F14 = SDL_SCANCODE_F14,
    F15 = SDL_SCANCODE_F15,
    F16 = SDL_SCANCODE_F16,
    F17 = SDL_SCANCODE_F17,
    F18 = SDL_SCANCODE_F18,
    F19 = SDL_SCANCODE_F19,
    F20 = SDL_SCANCODE_F20,
    F21 = SDL_SCANCODE_F21,
    F22 = SDL_SCANCODE_F22,
    F23 = SDL_SCANCODE_F23,
    F24 = SDL_SCANCODE_F24,
    Mute = SDL_SCANCODE_MUTE,
    VolumeUp = SDL_SCANCODE_VOLUMEUP,
    VolumeDown = SDL_SCANCODE_VOLUMEDOWN,
    NumpadComma = SDL_SCANCODE_KP_COMMA,

    LCtrl = SDL_SCANCODE_LCTRL,
    LShift = SDL_SCANCODE_LSHIFT,
    LAlt = SDL_SCANCODE_LALT,
    LGui = SDL_SCANCODE_LGUI,
    RCtrl = SDL_SCANCODE_RCTRL,
    RShift = SDL_SCANCODE_RSHIFT,
    RAlt = SDL_SCANCODE_RALT,
    RGui = SDL_SCANCODE_RGUI,

    MediaPlay = SDL_SCANCODE_MEDIA_PLAY,
    MediaPause = SDL_SCANCODE_MEDIA_PAUSE,
    MediaNext = SDL_SCANCODE_MEDIA_NEXT_TRACK,
    MediaPrev = SDL_SCANCODE_MEDIA_PREVIOUS_TRACK,
    MediaStop = SDL_SCANCODE_MEDIA_STOP,
    MediaPlayPause = SDL_SCANCODE_MEDIA_PLAY_PAUSE,
  };
}
