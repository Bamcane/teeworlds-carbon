/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_KEYS_H
#define ENGINE_KEYS_H
#if defined(CONF_FAMILY_WINDOWS)
#undef KEY_EXECUTE
#endif

enum
{
	KEY_FIRST = 0,
	KEY_UNKNOWN = 0,
	KEY_BACKSPACE = 8,
	KEY_TAB = 9,
	KEY_RETURN = 13,
	KEY_ESCAPE = 27,
	KEY_SPACE = 32,
	KEY_EXCLAIM = 33,
	KEY_QUOTEDBL = 34,
	KEY_HASH = 35,
	KEY_DOLLAR = 36,
	KEY_PERCENT = 37,
	KEY_AMPERSAND = 38,
	KEY_QUOTE = 39,
	KEY_LEFTPAREN = 40,
	KEY_RIGHTPAREN = 41,
	KEY_ASTERISK = 42,
	KEY_PLUS = 43,
	KEY_COMMA = 44,
	KEY_MINUS = 45,
	KEY_PERIOD = 46,
	KEY_SLASH = 47,
	KEY_0 = 48,
	KEY_1 = 49,
	KEY_2 = 50,
	KEY_3 = 51,
	KEY_4 = 52,
	KEY_5 = 53,
	KEY_6 = 54,
	KEY_7 = 55,
	KEY_8 = 56,
	KEY_9 = 57,
	KEY_COLON = 58,
	KEY_SEMICOLON = 59,
	KEY_LESS = 60,
	KEY_EQUALS = 61,
	KEY_GREATER = 62,
	KEY_QUESTION = 63,
	KEY_AT = 64,
	KEY_LEFTBRACKET = 91,
	KEY_BACKSLASH = 92,
	KEY_RIGHTBRACKET = 93,
	KEY_CARET = 94,
	KEY_UNDERSCORE = 95,
	KEY_BACKQUOTE = 96,
	KEY_A = 97,
	KEY_B = 98,
	KEY_C = 99,
	KEY_D = 100,
	KEY_E = 101,
	KEY_F = 102,
	KEY_G = 103,
	KEY_H = 104,
	KEY_I = 105,
	KEY_J = 106,
	KEY_K = 107,
	KEY_L = 108,
	KEY_M = 109,
	KEY_N = 110,
	KEY_O = 111,
	KEY_P = 112,
	KEY_Q = 113,
	KEY_R = 114,
	KEY_S = 115,
	KEY_T = 116,
	KEY_U = 117,
	KEY_V = 118,
	KEY_W = 119,
	KEY_X = 120,
	KEY_Y = 121,
	KEY_Z = 122,
	KEY_DELETE = 127,
	KEY_CAPSLOCK = 185,
	KEY_F1 = 186,
	KEY_F2 = 187,
	KEY_F3 = 188,
	KEY_F4 = 189,
	KEY_F5 = 190,
	KEY_F6 = 191,
	KEY_F7 = 192,
	KEY_F8 = 193,
	KEY_F9 = 194,
	KEY_F10 = 195,
	KEY_F11 = 196,
	KEY_F12 = 197,
	KEY_PRINTSCREEN = 198,
	KEY_SCROLLLOCK = 199,
	KEY_PAUSE = 200,
	KEY_INSERT = 201,
	KEY_HOME = 202,
	KEY_PAGEUP = 203,
	KEY_END = 205,
	KEY_PAGEDOWN = 206,
	KEY_RIGHT = 207,
	KEY_LEFT = 208,
	KEY_DOWN = 209,
	KEY_UP = 210,
	KEY_NUMLOCKCLEAR = 211,
	KEY_KP_DIVIDE = 212,
	KEY_KP_MULTIPLY = 213,
	KEY_KP_MINUS = 214,
	KEY_KP_PLUS = 215,
	KEY_KP_ENTER = 216,
	KEY_KP_1 = 217,
	KEY_KP_2 = 218,
	KEY_KP_3 = 219,
	KEY_KP_4 = 220,
	KEY_KP_5 = 221,
	KEY_KP_6 = 222,
	KEY_KP_7 = 223,
	KEY_KP_8 = 224,
	KEY_KP_9 = 225,
	KEY_KP_0 = 226,
	KEY_KP_PERIOD = 227,
	KEY_APPLICATION = 229,
	KEY_POWER = 230,
	KEY_KP_EQUALS = 231,
	KEY_F13 = 232,
	KEY_F14 = 233,
	KEY_F15 = 234,
	KEY_F16 = 235,
	KEY_F17 = 236,
	KEY_F18 = 237,
	KEY_F19 = 238,
	KEY_F20 = 239,
	KEY_F21 = 240,
	KEY_F22 = 241,
	KEY_F23 = 242,
	KEY_F24 = 243,
	KEY_EXECUTE = 244,
	KEY_HELP = 245,
	KEY_MENU = 246,
	KEY_SELECT = 247,
	KEY_STOP = 248,
	KEY_AGAIN = 249,
	KEY_UNDO = 250,
	KEY_CUT = 251,
	KEY_COPY = 252,
	KEY_PASTE = 253,
	KEY_FIND = 254,
	KEY_MUTE = 255,
	KEY_VOLUMEUP = 256,
	KEY_VOLUMEDOWN = 257,
	KEY_KP_COMMA = 261,
	KEY_KP_EQUALSAS400 = 262,
	KEY_ALTERASE = 281,
	KEY_SYSREQ = 282,
	KEY_CANCEL = 283,
	KEY_CLEAR = 284,
	KEY_PRIOR = 285,
	KEY_RETURN2 = 286,
	KEY_SEPARATOR = 287,
	KEY_OUT = 288,
	KEY_OPER = 289,
	KEY_CLEARAGAIN = 290,
	KEY_CRSEL = 291,
	KEY_EXSEL = 292,
	KEY_KP_00 = 304,
	KEY_KP_000 = 305,
	KEY_THOUSANDSSEPARATOR = 306,
	KEY_DECIMALSEPARATOR = 307,
	KEY_CURRENCYUNIT = 308,
	KEY_CURRENCYSUBUNIT = 309,
	KEY_KP_LEFTPAREN = 310,
	KEY_KP_RIGHTPAREN = 311,
	KEY_KP_LEFTBRACE = 312,
	KEY_KP_RIGHTBRACE = 313,
	KEY_KP_TAB = 314,
	KEY_KP_BACKSPACE = 315,
	KEY_KP_A = 316,
	KEY_KP_B = 317,
	KEY_KP_C = 318,
	KEY_KP_D = 319,
	KEY_KP_E = 320,
	KEY_KP_F = 321,
	KEY_KP_XOR = 322,
	KEY_KP_POWER = 323,
	KEY_KP_PERCENT = 324,
	KEY_KP_LESS = 325,
	KEY_KP_GREATER = 326,
	KEY_KP_AMPERSAND = 327,
	KEY_KP_DBLAMPERSAND = 328,
	KEY_KP_VERTICALBAR = 329,
	KEY_KP_DBLVERTICALBAR = 330,
	KEY_KP_COLON = 331,
	KEY_KP_HASH = 332,
	KEY_KP_SPACE = 333,
	KEY_KP_AT = 334,
	KEY_KP_EXCLAM = 335,
	KEY_KP_MEMSTORE = 336,
	KEY_KP_MEMRECALL = 337,
	KEY_KP_MEMCLEAR = 338,
	KEY_KP_MEMADD = 339,
	KEY_KP_MEMSUBTRACT = 340,
	KEY_KP_MEMMULTIPLY = 341,
	KEY_KP_MEMDIVIDE = 342,
	KEY_KP_PLUSMINUS = 343,
	KEY_KP_CLEAR = 344,
	KEY_KP_CLEARENTRY = 345,
	KEY_KP_BINARY = 346,
	KEY_KP_OCTAL = 347,
	KEY_KP_DECIMAL = 348,
	KEY_KP_HEXADECIMAL = 349,
	KEY_LCTRL = 352,
	KEY_LSHIFT = 353,
	KEY_LALT = 354,
	KEY_LGUI = 355,
	KEY_RCTRL = 356,
	KEY_RSHIFT = 357,
	KEY_RALT = 358,
	KEY_RGUI = 359,
	KEY_MODE = 385,
	KEY_AUDIONEXT = 386,
	KEY_AUDIOPREV = 387,
	KEY_AUDIOSTOP = 388,
	KEY_AUDIOPLAY = 389,
	KEY_AUDIOMUTE = 390,
	KEY_MEDIASELECT = 391,
	KEY_WWW = 392,
	KEY_MAIL = 393,
	KEY_CALCULATOR = 394,
	KEY_COMPUTER = 395,
	KEY_AC_SEARCH = 396,
	KEY_AC_HOME = 397,
	KEY_AC_BACK = 398,
	KEY_AC_FORWARD = 399,
	KEY_AC_STOP = 400,
	KEY_AC_REFRESH = 401,
	KEY_AC_BOOKMARKS = 402,
	KEY_BRIGHTNESSDOWN = 403,
	KEY_BRIGHTNESSUP = 404,
	KEY_DISPLAYSWITCH = 405,
	KEY_KBDILLUMTOGGLE = 406,
	KEY_KBDILLUMDOWN = 407,
	KEY_KBDILLUMUP = 408,
	KEY_EJECT = 409,
	KEY_SLEEP = 410,

