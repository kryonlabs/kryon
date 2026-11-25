# Workout scene

A standalone animated VRM figure drawn by Kryon's GPU Scene backend. It has
no Breeze imports, no navigation and no text: only the 3D animation, so its
frame time is the figure's real cost.

On Android the scene renders straight into the activity window. No Pixmap
and no CPU copy take part in a frame. The renderer chooses its resolution
scale and MSAA count from the device's GPU: a Moto e6 play (PowerVR GE8320)
settles at 35% with one sample, stronger GPUs keep 100% with 4x MSAA.
The desktop profile has no GPU window presenter yet: it reads each frame
back and draws it as an image, so it shows the scene but its frame time is
not the Android renderer's.

```sh
make desktop                 # build the desktop profile
make android                 # debug APK (blocked for now, see below)
make install DEVICE=SERIAL   # adb install of that APK
adb -s SERIAL logcat -s WorkoutProfile
```

`src/workout.zi` is the application: a plain `main` that runs `App.Run`.
The Android build follows Kryon's application layout. `make android`
scaffolds the Gradle project under `droid/` and generates the two Android
host sources from `templates/droid`: `src/main.zi`, the entry that calls
`AndroidRunMain`, and `src/android_glue.zi`. Neither is tracked.

`make android` does not link at the moment, and the example does not hide
it. Since Kryon 09664f07 the Android source generator binds the host's
`AndroidProgramMain` to the entry module's plain `main`, while Ziran's host
bind (`BindHostProvider`) only accepts a function marked `#program_export`.
The build stops with `exported Ziran provider is missing:
..._android_run:AndroidProgramMain=workout:main`. By that code every C99
Android app stops there, Kryon's app template included (not built here to
confirm). Exporting `main` is not the fix; Kryon or Ziran has to change.
The Moto numbers come from the APK built on 2026-11-25 16:35 with the
earlier glue. Rebuild and measure again once `make android` links.

Every window of at least 60 frames prints one `WORKOUT: frames=...` line:
frames, mean interval, fps, p95 interval, pose, skin, render, prepare and
draw time, draw calls, swap and GPU time, then the chosen `scale` and
`msaa`. The first line also names the renderer, its GL version and how many
skins use a palette texture or a uniform array.

The default figure is the shared practice figure; set `VRM` to use another
VRM 0.x figure. It is copied to `assets/figure/man.vrm`, which is not
tracked.
