-- EEF-JP8000 VST3 host smoke via ReaScript API composition.
-- Launch with an isolated profile:
--   REAPER -newinst -nosplash -cfgfile <profile>/reaper.ini <this script>
-- Environment: EEF_RENDER_DIR, EEF_STATUS, EEF_SRATE, EEF_DURATION,
--              EEF_PLUGIN_NAME (optional).

local RENDER_DIR = os.getenv("EEF_RENDER_DIR")
local STATUS = os.getenv("EEF_STATUS") or "/tmp/eef-reaper-smoke-status.txt"
local DURATION = tonumber(os.getenv("EEF_DURATION")) or 3.0
local SRATE = tonumber(os.getenv("EEF_SRATE")) or 48000
local PLUGIN_NAME = os.getenv("EEF_PLUGIN_NAME") or "EEF-JP8000"

local function write_status(state, detail)
  local file = io.open(STATUS, "w")
  if file then
    file:write("status=" .. tostring(state) .. "\n")
    file:write("detail=" .. tostring(detail or "") .. "\n")
    file:close()
  end
end

write_status("boot", "")

if not RENDER_DIR or DURATION <= 0 or SRATE <= 0 then
  write_status("error", "set EEF_RENDER_DIR and positive EEF_DURATION/EEF_SRATE")
  return
end
if reaper.CountTracks(0) ~= 0 then
  write_status("error", "requires an empty isolated project")
  return
end

local function ensure_directory(path)
  -- The shell creates the directory before launch. This only makes a useful
  -- status if a caller supplied an invalid path instead of failing silently.
  local probe = io.open(path .. "/.eef_probe", "wb")
  if not probe then return false end
  probe:close()
  os.remove(path .. "/.eef_probe")
  return true
end

if not ensure_directory(RENDER_DIR) then
  write_status("error", "render directory is not writable")
  return
end

local track = reaper.GetTrack(0, 0)
if not track then
  reaper.InsertTrackAtIndex(0, false)
  track = reaper.GetTrack(0, 0)
end
if not track then
  write_status("error", "track creation failed")
  return
end

local fx = -1
local search_names = {
  PLUGIN_NAME,
  "EEF-JP8000 (EON Audio)",
  "VST3: " .. PLUGIN_NAME,
  "VST3: EEF-JP8000 (EON Audio)"
}
for _, search_name in ipairs(search_names) do
  fx = reaper.TrackFX_AddByName(track, search_name, false, -1)
  if fx >= 0 then break end
end
if fx < 0 then
  write_status("error", "VST3 not found by name; fx=" .. tostring(fx))
  return
end

local _, fx_name = reaper.TrackFX_GetFXName(track, fx, "")
local param_count = reaper.TrackFX_GetNumParams(track, fx)

-- A short chromatic chord exercises oscillator, envelope, filter and the
-- stereo ambience path without depending on an external source file.
local item = reaper.CreateNewMIDIItemInProj(track, 0.0, DURATION, false)
if not item then
  write_status("error", "MIDI item creation failed")
  return
end
local take = reaper.GetActiveTake(item)
if not take then
  write_status("error", "MIDI take creation failed")
  return
end

local notes = { 48, 55, 60, 64, 67 }
for index, note in ipairs(notes) do
  local start = 0.08 + (index - 1) * 0.18
  local finish = math.min(DURATION - 0.05, start + 0.42)
  reaper.MIDI_InsertNote(take, false, false,
                         reaper.MIDI_GetPPQPosFromProjTime(take, start),
                         reaper.MIDI_GetPPQPosFromProjTime(take, finish),
                         0, note, 96, true)
end
reaper.MIDI_Sort(take)

local output_path = RENDER_DIR .. "/eef-reaper-smoke.wav"
local old = io.open(output_path, "rb")
if old then
  old:close()
  os.remove(output_path)
end

reaper.GetSetProjectInfo_String(0, "RENDER_FILE", RENDER_DIR, true)
reaper.GetSetProjectInfo_String(0, "RENDER_PATTERN", "eef-reaper-smoke", true)
reaper.GetSetProjectInfo(0, "RENDER_BOUNDSFLAG", 0, true)
reaper.GetSetProjectInfo(0, "RENDER_STARTPOS", 0, true)
reaper.GetSetProjectInfo(0, "RENDER_ENDPOS", DURATION, true)
reaper.GetSetProjectInfo(0, "RENDER_SRATE", SRATE, true)
reaper.GetSetProjectInfo(0, "RENDER_CHANNELS", 2, true)
reaper.GetSetProjectInfo_String(0, "RENDER_FORMAT", "evaw", true)
reaper.GetSetProjectInfo(0, "RENDER_TAIL", 0, true)

write_status("fx-loaded", "name=" .. tostring(fx_name) ..
             " params=" .. tostring(param_count) ..
             " notes=" .. tostring(#notes))
reaper.Main_OnCommand(42230, 0) -- File: Render project, using recent settings

local started = reaper.time_precise()
local function poll_render()
  local file = io.open(output_path, "rb")
  if file then
    local header = file:read(12) or ""
    local bytes = file:seek("end") or 0
    file:close()
    local minimum = math.floor(DURATION * SRATE * 2 * 2 * 0.80)
    if header:sub(1, 4) == "RIFF" and header:sub(9, 12) == "WAVE" and bytes >= minimum then
      write_status("done", "bytes=" .. tostring(bytes) ..
                   " sample_rate=" .. tostring(SRATE) ..
                   " channels=2")
      reaper.Main_OnCommand(40004, 0) -- File: Quit
      return
    end
  end
  if reaper.time_precise() - started > 120 then
    write_status("render-timeout", "output=" .. output_path)
    reaper.Main_OnCommand(40004, 0)
    return
  end
  reaper.defer(poll_render)
end

reaper.defer(poll_render)
