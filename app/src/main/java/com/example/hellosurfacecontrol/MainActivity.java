package com.example.hellosurfacecontrol;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("hellosurfacecontrol");
    }

    private native void nativeInitSurfaceControl(Surface surface);

    private native void nativeUpdateSurfaceControl(Surface surface, int format, int width, int height);
    private native void nativeDestroySurfaceControl(Surface surface);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        SurfaceView surfaceView = findViewById(R.id.surfaceView);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                nativeInitSurfaceControl(holder.getSurface());
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
                nativeUpdateSurfaceControl(holder.getSurface(), format, width, height);
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                nativeDestroySurfaceControl(holder.getSurface());
            }
        });
    }
}