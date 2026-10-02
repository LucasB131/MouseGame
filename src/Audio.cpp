#include "Audio.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

#include "Viewport.h"

namespace
{
constexpr int SfxCount = static_cast<int>(Sfx::Count);
constexpr int TrackCount = 4; // Menu, House, Egypt, Boss
constexpr int AliasCount = 3; // extra copies so the same sound can overlap itself

const char* const kSfxFiles[SfxCount] = {
    "step1", "step2", "cheese", "golden_cheese", "store", "hole_enter", "hole_exit", "hole_teleport", "pepper", "exit_open",
    "alert", "windup", "pounce", "stun", "boss_hit", "boss_ko", "caught", "win", "intro",
    "ui_move", "ui_select", "ui_back", "ui_locked", "shop_buy", "shop_equip", "shop_fail"};

const char* const kTrackFiles[TrackCount] = {"music_menu", "music_house", "music_egypt", "music_boss"};
// Relative loudness of each track (the mastering already evens them out, this is the final trim).
const float kTrackGain[TrackCount] = {0.55f, 0.5f, 0.55f, 0.5f};

struct Voice
{
    Sound base{};
    Sound alias[AliasCount]{};
    int next = 0;
    bool loaded = false;
};

struct Stream
{
    Music music{};
    bool loaded = false;
    float fade = 0.0f; // 0..1, how much of the track's volume is audible
};

bool g_ready = false;
Voice g_voices[SfxCount];
Stream g_streams[TrackCount];
int g_current = -1; // index into g_streams of the track that should be audible, -1 for none
float g_duck = 1.0f;
float g_duckTimer = 0.0f;

float g_master = 0.8f;
bool g_muted = false;
float g_toastTimer = 0.0f;
std::string g_settingsPath;

void ApplyMasterVolume() { SetMasterVolume(g_muted ? 0.0f : g_master); }

void SaveSettings()
{
    if (g_settingsPath.empty()) return;
    std::ofstream out(g_settingsPath);
    out << "volume " << g_master << "\nmuted " << (g_muted ? 1 : 0) << "\n";
}

void LoadSettings()
{
    std::ifstream in(g_settingsPath);
    std::string key;
    float value = 0.0f;
    while (in >> key >> value)
    {
        if (key == "volume") g_master = std::clamp(value, 0.0f, 1.0f);
        else if (key == "muted") g_muted = value != 0.0f;
    }
}
} // namespace