	KEY_MOUSE_1 = 411,
	KEY_MOUSE_2 = 412,
	KEY_MOUSE_3 = 413,
	KEY_MOUSE_4 = 414,
	KEY_MOUSE_5 = 415,
	KEY_MOUSE_6 = 416,
	KEY_MOUSE_7 = 417,
	KEY_MOUSE_8 = 418,
	KEY_MOUSE_9 = 419,
	KEY_MOUSE_WHEEL_UP = 420,
	KEY_MOUSE_WHEEL_DOWN = 421,

	KEY_JOYSTICK_BUTTON_0 = 422,
	KEY_JOYSTICK_BUTTON_1,
	KEY_JOYSTICK_BUTTON_2,
	KEY_JOYSTICK_BUTTON_3,
	KEY_JOYSTICK_BUTTON_4,
	KEY_JOYSTICK_BUTTON_5,
	KEY_JOYSTICK_BUTTON_6,
	KEY_JOYSTICK_BUTTON_7,
	KEY_JOYSTICK_BUTTON_8,
	KEY_JOYSTICK_BUTTON_9,
	KEY_JOYSTICK_BUTTON_10,
	KEY_JOYSTICK_BUTTON_11,

	KEY_JOY_HAT0_LEFTUP,
	KEY_JOY_HAT0_UP,
	KEY_JOY_HAT0_RIGHTUP,
	KEY_JOY_HAT0_LEFT,
	KEY_JOY_HAT0_RIGHT,
	KEY_JOY_HAT0_LEFTDOWN,
	KEY_JOY_HAT0_DOWN,
	KEY_JOY_HAT0_RIGHTDOWN,
	KEY_JOY_HAT1_LEFTUP,
	KEY_JOY_HAT1_UP,
	KEY_JOY_HAT1_RIGHTUP,
	KEY_JOY_HAT1_LEFT,
	KEY_JOY_HAT1_RIGHT,
	KEY_JOY_HAT1_LEFTDOWN,
	KEY_JOY_HAT1_DOWN,
	KEY_JOY_HAT1_RIGHTDOWN,

