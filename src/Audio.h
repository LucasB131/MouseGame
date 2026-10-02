#pragma once

#include "raylib.h"

// Sound effects (short WAV files) and looping music (OGG files), all loaded from assets/audio/.
// Everything is a harmless no-op when the audio device could not be opened or a file is missing, so the game
// always runs, with or without sound.

enum class Sfx
{
    Step1, Step2, Cheese, GoldenCheese, Store, HoleEnter, HoleExit, HoleTeleport, Pepper, ExitOpen,
    Alert, WindUp, Pounce, Stun, BossHit, BossKo, Caught, Win, Intro,
    UiMove, UiSelect, UiBack, UiLocked, ShopBuy, ShopEquip, ShopFail,
    Count
};

enum class Track { None, Menu, House, Egypt, Boss };

namespace Audio
{
void Init();     // open the audio device and load every sound; call after InitWindow
void Shutdown(); // call before CloseWindow
void Update(float dt); // once per frame: feeds the music streams and handles fades
bool Ready();

// volume 0..1, pan -1 (left) .. 1 (right), pitch 1 = normal
void Play(Sfx sfx, float volume = 1.0f, float pan = 0.0f, float pitch = 1.0f);
// Quieter the farther the source is from the listener, and panned by its horizontal position.
void PlayAt(Sfx sfx, Vector2 source, Vector2 listener, float volume = 1.0f);

void PlayMusic(Track track); // crossfades from the current track; does nothing if it is already playing
void DuckMusic(float seconds); // quiet the music for a moment under the win / caught jingles (0 restores it)

// Master volume and mute apply to everything, are saved in settings.txt, and are shown in a small toast.
void HandleKeys(); // M: mute, - and =: volume. Call once per frame
void DrawOverlay(); // the volume toast (draw inside the logical view)
float MasterVolume();
bool Muted();
} // namespace Audio
