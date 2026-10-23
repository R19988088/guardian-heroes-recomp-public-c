#!/usr/bin/env python3
"""Guardian Heroes SDK edits (PC and Switch builds), run after switch/sdk-edits.py:

    python3 patches/sdk-edits-gh.py /root/gh/rexglue-sdk

InputSystem (include/rex/input/input_system.h, src/input/input_system.cpp):
 1. SetGameInputBlocked: while the in-game settings window (or an SDK ImGui popup) is open the guest
    sees a neutral pad and an empty keystroke queue; GetStateRaw reads the live pad for the window itself.
 2. One recursive mutex over the entry points: the settings window polls the pad on the UI thread while
    guest threads call GetState, and RefreshDevices mutates the device vectors (heap corruption otherwise).
Idempotent: the .cpp carries the marker `gh-edits` once the edit is in.
"""
import os, sys

SDK = sys.argv[1]
NL = chr(10)
MARK = '// gh-edits: input blocking and locking'


def rw(rel, fn):
    path = os.path.join(SDK, rel)
    s = open(path, encoding='utf-8').read()
    t = fn(s)
    if t != s:
        open(path, 'w', encoding='utf-8', newline=NL).write(t)
        print('changed:', rel)


def header(s):
    if 'GetStateRaw' in s:
        return s
    old = '  X_RESULT GetState(uint32_t user_index, X_INPUT_STATE* out_state);' + NL
    assert old in s, 'input_system.h anchor'
    return s.replace(old, old +
                     '  /// The live pad, also while the guest is blocked (the in-game settings window reads it).' + NL +
                     '  X_RESULT GetStateRaw(uint32_t user_index, X_INPUT_STATE* out_state);' + NL +
                     '  /// While true the guest sees a neutral pad and no keystrokes (gh: settings window open).' + NL +
                     '  static void SetGameInputBlocked(bool blocked);' + NL, 1)


def source(s):
    if MARK in s:
        return s
    # statics before the first function of the namespace
    anchor = 'namespace rex::input {' + NL
    assert anchor in s, 'input_system.cpp namespace'
    s = s.replace(anchor, anchor + MARK + NL +
                  'static std::atomic<bool> g_game_input_blocked{false};' + NL +
                  'static std::recursive_mutex g_input_mutex;' + NL +
                  'void InputSystem::SetGameInputBlocked(bool blocked) { g_game_input_blocked.store(blocked); }' + NL + NL, 1)
    if '#include <atomic>' not in s:
        s = s.replace('#include <rex/input/input_system.h>' + NL,
                      '#include <rex/input/input_system.h>' + NL + '#include <atomic>' + NL + '#include <mutex>' + NL, 1)
    lock = '  std::lock_guard<std::recursive_mutex> input_lock(g_input_mutex);' + NL

    def after_open_brace(src, sig):
        i = src.index(sig)
        j = src.index('{' + NL, i) + 2
        return src[:j], src[j:]

    # GetState -> GetStateRaw (the old body, now locked) + a wrapper that blanks the pad while blocked
    sig = 'X_RESULT InputSystem::GetState(uint32_t user_index, X_INPUT_STATE* out_state) {' + NL
    assert sig in s, 'GetState'
    s = s.replace(sig,
                  sig + '  X_RESULT result = GetStateRaw(user_index, out_state);' + NL +
                  '  if (result == X_ERROR_SUCCESS && g_game_input_blocked.load()) {' + NL +
                  '    out_state->gamepad = X_INPUT_GAMEPAD{};' + NL + '  }' + NL +
                  '  return result;' + NL + '}' + NL + NL +
                  'X_RESULT InputSystem::GetStateRaw(uint32_t user_index, X_INPUT_STATE* out_state) {' + NL + lock, 1)
    for name in ('X_RESULT InputSystem::GetCapabilities(', 'X_RESULT InputSystem::SetState('):
        head, tail = after_open_brace(s, name)
        s = head + lock + tail
    head, tail = after_open_brace(s, 'X_RESULT InputSystem::GetKeystroke(')
    s = head + '  if (g_game_input_blocked.load()) {' + NL + '    return X_ERROR_EMPTY;' + NL + '  }' + NL + lock + tail
    return s


rw('include/rex/input/input_system.h', header)
rw('src/input/input_system.cpp', source)


# Windows build with MSYS2 clang (mingw): no SEH __try/__except; the debugger thread-name trick is optional.
def threading_win(s):
    if 'gh-edits: no SEH' in s:
        return s
    old = '  __try {' + NL
    assert old in s, 'threading_win.cpp __try'
    i = s.index(old)
    j = s.index('  }' + NL, s.index('__except', i)) + 4
    block = s[i:j]
    return (s[:i] + '#ifdef _MSC_VER  // gh-edits: no SEH on mingw' + NL + block + '#else' + NL +
            '  (void)info;' + NL + '#endif' + NL + s[j:])


if os.path.exists(os.path.join(SDK, 'src/core/threading_win.cpp')):
    rw('src/core/threading_win.cpp', threading_win)


# mingw clang: FARPROC -> void* needs a cast (xinput driver keeps its entry points as void*).
def xinput_cast(s):
    import re
    return re.sub(r'^(  XInput\w+_ = )(xi\w+);', r'\1reinterpret_cast<void*>(\2);', s, flags=re.M)


if os.path.exists(os.path.join(SDK, 'src/input/xinput/xinput_input_driver.cpp')):
    rw('src/input/xinput/xinput_input_driver.cpp', xinput_cast)


# MSYS2 leaves CMAKE_SYSTEM_PROCESSOR empty: thirdparty/ then skips the x86 FFmpeg sources (link errors).
def win_processor(s):
    if 'gh-edits: win processor' in s:
        return s
    old = 'set(REX_TARGET_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")' + NL
    assert old in s, 'CMakeLists REX_TARGET_PROCESSOR'
    return s.replace(old, 'if(WIN32 AND NOT CMAKE_SYSTEM_PROCESSOR)  # gh-edits: win processor' + NL +
                     '    set(CMAKE_SYSTEM_PROCESSOR AMD64)' + NL + 'endif()' + NL + old, 1)


rw('CMakeLists.txt', win_processor)