namespace Audio
{
void Init()
{
    InitAudioDevice();
    if (!IsAudioDeviceReady())
    {
        TraceLog(LOG_WARNING, "Audio: no audio device, running without sound");
        return;
    }
    const std::string dir = std::string(GetApplicationDirectory()) + "assets/audio/";
    g_settingsPath = std::string(GetApplicationDirectory()) + "settings.txt";
    LoadSettings();
    ApplyMasterVolume();

    for (int i = 0; i < SfxCount; ++i)
    {
        const std::string path = dir + kSfxFiles[i] + ".wav";
        if (!FileExists(path.c_str())) continue;
        Voice& v = g_voices[i];
        v.base = LoadSound(path.c_str());
        v.loaded = IsSoundValid(v.base);
        if (!v.loaded) continue;
        for (Sound& a : v.alias) a = LoadSoundAlias(v.base);
    }
    for (int i = 0; i < TrackCount; ++i)
    {
        const std::string path = dir + kTrackFiles[i] + ".ogg";
        if (!FileExists(path.c_str())) continue;
        Stream& s = g_streams[i];
        s.music = LoadMusicStream(path.c_str());
        s.loaded = IsMusicValid(s.music);
        if (s.loaded) s.music.looping = true;
    }
    g_ready = true;
}

void Shutdown()
{
    if (!g_ready) return;
    for (Stream& s : g_streams)
        if (s.loaded) UnloadMusicStream(s.music);
    for (Voice& v : g_voices)
    {
        if (!v.loaded) continue;
        for (Sound& a : v.alias) UnloadSoundAlias(a);
        UnloadSound(v.base);
    }
    CloseAudioDevice();
    g_ready = false;
}

bool Ready() { return g_ready; }

void Update(float dt)
{
    g_toastTimer = std::max(0.0f, g_toastTimer - dt);
    if (!g_ready) return;

    // Duck eases toward its target so the win and caught jingles are never fought by the music.
    g_duckTimer = std::max(0.0f, g_duckTimer - dt);
    const float duckTarget = g_duckTimer > 0.0f ? 0.18f : 1.0f;
    g_duck += (duckTarget - g_duck) * std::min(1.0f, dt * 4.0f);

    for (int i = 0; i < TrackCount; ++i)
    {
        Stream& s = g_streams[i];
        if (!s.loaded) continue;
        const float target = i == g_current ? 1.0f : 0.0f;
        const float step = dt / 1.2f; // 1.2 second crossfade
        s.fade = s.fade < target ? std::min(target, s.fade + step) : std::max(target, s.fade - step);

        if (s.fade > 0.0f)
        {
            if (!IsMusicStreamPlaying(s.music)) PlayMusicStream(s.music);
            SetMusicVolume(s.music, s.fade * kTrackGain[i] * g_duck);
            UpdateMusicStream(s.music);
        }
        else if (IsMusicStreamPlaying(s.music))
        {
            StopMusicStream(s.music);
        }
    }
}

void Play(Sfx sfx, float volume, float pan, float pitch)
{
    if (!g_ready || volume <= 0.001f) return;
    Voice& v = g_voices[static_cast<int>(sfx)];
    if (!v.loaded) return;
    // Rotate through the original and its aliases so quick repeats overlap instead of cutting each other off.
    const int slot = v.next;
    v.next = (v.next + 1) % (AliasCount + 1);
    Sound& s = slot == 0 ? v.base : v.alias[slot - 1];
    SetSoundVolume(s, std::clamp(volume, 0.0f, 1.0f));
    SetSoundPan(s, 0.5f + 0.5f * std::clamp(pan, -1.0f, 1.0f)); // raylib: 0 = left, 0.5 = center, 1 = right
    SetSoundPitch(s, pitch);
    PlaySound(s);
}

void PlayAt(Sfx sfx, Vector2 source, Vector2 listener, float volume)
{
    const float dx = source.x - listener.x;
    const float dy = source.y - listener.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    const float near = 1.0f - std::min(1.0f, dist / 900.0f);
    Play(sfx, volume * (0.2f + 0.8f * near * near), std::clamp(dx / 640.0f, -1.0f, 1.0f) * 0.7f);
}

void PlayMusic(Track track)
{
    const int index = static_cast<int>(track) - 1; // Track::None is 0
    if (index == g_current) return;
    g_current = index >= 0 && index < TrackCount && g_streams[index].loaded ? index : -1;
}

void DuckMusic(float seconds) { g_duckTimer = std::max(0.0f, seconds); }

void HandleKeys()
{
    bool changed = false;
    if (IsKeyPressed(KEY_M))
    {
        g_muted = !g_muted;
        changed = true;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
    {
        g_master = std::max(0.0f, std::round((g_master - 0.1f) * 10.0f) / 10.0f);
        g_muted = false;
        changed = true;
    }
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
    {
        g_master = std::min(1.0f, std::round((g_master + 0.1f) * 10.0f) / 10.0f);
        g_muted = false;
        changed = true;
    }
    if (!changed) return;
    g_toastTimer = 1.4f;
    if (!g_ready) return;
    ApplyMasterVolume();
    SaveSettings();
    Play(Sfx::UiMove);
}

void DrawOverlay()
{
    if (g_toastTimer <= 0.0f) return;
    const char* text = g_muted ? "Sound off  (M)" : TextFormat("Volume %d%%  (- / =)", static_cast<int>(std::round(g_master * 100.0f)));
    const int size = 22;
    const int w = MeasureText(text, size) + 36;
    const float a = std::min(1.0f, g_toastTimer * 2.0f);
    const Rectangle box{LogicalWidth / 2.0f - w / 2.0f, 12.0f, static_cast<float>(w), 38.0f};
    DrawRectangleRounded(box, 0.4f, 8, Fade(BLACK, 0.75f * a));
    DrawText(text, static_cast<int>(box.x + 18), static_cast<int>(box.y + 8), size, Fade(g_muted ? Color{240, 140, 120, 255} : RAYWHITE, a));
}

float MasterVolume() { return g_master; }
bool Muted() { return g_muted; }
} // namespace Audio
