package com.gagguino.monitor;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Base64;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.webkit.JavascriptInterface;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Locale;

/** Local Wi-Fi WebView shell for the ESP32-hosted Gagguino UI. */
public final class MainActivity extends Activity {
    private static final int BG = Color.rgb(11,17,26);
    private static final int PANEL = Color.rgb(22,31,43);
    private static final int FG = Color.rgb(241,244,247);
    private static final int MUTED = Color.rgb(165,180,197);
    private static final int ACCENT = Color.rgb(240,177,124);
    private static final int CSV_REQUEST = 1001;
    private static final int CSV_LIMIT = 256000;

    private WebView browser;
    private FrameLayout viewport;
    private LinearLayout offline;
    private ProgressBar spinner;
    private TextView addressText;
    private LinearLayout toolbar;
    private Button expandToolbar;
    private FrameLayout rootFrame;
    private String address = "";
    private boolean failed;
    private byte[] csvData;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        address = getPreferences(0).getString("address", "");
        createLayout();
        configureBrowser();
        if (address.isEmpty()) promptAddress(true);
        else loadDevice();
    }

    private int dp(float n) {
        return (int) (n * getResources().getDisplayMetrics().density + .5f);
    }

    private Button button(String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(FG);
        b.setTextSize(14);
        return b;
    }

    private void createLayout() {
        rootFrame = new FrameLayout(this);
        rootFrame.setBackgroundColor(BG);
        setContentView(rootFrame);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        rootFrame.addView(root, new FrameLayout.LayoutParams(-1,-1));
        if (Build.VERSION.SDK_INT >= 30) {
            rootFrame.setOnApplyWindowInsetsListener((view, insets) -> {
                android.graphics.Insets bars = insets.getInsets(
                    WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars()
                    | WindowInsets.Type.displayCutout());
                rootFrame.setPadding(bars.left,bars.top,bars.right,bars.bottom);
                return insets;
            });
            rootFrame.requestApplyInsets();
        }

        toolbar = new LinearLayout(this);
        toolbar.setGravity(Gravity.CENTER_VERTICAL);
        toolbar.setPadding(dp(12),0,dp(6),0);
        toolbar.setBackgroundColor(PANEL);
        root.addView(toolbar,new LinearLayout.LayoutParams(-1,dp(54)));

        TextView brand = new TextView(this);
        brand.setText("Gagguino");
        brand.setTextColor(ACCENT);
        brand.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        brand.setTextSize(19);
        toolbar.addView(brand);

        addressText = new TextView(this);
        addressText.setTextSize(11);
        addressText.setTextColor(MUTED);
        addressText.setPadding(dp(12),0,0,0);
        addressText.setSingleLine(true);
        toolbar.addView(addressText,new LinearLayout.LayoutParams(0,-2,1));
        showAddress();

        Button reload = button("⟳");
        reload.setContentDescription("Reload / Оновити");
        reload.setOnClickListener(v -> loadDevice());
        toolbar.addView(reload,new LinearLayout.LayoutParams(dp(44),dp(48)));

        Button settings = button("⚙");
        settings.setContentDescription("Change IP / Змінити IP");
        settings.setOnClickListener(v -> promptAddress(false));
        toolbar.addView(settings,new LinearLayout.LayoutParams(dp(44),dp(48)));

        Button hide = button("⌃");
        hide.setContentDescription("Hide controls / Сховати панель");
        hide.setOnClickListener(v -> setToolbarVisible(false));
        toolbar.addView(hide,new LinearLayout.LayoutParams(dp(44),dp(48)));

        expandToolbar = button("☰");
        expandToolbar.setTextColor(ACCENT);
        expandToolbar.setContentDescription("Show app controls / Показати панель");
        expandToolbar.setBackgroundTintList(android.content.res.ColorStateList.valueOf(PANEL));
        expandToolbar.setElevation(dp(5));
        FrameLayout.LayoutParams toggleLayout = new FrameLayout.LayoutParams(
            dp(44),dp(44),Gravity.TOP | Gravity.END);
        toggleLayout.setMargins(0,dp(6),dp(8),0);
        rootFrame.addView(expandToolbar,toggleLayout);
        expandToolbar.setOnClickListener(v -> setToolbarVisible(true));

        viewport = new FrameLayout(this);
        root.addView(viewport,new LinearLayout.LayoutParams(-1,0,1));

        browser = new WebView(this);
        browser.setBackgroundColor(BG);
        viewport.addView(browser,new FrameLayout.LayoutParams(-1,-1));

        spinner = new ProgressBar(this,null,android.R.attr.progressBarStyleHorizontal);
        spinner.setIndeterminate(true);
        spinner.setVisibility(View.GONE);
        viewport.addView(spinner,new FrameLayout.LayoutParams(-1,dp(3),Gravity.TOP));

        offline = new LinearLayout(this);
        offline.setOrientation(LinearLayout.VERTICAL);
        offline.setGravity(Gravity.CENTER);
        offline.setPadding(dp(22),dp(22),dp(22),dp(22));
        offline.setBackgroundColor(BG);
        viewport.addView(offline,new FrameLayout.LayoutParams(-1,-1));

        TextView msg = new TextView(this);
        msg.setText("ESP32 недоступна / ESP32 offline");
        msg.setGravity(Gravity.CENTER);
        msg.setTextColor(FG);
        msg.setTextSize(20);
        msg.setTypeface(Typeface.DEFAULT,Typeface.BOLD);
        offline.addView(msg);

        TextView hint = new TextView(this);
        hint.setText("Підключіться до Wi-Fi мережі ESP32 і перевірте IP-адресу.\nConnect to the ESP32 Wi-Fi network and check its IP.");
        hint.setTextColor(MUTED);
        hint.setGravity(Gravity.CENTER);
        hint.setTextSize(14);
        hint.setPadding(0,dp(18),0,dp(14));
        offline.addView(hint);

        Button retry = button("Повторити / Retry");
        retry.setOnClickListener(v -> loadDevice());
        offline.addView(retry,new LinearLayout.LayoutParams(-1,dp(54)));

        Button edit = button("Змінити IP / Change IP");
        edit.setOnClickListener(v -> promptAddress(false));
        offline.addView(edit,new LinearLayout.LayoutParams(-1,dp(54)));
        offline.setVisibility(View.GONE);
        setToolbarVisible(getPreferences(0).getBoolean("toolbar_visible",false));
    }

    private void setToolbarVisible(boolean show) {
        if(toolbar == null || expandToolbar == null)return;
        toolbar.setVisibility(show ? View.VISIBLE : View.GONE);
        expandToolbar.setVisibility(show ? View.GONE : View.VISIBLE);
        getPreferences(0).edit().putBoolean("toolbar_visible",show).apply();
    }

    private void showAddress() {
        if(addressText!=null) addressText.setText(address);
    }

    /** Do not let web content navigate or issue HTTP requests to other hosts. */
    private boolean isDevice(Uri uri) {
        return uri!=null && "http".equalsIgnoreCase(uri.getScheme())
            && address.equals(uri.getHost()) && (uri.getPort()==-1 || uri.getPort()==80)
            && uri.getUserInfo()==null;
    }

    private boolean isPrivateIp(String value) {
        if(value==null || !value.matches("[0-9]{1,3}(\\.[0-9]{1,3}){3}"))return false;
        String[] parts=value.split("\\.");
        int[] n=new int[4];
        for(int i=0;i<4;i++) {
            try{n[i]=Integer.parseInt(parts[i]);}catch(NumberFormatException e){return false;}
            if(n[i]>255)return false;
        }
        return n[0]==10 || (n[0]==172 && n[1]>=16 && n[1]<=31)
            || (n[0]==192 && n[1]==168);
    }

    private void promptAddress(boolean mandatory) {
        EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setText(address.isEmpty()?"10.0.0.180":address);
        input.setSelectAllOnFocus(true);
        input.setHint("192.168.1.10");
        LinearLayout container=new LinearLayout(this);
        container.setPadding(dp(22),0,dp(22),0);
        container.addView(input,new LinearLayout.LayoutParams(-1,-2));

        AlertDialog dialog = new AlertDialog.Builder(this)
            .setTitle("ESP32 IP address / IP-адреса")
            .setView(container)
            .setCancelable(!mandatory)
            .setPositiveButton("Connect",null)
            .setNegativeButton(mandatory?null:"Cancel",(d,w)->{})
            .create();
        dialog.setOnShowListener(ignored -> {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
                String host=input.getText().toString().trim();
                if(!isPrivateIp(host)) {
                    input.setError("Enter a private IPv4 address (10.x, 172.16-31.x, 192.168.x)");
                    return;
                }
                address=host;
                getPreferences(0).edit().putString("address",host).apply();
                showAddress();
                dialog.dismiss();
                loadDevice();
            });
        });
        dialog.show();
    }

    @SuppressLint({"SetJavaScriptEnabled","AddJavascriptInterface"})
    private void configureBrowser() {
        WebSettings web=browser.getSettings();
        web.setJavaScriptEnabled(true);
        web.setDomStorageEnabled(true);
        web.setAllowFileAccess(false);
        web.setAllowContentAccess(false);
        web.setJavaScriptCanOpenWindowsAutomatically(false);
        web.setSupportMultipleWindows(false);
        web.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        browser.setOverScrollMode(View.OVER_SCROLL_NEVER);
        browser.setWebChromeClient(new WebChromeClient());
        browser.addJavascriptInterface(new CsvSaver(),"GagguinoCsv");
        browser.setWebViewClient(new WebViewClient() {
            @Override public boolean shouldOverrideUrlLoading(WebView view,WebResourceRequest req){
                return !isDevice(req.getUrl());
            }
            @Override public WebResourceResponse shouldInterceptRequest(WebView view,WebResourceRequest req){
                Uri uri=req.getUrl();
                String scheme=uri.getScheme();
                if(("http".equalsIgnoreCase(scheme) || "https".equalsIgnoreCase(scheme))
                        && !isDevice(uri)){
                    return new WebResourceResponse("text/plain","UTF-8",
                        new ByteArrayInputStream(new byte[0]));
                }
                return null;
            }
            @Override public void onPageStarted(WebView view,String url,android.graphics.Bitmap icon){
                failed=false;
                spinner.setVisibility(View.VISIBLE);
            }
            @Override public void onReceivedError(WebView view,WebResourceRequest req,WebResourceError error){
                if(req.isForMainFrame()) {
                    failed=true;
                    runOnUiThread(()->offline.setVisibility(View.VISIBLE));
                }
            }
            @Override public void onReceivedHttpError(WebView view,WebResourceRequest req,WebResourceResponse resp){
                if(req.isForMainFrame() && resp.getStatusCode()>=400) {
                    failed=true;
                    runOnUiThread(()->offline.setVisibility(View.VISIBLE));
                }
            }
            @Override public void onPageFinished(WebView view,String url){
                spinner.setVisibility(View.GONE);
                if(!failed) {
                    offline.setVisibility(View.GONE);
                    injectCsvDownload();
                }
            }
        });
    }

    private void loadDevice(){
        if(address.isEmpty()) {promptAddress(true);return;}
        offline.setVisibility(View.GONE);
        spinner.setVisibility(View.VISIBLE);
        browser.loadUrl("http://"+address+"/");
    }

    private void injectCsvDownload(){
        // Existing UI exports a blob: URL via a temporary anchor.
        // Intercept it before navigation and use Android's native file picker.
        String script = "(function(){if(window.__gagguinoCsvHook)return;"
            +"window.__gagguinoCsvHook=true;"
            +"document.addEventListener('click',function(e){"
            +"var a=e.target.closest&&e.target.closest('a[download]');"
            +"if(!a||!a.href.startsWith('blob:'))return;"
            +"if(!/^gagguino-shot-[0-9]+\\.csv$/.test(a.download))return;"
            +"e.preventDefault();e.stopImmediatePropagation();"
            +"fetch(a.href).then(function(r){return r.blob()}).then(function(blob){"
            +"if(blob.size>256000)throw Error('CSV too large');"
            +"var reader=new FileReader();reader.onload=function(){"
            +"GagguinoCsv.saveCsv(a.download,reader.result.split(',')[1]);};"
            +"reader.readAsDataURL(blob);});"
            +"},true);})();";
        browser.evaluateJavascript(script,null);
    }

    private class CsvSaver {
        @JavascriptInterface public void saveCsv(String name,String encoded){
            if(name==null || !name.matches("gagguino-shot-[0-9]+\\.csv")
                  || encoded==null || encoded.length()>CSV_LIMIT*2)return;
            byte[] data;
            try{data=Base64.decode(encoded,Base64.DEFAULT);}catch(IllegalArgumentException e){return;}
            if(data.length>CSV_LIMIT)return;
            // Check CSV starts with known archive header before saving.
            String head=new String(data,0,Math.min(data.length,80),java.nio.charset.StandardCharsets.UTF_8);
            if(!head.startsWith("time_s,temperature_c,pressure_bar,"))return;
            runOnUiThread(()->{
                csvData=data;
                Intent save=new Intent(Intent.ACTION_CREATE_DOCUMENT);
                save.addCategory(Intent.CATEGORY_OPENABLE);
                save.setType("text/csv");
                save.putExtra(Intent.EXTRA_TITLE,name);
                startActivityForResult(save,CSV_REQUEST);
            });
        }
    }

    @Override @Deprecated protected void onActivityResult(int req,int result,Intent data){
        super.onActivityResult(req,result,data);
        if(req!=CSV_REQUEST)return;
        if(result==RESULT_OK && data!=null && csvData!=null) {
            try(OutputStream stream=getContentResolver().openOutputStream(data.getData())){
                if(stream==null)throw new IOException("Storage provider unavailable");
                stream.write(csvData);
                Toast.makeText(this,"CSV saved",Toast.LENGTH_SHORT).show();
            }catch(Exception e){
                Toast.makeText(this,"Could not save CSV",Toast.LENGTH_LONG).show();
            }
        }
        csvData=null;
    }

    @Override public void onBackPressed(){
        if(browser.canGoBack())browser.goBack(); else super.onBackPressed();
    }

    @Override protected void onDestroy(){
        if(browser!=null) {
            browser.removeJavascriptInterface("GagguinoCsv");
            browser.destroy();
        }
        super.onDestroy();
    }
}
