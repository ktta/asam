/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.dodo.android_test;

import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.app.Activity;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.Surface;


public class MainActivity extends Activity 
                          implements SurfaceHolder.Callback
                                     {
    private SurfaceView view;

    native boolean setup();
    native void surface_new(Surface s);
    native void surface_del();

    @Override
    protected void onCreate(Bundle savedInstanceState) 
    {
        super.onCreate(savedInstanceState);
        System.loadLibrary("main");
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);

        view = new SurfaceView(this);
        view.setSystemUiVisibility(SYSTEM_UI_FLAG_FULLSCREEN);
        view.getHolder().addCallback(this);
        setContentView(view);

        setup();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    public void surfaceCreated(SurfaceHolder holder)
    {
      // do nothing, we will handle it in Changed()
    }

    public void surfaceChanged(SurfaceHolder holder, int format,
                               int width, int height)
    {
      surface_new(holder.getSurface());
    }

    public void surfaceDestroyed(SurfaceHolder holder)
    {
      surface_del();
    }
}
