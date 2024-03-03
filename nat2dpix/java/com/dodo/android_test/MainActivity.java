package com.dodo.android_test;

import android.content.pm.ActivityInfo;
import android.os.Build;
import android.os.Bundle;
import android.app.Activity;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;

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
