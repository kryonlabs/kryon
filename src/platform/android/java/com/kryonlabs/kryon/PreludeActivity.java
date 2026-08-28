package com.kryonlabs.kryon;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetFileDescriptor;
import android.graphics.Color;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public class PreludeActivity extends Activity implements SurfaceHolder.Callback {
    public static final String META_VIDEO_ASSET = "com.kryonlabs.kryon.prelude.VIDEO_ASSET";
    public static final String META_SUBTITLE_ASSET = "com.kryonlabs.kryon.prelude.SUBTITLE_ASSET";
    public static final String META_NATIVE_ACTIVITY = "com.kryonlabs.kryon.prelude.NATIVE_ACTIVITY";
    public static final String META_SKIP_ON_TAP = "com.kryonlabs.kryon.prelude.SKIP_ON_TAP";

    private static final String DEFAULT_NATIVE_ACTIVITY = "android.app.NativeActivity";
    private static final long SUBTITLE_TICK_MS = 80L;

    private final Handler handler = new Handler();
    private final List<Cue> cues = new ArrayList<>();
    private FrameLayout videoFrame;
    private MediaPlayer player;
    private SurfaceView surface;
    private TextView subtitles;
    private String videoAsset;
    private String subtitleAsset;
    private String nativeActivity;
    private boolean skipOnTap = true;
    private boolean launchedGame;
    private int videoWidth;
    private int videoHeight;

    private final Runnable subtitleTick = new Runnable() {
        @Override
        public void run() {
            updateSubtitle();
            if (!launchedGame && player != null) {
                handler.postDelayed(this, SUBTITLE_TICK_MS);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN);
        enterImmersiveMode();
        loadConfig();

        if (videoAsset == null || videoAsset.length() == 0) {
            launchGame();
            return;
        }

        surface = new SurfaceView(this);
        surface.getHolder().addCallback(this);

        subtitles = new TextView(this);
        subtitles.setTextColor(Color.WHITE);
        subtitles.setTextSize(18.0f);
        subtitles.setGravity(Gravity.CENTER);
        subtitles.setShadowLayer(4.0f, 1.0f, 1.0f, Color.BLACK);
        subtitles.setBackgroundColor(0x99000000);
        subtitles.setPadding(18, 10, 18, 10);
        subtitles.setVisibility(View.INVISIBLE);

        FrameLayout root = new FrameLayout(this);
        videoFrame = new FrameLayout(this);
        videoFrame.setBackgroundColor(Color.BLACK);
        videoFrame.addView(surface, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT,
            Gravity.CENTER));

        root.setBackgroundColor(Color.BLACK);
        root.addView(videoFrame, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT,
            Gravity.CENTER));

        FrameLayout.LayoutParams subtitleParams = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        subtitleParams.setMargins(24, 0, 24, 42);
        videoFrame.addView(subtitles, subtitleParams);
        setContentView(root);

        root.post(new Runnable() {
            @Override
            public void run() {
                fitVideoFrame(videoWidth, videoHeight);
            }
        });

        loadSubtitles();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        startVideo(holder);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        fitVideoFrame(videoWidth, videoHeight);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        releasePlayer();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enterImmersiveMode();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (player != null && player.isPlaying()) {
            player.pause();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        enterImmersiveMode();
        if (player != null && !launchedGame) {
            player.start();
        }
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacksAndMessages(null);
        releasePlayer();
        super.onDestroy();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (skipOnTap && event.getAction() == MotionEvent.ACTION_UP) {
            launchGame();
            return true;
        }
        return true;
    }

    private void loadConfig() {
        nativeActivity = DEFAULT_NATIVE_ACTIVITY;
        try {
            ActivityInfo info = getPackageManager().getActivityInfo(getComponentName(),
                PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                return;
            }
            videoAsset = info.metaData.getString(META_VIDEO_ASSET);
            subtitleAsset = info.metaData.getString(META_SUBTITLE_ASSET);
            String configuredNativeActivity = info.metaData.getString(META_NATIVE_ACTIVITY);
            if (configuredNativeActivity != null && configuredNativeActivity.length() > 0) {
                nativeActivity = configuredNativeActivity;
            }
            skipOnTap = info.metaData.getBoolean(META_SKIP_ON_TAP, true);
        } catch (PackageManager.NameNotFoundException ignored) {
        }
    }

    private void enterImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    private void startVideo(SurfaceHolder holder) {
        if (player != null || launchedGame) {
            return;
        }
        try {
            AssetFileDescriptor video = getAssets().openFd(videoAsset);
            player = new MediaPlayer();
            player.setDataSource(video.getFileDescriptor(), video.getStartOffset(), video.getLength());
            video.close();
            player.setDisplay(holder);
            player.setScreenOnWhilePlaying(true);
            player.setOnPreparedListener(new MediaPlayer.OnPreparedListener() {
                @Override
                public void onPrepared(MediaPlayer mediaPlayer) {
                    videoWidth = mediaPlayer.getVideoWidth();
                    videoHeight = mediaPlayer.getVideoHeight();
                    fitVideoFrame(videoWidth, videoHeight);
                    mediaPlayer.start();
                    handler.post(subtitleTick);
                }
            });
            player.setOnVideoSizeChangedListener(new MediaPlayer.OnVideoSizeChangedListener() {
                @Override
                public void onVideoSizeChanged(MediaPlayer mediaPlayer, int width, int height) {
                    videoWidth = width;
                    videoHeight = height;
                    fitVideoFrame(width, height);
                }
            });
            player.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
                @Override
                public void onCompletion(MediaPlayer mediaPlayer) {
                    launchGame();
                }
            });
            player.setOnErrorListener(new MediaPlayer.OnErrorListener() {
                @Override
                public boolean onError(MediaPlayer mediaPlayer, int what, int extra) {
                    launchGame();
                    return true;
                }
            });
            player.prepareAsync();
        } catch (Exception ignored) {
            launchGame();
        }
    }

    private void fitVideoFrame(int width, int height) {
        if (videoFrame == null || width <= 0 || height <= 0) {
            return;
        }

        DisplayMetrics metrics = getResources().getDisplayMetrics();
        int screenWidth = metrics.widthPixels;
        int screenHeight = metrics.heightPixels;
        float videoAspect = (float)width / (float)height;
        int fittedWidth = screenWidth;
        int fittedHeight = Math.round((float)screenWidth / videoAspect);

        if (fittedHeight > screenHeight) {
            fittedHeight = screenHeight;
            fittedWidth = Math.round((float)screenHeight * videoAspect);
        }

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
            fittedWidth,
            fittedHeight,
            Gravity.CENTER);
        videoFrame.setLayoutParams(params);
    }

    private void launchGame() {
        if (launchedGame) {
            return;
        }
        launchedGame = true;
        handler.removeCallbacksAndMessages(null);
        releasePlayer();

        Intent intent = new Intent();
        intent.setClassName(getPackageName(), nativeActivity);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivity(intent);
        finish();
        overridePendingTransition(0, 0);
    }

    private void releasePlayer() {
        if (player == null) {
            return;
        }
        try {
            player.stop();
        } catch (IllegalStateException ignored) {
        }
        player.release();
        player = null;
    }

    private void updateSubtitle() {
        if (player == null || subtitles == null) {
            return;
        }

        int position;
        try {
            position = player.getCurrentPosition();
        } catch (IllegalStateException ignored) {
            subtitles.setVisibility(View.INVISIBLE);
            return;
        }

        for (Cue cue : cues) {
            if (position >= cue.startMs && position < cue.endMs) {
                subtitles.setText(cue.text);
                subtitles.setVisibility(View.VISIBLE);
                return;
            }
        }
        subtitles.setVisibility(View.INVISIBLE);
    }

    private void loadSubtitles() {
        if (subtitleAsset == null || subtitleAsset.length() == 0) {
            return;
        }
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
            getAssets().open(subtitleAsset), StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                if (!line.contains("-->")) {
                    continue;
                }
                String[] times = line.split("-->");
                if (times.length != 2) {
                    continue;
                }
                int start = parseTimestampMs(times[0].trim());
                int end = parseTimestampMs(times[1].trim());
                StringBuilder text = new StringBuilder();
                while ((line = reader.readLine()) != null && line.trim().length() > 0) {
                    if (text.length() > 0) {
                        text.append('\n');
                    }
                    text.append(line.trim());
                }
                if (start >= 0 && end > start && text.length() > 0) {
                    cues.add(new Cue(start, end, text.toString()));
                }
            }
        } catch (Exception ignored) {
            cues.clear();
        }
    }

    private static int parseTimestampMs(String value) {
        String normalized = value.replace(',', ':');
        String[] parts = normalized.split(":");
        if (parts.length != 4) {
            return -1;
        }
        try {
            int hours = Integer.parseInt(parts[0]);
            int minutes = Integer.parseInt(parts[1]);
            int seconds = Integer.parseInt(parts[2]);
            int millis = Integer.parseInt(parts[3]);
            return (int)(((hours * 60L + minutes) * 60L + seconds) * 1000L + millis);
        } catch (NumberFormatException ignored) {
            return -1;
        }
    }

    private static final class Cue {
        final int startMs;
        final int endMs;
        final String text;

        Cue(int startMs, int endMs, String text) {
            this.startMs = startMs;
            this.endMs = endMs;
            this.text = text;
        }
    }
}
