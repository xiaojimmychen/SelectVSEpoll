package com.blog4jimmy.hp.selectvsepoll;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {

    private Button testSelectBtn;
    private Button testEpollBtn;
    private Button startSelectServer;
    private Button startEpollServer;
    private boolean selectIsStart;
    private boolean epollIsStart;
    private TextView selectTV;
    private TextView epollTV;

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        testSelectBtn = (Button)findViewById(R.id.test_select);
        testEpollBtn = (Button)findViewById(R.id.test_epoll);
        startSelectServer = (Button)findViewById(R.id.start_select);
        startEpollServer = (Button)findViewById(R.id.start_epoll);
        selectTV = (TextView)findViewById(R.id.select_result);
        epollTV = (TextView)findViewById(R.id.epoll_result);

        testSelectBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Toast.makeText(MainActivity.this, "Select test executing, please wait a moment!", Toast.LENGTH_LONG).show();
                int result = testSelectClient();
                String resultStr = "Select test spend " + Integer.toString(result) + " seconds!";
                selectTV.setText(resultStr);
            }
        });

        testEpollBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Toast.makeText(MainActivity.this, "Epoll test executing, please wait a moment!", Toast.LENGTH_LONG).show();
                int result = testEpollClient();
                String resultStr = "Epoll test spend " + Integer.toString(result) + " seconds!";
                epollTV.setText(resultStr);
            }
        });

        startSelectServer.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(!selectIsStart)
                    selectIsStart = startSelectServer();
                if (selectIsStart) {
                    startSelectServer.setBackgroundColor(getResources().getColor(R.color.colorGreen));
                } else {
                    startSelectServer.setBackgroundColor(getResources().getColor(R.color.colorRed));
                }
            }
        });

        startEpollServer.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(!epollIsStart)
                    epollIsStart = startEpollServer();
                if (epollIsStart) {
                    startEpollServer.setBackgroundColor(getResources().getColor(R.color.colorGreen));
                } else {
                    startEpollServer.setBackgroundColor(getResources().getColor(R.color.colorRed));
                }
            }
        });

        nativeInit();
    }


    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native boolean nativeInit();
    public native boolean startSelectServer();
    public native boolean startEpollServer();
    public native int testSelectClient();
    public native int testEpollClient();
}
