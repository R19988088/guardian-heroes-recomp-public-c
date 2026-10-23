# Button glyphs and menu confirm/cancel (Guardian Heroes, Switch)

## What is wanted
1. **Positional glyph swap** (done): the game draws Xbox icons; the Switch pad maps by position (Xbox A = bottom = Switch B),
   so the icon at the Xbox-A position must show the Switch letter B, and so on (A<->B, X<->Y).
2. **Switch-style icons** (not done): replace the coloured Xbox discs by flat grey/white round Switch buttons with a black
   letter, for every glyph the game draws: face buttons, LB/RB -> L/R, LT/RT -> ZL/ZR, Start -> +, Back -> -, stick and
   stick-press icons, D-pad. Option `button_glyphs = switch | xbox | ps` (default switch on Switch builds). Icons drawn in
   code (simple vector shapes), no artwork copied from elsewhere.
3. **Menu confirm/cancel** (not done): in menus Switch A (right) = confirm and Switch B (bottom) = cancel; gameplay unchanged.
   Option `menu_confirm = nintendo | xbox`. The glyphs shown in menu prompts must follow the active mapping.

## What is done (build 0.33+, Eden-verified)
- Cvar `button_glyphs = switch | xbox` (default switch), `src/gpu/deko3d_backend.cpp`, Switch only.
- Where the icons are: characters of the UI font atlas (`k_4_4_4_4`, 2048x2225, linear, physical 0x1D89E000). One row at
  y 49..74 holds the icons; cells of 24x26 px at x = 125 (A, green), 150 (B, red), 175 (X, blue), 200 (Y, orange). The same
  row also has LT, RT, LB, RB, START, BACK, the two stick icons and arrows to the right of Y (not touched yet).
  One more icon is baked into the title texture "PRESS A BUTTON" (`k_8_8_8_8` 288x96, disc at x 126..148, y 36..58).
- How: `UploadTexture` calls `PatchGlyphs` on the staged, untiled, endian-swapped data before the GPU copy.
  `SwapAtlasGlyphs` swaps the A/B and X/Y cells (guarded by `AtlasCellIs`: hue of the cell: A ~70, B ~0, X ~216, Y ~37).
  `ReplaceTitleGlyph` draws the atlas B icon over the title's A disc (filling the rest with the surrounding glow colour);
  when the title is uploaded before the atlas, the atlas rows are read from guest memory at the hint address.
- Found with the texture dumper: marker file `sdmc:/switch/gh/dk_dump_tex` writes every distinct texture as a PPM to
  `sdmc:/switch/gh/tex/` (decoded from the staged data: DXT1/3/5, 8888, 4444, k_8). Montage them to find glyph textures.
- Checked: title screenshot shows "PRESS (B) BUTTON"; a dump of the patched atlas shows B, A, Y, X.
- Not found yet: a screen that shows a coloured A/B/X/Y icon inside text (menus/lobby show START and BACK icons only).

## Plan for the rest
- **Switch-style icons:** render the tiles in code into the same 24x26 cells (and the title disc): a grey disc with a
  white rim and a black letter from a small bitmap font (the 5x7 font of the version overlay is a start; a larger smooth
  version is better), `+`/`-` as discs with a plus/minus, L/R and ZL/ZR as rounded shoulder shapes with the letters, stick
  and D-pad drawn as simple circles/crosses. Keep the cell positions so the game's own metrics stay valid. A `ps` set
  follows the same code with other shapes. Put the generator in `src/gpu/button_glyphs.{h,cpp}` shared by the backends.
- **menu_confirm:** trace which recompiled function reads the confirm button in menus (the pad state goes through
  `XamInputGetState`; look at the callers that test `X_INPUT_GAMEPAD_A` in the menu code, found by the texture/draw log
  of a menu screen plus a call trace), then wrap the menu handlers' input read; if no clean hook exists, fall back to a
  global A/B swap in `switch/host/input/nx_input_driver.cpp` and tell the user that gameplay buttons move too.
  Glyphs follow the mapping (swap the cell pairs only when the menu mapping is active).
- Verify each step in Eden with `switch/eden-deko3d.ps1 -Keys` (pad.txt) and before/after screenshots, then push with
  `switch/deploy.sh` (SHA-1) and report.