	KEY_JOY_AXIS_0_LEFT,
	KEY_JOY_AXIS_0_RIGHT,
	KEY_JOY_AXIS_1_LEFT,
	KEY_JOY_AXIS_1_RIGHT,
	KEY_JOY_AXIS_2_LEFT,
	KEY_JOY_AXIS_2_RIGHT,
	KEY_JOY_AXIS_3_LEFT,
	KEY_JOY_AXIS_3_RIGHT,
	KEY_JOY_AXIS_4_LEFT,
	KEY_JOY_AXIS_4_RIGHT,
	KEY_JOY_AXIS_5_LEFT,
	KEY_JOY_AXIS_5_RIGHT,
	KEY_JOY_AXIS_6_LEFT,
	KEY_JOY_AXIS_6_RIGHT,
	KEY_JOY_AXIS_7_LEFT,
	KEY_JOY_AXIS_7_RIGHT,
	KEY_JOY_AXIS_8_LEFT,
	KEY_JOY_AXIS_8_RIGHT,
	KEY_JOY_AXIS_9_LEFT,
	KEY_JOY_AXIS_9_RIGHT,
	KEY_JOY_AXIS_10_LEFT,
	KEY_JOY_AXIS_10_RIGHT,
	KEY_JOY_AXIS_11_LEFT,
	KEY_JOY_AXIS_11_RIGHT,

	KEY_LAST,

	NUM_JOYSTICK_BUTTONS = KEY_JOYSTICK_BUTTON_11 - KEY_JOYSTICK_BUTTON_0 + 1,
	NUM_JOYSTICK_AXES_BUTTONS = KEY_JOY_AXIS_11_RIGHT - KEY_JOY_AXIS_0_LEFT + 1,
	NUM_JOYSTICK_BUTTONS_PER_AXIS = KEY_JOY_AXIS_0_RIGHT - KEY_JOY_AXIS_0_LEFT + 1,
	NUM_JOYSTICK_AXES = NUM_JOYSTICK_AXES_BUTTONS / NUM_JOYSTICK_BUTTONS_PER_AXIS,
	NUM_JOYSTICK_HAT_BUTTONS = KEY_JOY_HAT1_RIGHTDOWN - KEY_JOY_HAT0_LEFTUP + 1,
	NUM_JOYSTICK_BUTTONS_PER_HAT = KEY_JOY_HAT1_RIGHTDOWN - KEY_JOY_HAT1_LEFTUP + 1,
	NUM_JOYSTICK_HATS = NUM_JOYSTICK_HAT_BUTTONS / NUM_JOYSTICK_BUTTONS_PER_HAT,
};

inline int KeyToKeycode(int Key) { return (Key >= 0x80) ? (Key - 0x80) | (1 << 30) : Key; }
inline int KeycodeToKey(int Keycode) { return (Keycode & (1 << 30)) ? Keycode - (1 << 30) + 0x80 : Keycode; }

inline int DigitToNumberKey(int Digit)
{
	if(Digit < 0 || Digit > 9)
		return KEY_UNKNOWN;
	return KEY_0 + Digit;
}

inline int DigitToKeypadKey(int Digit)
{
	if(Digit < 0 || Digit > 9)
		return KEY_UNKNOWN;
	if(Digit == 0)
		return KEY_KP_0;
	return KEY_KP_1 + Digit - 1;
}

#endif
