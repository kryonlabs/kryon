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
make android                 # build the debug APK
make install DEVICE=SERIAL   # adb install of that APK
adb -s SERIAL logcat -s WorkoutProfile
```

`src/workout.tsx` composes the desktop `Image` and runs `App.Run`. Native
loading, pose updates, GPU rendering and measurements live in
`src/workout_model.zi`.
The Android build follows Kryon's application layout. `make android`
scaffolds the Gradle project under `droid/` and generates the two Android
host sources from `templates/droid`: `src/main.zi`, the entry that calls
`AndroidRunMain`, and `src/android_glue.zi`. Neither is tracked.

The generator also supplies the native `AndroidProgramMain` bridge used
by the activity host. The application's TSX `main` stays an ordinary native
entry without a `#program_export` annotation.

Every window of at least 60 frames prints one `WORKOUT: frames=...` line:
frames, mean interval, fps, p95 interval, pose, skin, render, prepare and
draw time, draw calls, swap and GPU time, then the chosen `scale` and
`msaa`. The first line also names the renderer, its GL version and how many
skins use a palette texture or a uniform array.

The default figure is the shared practice figure; set `VRM` to use another
VRM 0.x figure. It is copied to `assets/figure/man.vrm`, which is not
tracked.
