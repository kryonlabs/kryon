package com.kryonlabs.kryon;

import android.content.Context;
import android.text.InputType;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

public final class TextInputBridge {
    public interface Callbacks {
        void commitText(int codepoint);
        void composition(int phase, String text, int cursor, int selectionLength);
        void backspace();
        void enter();
    }

    private final BridgeView view;
    private final InputMethodManager inputMethodManager;
    private final Callbacks callbacks;

    private boolean active = false;
    private int showRequests = 0;
    private int hideRequests = 0;
    private int pendingShowAttempts = 0;
    private final Runnable showKeyboardRunnable = new Runnable() {
        @Override
        public void run() {
            showKeyboardNow();
        }
    };

    public TextInputBridge(Context context, Callbacks callbacks) {
        this.callbacks = callbacks;
        this.view = new BridgeView(context);
        this.view.setAlpha(0.0f);
        this.view.setBackgroundColor(0x00000000);
        this.inputMethodManager =
            (InputMethodManager)context.getSystemService(Context.INPUT_METHOD_SERVICE);
    }

    public View getView() {
        return view;
    }

    public void setVisible(boolean visible) {
        if (inputMethodManager == null) {
            return;
        }

        if (visible) {
            active = true;
            showRequests++;
            pendingShowAttempts = 3;
            showKeyboardNow();
        } else {
            active = false;
            hideRequests++;
            pendingShowAttempts = 0;
            view.removeCallbacks(showKeyboardRunnable);
            view.clearFocus();
            inputMethodManager.hideSoftInputFromWindow(view.getWindowToken(), 0);
        }
    }

    public int showRequestsForTest() {
        return showRequests;
    }

    public int hideRequestsForTest() {
        return hideRequests;
    }

    public boolean hasFocusForTest() {
        return view.hasFocus();
    }

    public InputConnection createInputConnectionForTest(EditorInfo info) {
        return view.onCreateInputConnection(info);
    }

    private void showKeyboardNow() {
        if (!active) {
            return;
        }
        if (!view.isAttachedToWindow()) {
            view.post(showKeyboardRunnable);
            return;
        }

        view.requestFocus();
        inputMethodManager.restartInput(view);
        boolean shown = inputMethodManager.showSoftInput(
            view, InputMethodManager.SHOW_IMPLICIT);
        if (!shown) {
            shown = inputMethodManager.showSoftInput(
                view, InputMethodManager.SHOW_FORCED);
        }
        pendingShowAttempts--;
        if (!shown && pendingShowAttempts > 0) {
            view.postDelayed(showKeyboardRunnable, 80);
        }
    }

    private void commitText(CharSequence text) {
        if (!active || text == null) {
            return;
        }

        for (int i = 0; i < text.length();) {
            int codepoint = Character.codePointAt(text, i);
            if (codepoint >= 32) {
                callbacks.commitText(codepoint);
            }
            i += Character.charCount(codepoint);
        }
    }

    private void commitBackspace() {
        if (active) {
            callbacks.backspace();
        }
    }

    private void commitEnter() {
        if (active) {
            callbacks.enter();
        }
    }

    private boolean handleKeyEvent(KeyEvent event) {
        if (event == null || event.getAction() != KeyEvent.ACTION_DOWN) {
            return false;
        }

        int keyCode = event.getKeyCode();
        if (keyCode == KeyEvent.KEYCODE_DEL) {
            commitBackspace();
            return true;
        }
        if (keyCode == KeyEvent.KEYCODE_ENTER ||
                keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER) {
            commitEnter();
            return true;
        }

        int unicode = event.getUnicodeChar();
        if (unicode >= 32) {
            commitText(new String(Character.toChars(unicode)));
            return true;
        }
        return false;
    }

    private final class BridgeView extends View {
        BridgeView(Context context) {
            super(context);
            setLayoutParams(new ViewGroup.LayoutParams(1, 1));
            setFocusable(true);
            setFocusableInTouchMode(true);
        }

        @Override
        public boolean onCheckIsTextEditor() {
            return true;
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            outAttrs.inputType = InputType.TYPE_CLASS_TEXT |
                InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD |
                InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS;
            outAttrs.imeOptions = EditorInfo.IME_ACTION_DONE |
                EditorInfo.IME_FLAG_NO_EXTRACT_UI;

            return new BaseInputConnection(this, false) {
                private boolean composing;

                @Override
                public boolean setComposingText(CharSequence text,
                                                int newCursorPosition) {
                    String value = text == null ? "" : text.toString();
                    callbacks.composition(composing ? 2 : 1, value,
                        Math.max(0, newCursorPosition - 1), 0);
                    composing = true;
                    return true;
                }

                @Override
                public boolean finishComposingText() {
                    if (composing) {
                        callbacks.composition(4, "", 0, 0);
                        composing = false;
                    }
                    return true;
                }

                @Override
                public boolean commitText(CharSequence text, int newCursorPosition) {
                    if (composing) {
                        callbacks.composition(3, text == null ? "" : text.toString(),
                            Math.max(0, newCursorPosition - 1), 0);
                        composing = false;
                    } else {
                        TextInputBridge.this.commitText(text);
                    }
                    return true;
                }

                @Override
                public boolean deleteSurroundingText(int beforeLength, int afterLength) {
                    if (beforeLength > 0) {
                        commitBackspace();
                    }
                    return true;
                }

                @Override
                public boolean sendKeyEvent(KeyEvent event) {
                    return handleKeyEvent(event) || super.sendKeyEvent(event);
                }

                @Override
                public boolean performEditorAction(int editorAction) {
                    if (editorAction == EditorInfo.IME_ACTION_DONE) {
                        commitEnter();
                        return true;
                    }
                    return super.performEditorAction(editorAction);
                }
            };
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            return handleKeyEvent(event) || super.onKeyDown(keyCode, event);
        }
    }
}
