# PC features, design note (not started)

Two wishes, PC first, Switch if cheap. What exists today is the base for both.

## (a) In-game overlay menu: Esc on PC, Minus on Switch; controller page with a remap wizard
What exists: `src/gh_settings.{h,cpp}` is an ImGui window (`GhSettings`, an `ImGuiDialog` created in
`GhApp::OnCreateDialogs`). It opens with F1 or with Back/Select held 0.5 s, edits cvars (display, audio, `keybind_*`)
and saves them to `gh.toml`. While it is open, `rex::input::InputSystem::SetGameInputBlocked` (patches/sdk-edits-gh.py)
gives the game a neutral pad, and the pad drives ImGui navigation.

Plan:
- Open key: register `Escape` as a second key beside F1 (`rex::ui::RegisterBind`); keep Back-held for pads.
  Switch has no ImGui under deko3d: a Minus-opened overlay needs its own drawing (the bitmap-font strip of
  `deko3d_backend.cpp` is the start: a text list drawn with the present shaders, driven by the pad driver). Cheap only
  for a short list (volume, glyph set, overlay on/off); the full page stays PC.
- Controller page (PC): list connected pads from the SDK's `InputSystem` (device names come from SDL; see
  `rex/input/sdl`), show the name of the pad that drives player 1, a "Remap" wizard that asks for each of the 18 buttons in
  turn (A B X Y, shoulders, triggers, d-pad, sticks, Start, Back) by reading the raw SDL state, and stores one mapping
  per pad GUID in `gh_pads.toml` (key = GUID, value = button -> SDL button/axis). The SDK maps pads through SDL's
  gamecontroller DB (`hid_mappings_file`); the wizard's result is applied by feeding that DB with a generated
  `SDL_GameControllerAddMapping` string at start and after each wizard run. Keyboard keeps using the `keybind_*` cvars.
- Files: `gh_pads.toml` next to `gh.toml`; no game data inside.

## (b) Texture replacement from a mod folder, and a dump option for modders
What exists: the Switch backend already sees every texture in `UploadTexture` as staged, untiled, endian-swapped data
and has a dumper (`DumpTexture`, marker file `dk_dump_tex`) and a patch hook (`PatchGlyphs`). On PC the SDK's Vulkan and
D3D12 backends upload textures in `texture_cache` (`src/graphics/pipeline/texture/`), tiled data is untiled on the GPU, so
the hook point differs.

Plan:
- Identity: a stable id per texture = XXH3 of the guest data (the SDK has xxHash) plus format and size, e.g.
  `<w>x<h>_<format>_<hash16>`. Addresses are not stable across runs; the hash of the loaded data is.
- `dump_textures=1`: write each new texture once as `<dump_dir>/<id>.png` (decode DXT/4444/8888 to RGBA8 like
  `DecodeStaged` does; keep alpha). PC: do it on the CPU side before upload, in the texture cache's load path, from the
  untiled copy the SDK already makes for hashing; Switch: in `UploadTexture` (exists as PPM).
- Replacement: at upload time, if `<mod_dir>/<id>.png` exists, load it (stb_image is vendored in the SDK
  `thirdparty/stb`), convert to the texture's host format and use it instead of the game data. Same size is the safe case
  (formats and mip chains stay valid); a larger replacement needs the sampling coordinates scaled by nothing (UVs are
  normalized) so a larger PNG works as a resolution pack for 2D sprites; the texture cache must create the image with the new
  size. Cache decoded replacements in memory; look the folder up once at start (directory listing) to avoid per-upload
  file system calls (slow on the Switch SD).
- Hot reload on PC: F5 in the overlay rescans the folder.
- Order with the glyph patch: replacement first, glyph patch only when no replacement exists.
- Switch cost: one hash per upload (uploads are rare after loading) and one stdio read per replaced texture; fits.
