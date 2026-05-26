#include "deui_wifi_portal_html.h"

#include "deui_ota.h"
#include "deui_wifi_internal.h"
#include "deui_weight_stop.h"

#include <stdio.h>
#include <string.h>

static const char PORTAL_FONT_CSS[] =
    "<style>"
    "@font-face{font-family:'Lab Grotesque';font-style:normal;font-weight:400;font-display:swap;"
    "src:url('/fonts/lab-regular.woff2') format('woff2');}"
    "@font-face{font-family:'Lab Grotesque';font-style:normal;font-weight:500;font-display:swap;"
    "src:url('/fonts/lab-medium.woff2') format('woff2');}"
    "@font-face{font-family:'Lab Grotesque';font-style:normal;font-weight:700;font-display:swap;"
    "src:url('/fonts/lab-bold.woff2') format('woff2');}"
    "</style>";

static const char PORTAL_CSS[] =
    "<style>"
    ":root{"
    "--page:#171717;--html-bg:#171717;--text:#EBE8E8;--text-secondary:#757575;"
    "--card:#000000;--divider:#272727;--field-surface:#000000;--input-text:#EBE8E8;"
    "--btn-bg:#272727;--btn-text:#EBE8E8;"
    "--deui-red:#B4000B;--deui-red-hover:#9A0009;--deui-blue:#3F9CF2;--adj-btn-bg:#171717;"
    "}"
    "*,*::before,*::after{box-sizing:border-box;}"
    "html.dark{font-family:'Lab Grotesque',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
    "background:var(--html-bg);color:var(--text);-webkit-text-size-adjust:100%;touch-action:manipulation;}"
    "body{margin:0;min-height:100vh;background:var(--page);font-weight:500;font-size:1.25rem;line-height:1.5;"
    "padding:max(16px,env(safe-area-inset-top)) 16px max(24px,env(safe-area-inset-bottom));}"
    ".wrap{max-width:28rem;margin:0 auto;}"
    ".page-top{margin:0 0 1.5rem;}"
    ".logo{display:block;width:48px;height:48px;object-fit:contain;}"
    ".page-title{margin:1rem 0 0;font-size:1.75rem;font-weight:500;line-height:normal;color:var(--text);}"
    ".back{display:inline-block;margin:0 0 1.25rem;color:var(--deui-blue);text-decoration:none;font-weight:500;"
    "font-size:1rem;-webkit-tap-highlight-color:transparent;}"
    ".back:active{opacity:0.8;}"
    ".page-heading{margin:0 0 0.75rem;font-size:0.625rem;line-height:2rem;letter-spacing:0.15em;"
    "text-transform:uppercase;color:var(--text-secondary);font-weight:500;}"
    ".card{background:var(--card);border-radius:16px;padding:1.25rem;box-shadow:0 0 10px 5px rgba(0,0,0,0.25);}"
    ".card+.card{margin-top:1rem;}"
    ".control-label{display:block;margin:0 0 0.5rem;font-size:0.625rem;line-height:2rem;letter-spacing:0.15em;"
    "text-transform:uppercase;color:var(--text-secondary);font-weight:500;}"
    ".control-box{display:block;width:100%;max-width:342px;height:64px;border-radius:16px;box-sizing:border-box;"
    "background:var(--field-surface);text-decoration:none;-webkit-tap-highlight-color:transparent;}"
    ".control-select{display:flex;align-items:center;justify-content:center;color:var(--input-text);"
    "font:inherit;font-weight:500;font-size:1.25rem;line-height:64px;text-align:center;}"
    ".control-select--muted{color:var(--text-secondary);}"
    ".control-select:active{opacity:0.8;}"
    ".field-input-wrap{width:100%;max-width:342px;height:64px;border-radius:16px;overflow:hidden;"
    "background:var(--field-surface);box-sizing:border-box;}"
    ".wifi-form{display:flex;flex-direction:column;align-items:flex-start;width:100%;}"
    ".wifi-form .control-box,.wifi-form .field-input-wrap,.wifi-form .btn,.wifi-form hr{width:100%;"
    "max-width:342px;box-sizing:border-box;}"
    ".field-input{display:block;width:100%;height:100%;margin:0;padding:0 1rem;border:0;border-radius:16px;"
    "background:var(--field-surface);color:var(--input-text);font:inherit;font-weight:500;font-size:1.25rem;"
    "line-height:64px;text-align:center;outline:none;-webkit-appearance:none;appearance:none;"
    "-webkit-tap-highlight-color:transparent;}"
    ".field-input:focus{color:var(--text);}"
    ".field-input::placeholder{color:#404040;opacity:1;}"
    ".field-block{margin-top:1rem;}"
    ".btn{display:flex;align-items:center;justify-content:center;width:100%;max-width:342px;box-sizing:border-box;"
    "min-height:64px;margin:1rem 0 0;padding:0.75rem 1rem;border:0;border-radius:8px;"
    "background:var(--btn-bg);color:var(--btn-text);font:inherit;font-weight:500;font-size:1.25rem;"
    "line-height:1.25;text-align:center;cursor:pointer;-webkit-tap-highlight-color:transparent;"
    "text-decoration:none;appearance:none;}"
    ".btn:disabled{opacity:0.5;cursor:default;}"
    ".btn:active:not(:disabled){opacity:0.8;}"
    ".btn--danger{background:var(--deui-red);color:#FFFFFF;}"
    ".btn--danger:active:not(:disabled){background:var(--deui-red-hover);}"
    ".menu{display:flex;flex-direction:column;gap:12px;margin-top:0;align-items:flex-start;}"
    ".menu-nav-btn{display:flex;align-items:center;justify-content:space-between;width:100%;max-width:342px;"
    "height:96px;padding:0 24px;border:0;border-radius:16px;background:#000;color:#EBE8E8;font-family:inherit;"
    "font-size:24px;font-weight:500;line-height:64px;text-decoration:none;-webkit-tap-highlight-color:transparent;"
    "touch-action:manipulation;appearance:none;}"
    ".menu-nav-btn:active{opacity:0.8;}"
    ".menu-nav-btn__label{display:flex;flex-direction:column;justify-content:center;width:248px;height:96px;"
    "min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;text-align:left;}"
    ".menu-nav-btn__chevron{flex-shrink:0;display:block;width:11px;height:16px;}"
    ".control-block{margin-top:1rem;}"
    ".control-block+.control-block{margin-top:1rem;}"
    ".toggle-shell{position:relative;display:flex;align-items:center;justify-content:center;"
    "width:100%;max-width:21.375rem;height:88px;padding:12px;border-radius:16px;background:var(--card);"
    "box-sizing:border-box;}"
    ".toggle{position:relative;width:312px;height:64px;flex:0 0 auto;}"
    ".toggle-pill{position:absolute;top:50%;left:50%;width:150px;height:64px;margin-left:-75px;"
    "border-radius:12px;background:var(--btn-bg);pointer-events:none;transition:transform 200ms linear;"
    "z-index:1;transform:translateY(-50%);}"
    ".toggle--on .toggle-pill{transform:translate(calc(-75px - 6px),-50%);}"
    ".toggle--off .toggle-pill{transform:translate(calc(75px + 6px),-50%);}"
    ".toggle-labels{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;gap:12px;"
    "pointer-events:none;z-index:2;}"
    ".toggle-label--on{left:0;}"
    ".toggle-label--off{right:0;}"
    ".toggle-label{position:absolute;width:150px;height:64px;display:flex;align-items:center;"
    "justify-content:center;font-size:1.25rem;font-weight:500;color:var(--text);opacity:0;"
    "transition:opacity 200ms ease-out;}"
    ".toggle--on .toggle-label--on,.toggle--off .toggle-label--off{opacity:1;}"
    ".toggle-hit{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;gap:12px;z-index:3;}"
    ".toggle-btn{width:150px;height:64px;border:0;border-radius:12px;background:transparent;"
    "color:var(--text-secondary);font:inherit;font-weight:500;font-size:1.25rem;cursor:pointer;"
    "-webkit-tap-highlight-color:transparent;appearance:none;}"
    ".toggle-btn:active{opacity:0.8;}"
    ".weight-panel{margin-top:1rem;}"
    ".weight-panel[hidden]{display:none;}"
    "hr{border:0;height:1px;background:var(--divider);margin:1.25rem 0 0;}"
    ".net-list{list-style:none;margin:0;padding:0;}"
    ".net-list li{border-bottom:1px solid var(--divider);}"
    ".net-list li:last-child{border-bottom:0;}"
    ".net-list a{display:block;padding:1rem 0.25rem;color:var(--text);text-decoration:none;font-weight:500;"
    "font-size:1.25rem;line-height:1.5;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}"
    ".net-list a:active{opacity:0.8;}"
    ".empty{color:var(--text-secondary);font-size:1rem;margin:0;}"
    ".connect-card{width:100%;max-width:342px;border-radius:16px;background:var(--card);padding:1.5rem;"
    "box-shadow:0 0 10px 5px rgba(0,0,0,0.25);}"
    ".connect-body{display:flex;align-items:flex-start;gap:1rem;}"
    ".connect-spinner{flex:0 0 40px;width:40px;height:40px;margin-top:0.25rem;border-radius:50%;"
    "border:3px solid var(--divider);border-top-color:var(--text);animation:portal-spin 1s linear infinite;}"
    ".connect-card--success .connect-spinner,.connect-card--failed .connect-spinner{display:none;}"
    ".connect-card--success .connect-status{color:#00B41C;}"
    ".connect-card--failed .connect-status{color:var(--deui-red);}"
    ".connect-copy{flex:1;min-width:0;}"
    ".connect-status{margin:0;font-size:1.5rem;font-weight:500;line-height:1.35;color:var(--text);}"
    ".connect-detail{margin:0.5rem 0 0;font-size:1rem;font-weight:500;line-height:1.5;color:var(--text-secondary);}"
    ".connect-hint{margin:1rem 0 0;font-size:0.875rem;line-height:1.45;color:var(--text-secondary);}"
    ".connect-actions{margin-top:1.25rem;display:none;flex-direction:column;gap:12px;align-items:flex-start;}"
    ".connect-actions.is-visible{display:flex;}"
    "@keyframes portal-spin{to{transform:rotate(360deg);}}"
    ".adjuster{display:flex;align-items:center;justify-content:space-between;gap:12px;border-radius:16px;"
    "padding:12px;background:var(--card);max-width:21.375rem;}"
    ".adj-btn{flex:0 0 88px;width:88px;max-width:28vw;height:64px;border:0;border-radius:12px;"
    "background:var(--adj-btn-bg);cursor:pointer;display:flex;align-items:center;justify-content:center;"
    "-webkit-tap-highlight-color:transparent;touch-action:manipulation;}"
    ".adj-btn:active{opacity:0.8;background:#272727;}"
    ".adj-value{flex:1;min-width:0;text-align:center;font-size:1.25rem;font-weight:500;color:var(--text);}"
    ".adj-btn img{display:block;}"
    ".adj-btn img.minus-icon{height:4px;width:16px;}"
    ".adj-btn img.plus-icon{height:16px;width:16px;}"
    "</style>";

static char s_portal_wifi_body[1600];

static const char PORTAL_MENU_CHEVRON_SVG[] =
    "<svg class='menu-nav-btn__chevron' xmlns='http://www.w3.org/2000/svg' width='11' height='16' "
    "viewBox='0 0 11 16' fill='none' aria-hidden='true'>"
    "<path d='M1.55879 1.43838L8.62983 7.96555L1.56273 14.4891' stroke='#EBE8E8' stroke-width='3' "
    "stroke-linecap='round' stroke-linejoin='round'/></svg>";

static const char PORTAL_CONNECT_JS[] =
    "<script>"
    "function setConnectCard(phase){"
    "var card=document.getElementById('connectCard');"
    "if(!card){return;}"
    "card.classList.remove('connect-card--success','connect-card--failed');"
    "if(phase==='success'){card.classList.add('connect-card--success');}"
    "if(phase==='failed'){card.classList.add('connect-card--failed');}}"
    "function setConnectActions(visible){"
    "var el=document.getElementById('connectActions');"
    "if(el){el.classList.toggle('is-visible',!!visible);}}"
    "function renderProvisionStatus(j){"
    "var status=document.getElementById('connectStatus');"
    "var detail=document.getElementById('connectDetail');"
    "var hint=document.getElementById('connectHint');"
    "if(!status||!detail){return;}"
    "var ssid=j.ssid||'';"
    "if(j.phase==='success'){"
    "status.textContent='Connected';"
    "detail.textContent=j.message||('DEUI is on '+ssid+'.');"
    "if(j.ip){detail.textContent+=' IP: '+j.ip;}"
    "if(hint){hint.textContent='Join the same Wi-Fi on your phone, then open http://deui.local/ or the IP shown above.';}"
    "setConnectCard('success');setConnectActions(true);return;}"
    "if(j.phase==='failed'){"
    "status.textContent='Could not connect';"
    "detail.textContent=j.message||'Check the network password and try again.';"
    "if(hint){hint.textContent='';}"
    "setConnectCard('failed');setConnectActions(true);return;}"
    "status.textContent='Connecting';"
    "detail.textContent=j.message||(ssid?('Joining \"'+ssid+'\"...'):'Joining your Wi-Fi network...');"
    "if(hint){hint.textContent='This may take up to a minute. Keep your phone on the DEUI setup network.';}"
    "setConnectCard('connecting');setConnectActions(false);}"
    "var connectPollTimer=0;"
    "async function pollProvisionStatus(){"
    "try{"
    "var r=await fetch('/api/wifi-status');"
    "if(!r.ok){return;}"
    "var j=await r.json();"
    "renderProvisionStatus(j);"
    "if(j.phase==='success'||j.phase==='failed'){"
    "if(connectPollTimer){clearInterval(connectPollTimer);connectPollTimer=0;}}"
    "}catch(e){}}"
    "connectPollTimer=setInterval(pollProvisionStatus,1000);"
    "pollProvisionStatus();"
    "</script>";

static const char PORTAL_WEIGHT_JS[] =
    "<script>"
    "function setStopWeightDisplay(g){"
    "var el=document.getElementById('stopWeightVal');"
    "if(el){el.textContent=(g<=0?'0':String(Math.round(g)));}}"
    "var adjStopWeightAt=0;"
    "async function adjStopWeight(d){"
    "var n=Date.now();if(n-adjStopWeightAt<500){return;}adjStopWeightAt=n;"
    "try{"
    "var r=await fetch('/api/stop-weight',{method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:'delta='+encodeURIComponent(d)});"
    "if(!r.ok){return;}"
    "var j=await r.json();"
    "setStopWeightDisplay(j.grams);"
    "setStopWeightEnabled(j.enabled==='true'||j.enabled===true);"
    "}catch(e){}}"
    "function setStopWeightEnabled(on){"
    "var t=document.getElementById('stopWeightToggle');"
    "var p=document.getElementById('weightPanel');"
    "if(t){t.classList.toggle('toggle--on',on);t.classList.toggle('toggle--off',!on);}"
    "if(p){p.hidden=!on;}}"
    "async function setStopWeightEnabledRemote(on){"
    "try{"
    "var r=await fetch('/api/stop-weight',{method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:'enabled='+(on?'1':'0')});"
    "if(!r.ok){return;}"
    "var j=await r.json();"
    "setStopWeightEnabled(j.enabled==='true'||j.enabled===true);"
    "setStopWeightDisplay(j.grams);"
    "}catch(e){}}"
    "</script>";

static const char PORTAL_OTA_JS[] =
    "<script>"
    "function setOtaCard(phase){"
    "var card=document.getElementById('otaCard');"
    "if(!card){return;}"
    "card.classList.remove('connect-card--success','connect-card--failed');"
    "if(phase==='up_to_date'){card.classList.add('connect-card--success');}"
    "if(phase==='failed'||phase==='no_internet'){card.classList.add('connect-card--failed');}}"
    "function renderOtaStatus(j){"
    "var card=document.getElementById('otaCard');"
    "var status=document.getElementById('otaStatus');"
    "var detail=document.getElementById('otaDetail');"
    "var btn=document.getElementById('otaCheckBtn');"
    "if(!status||!detail||!card){return;}"
    "var phase=j.phase||'idle';"
    "if(phase==='checking'||phase==='downloading'||phase==='updating'){"
    "status.textContent=phase==='checking'?'Checking for updates…':'Installing update. Device will restart…';"
    "detail.textContent=j.message||'';"
    "setOtaCard('checking');"
    "if(btn){btn.disabled=true;}"
    "return;}"
    "if(phase==='up_to_date'){"
    "status.textContent=\"You're on the latest version.\";"
    "detail.textContent=j.message||'';"
    "setOtaCard('up_to_date');"
    "if(btn){btn.disabled=false;}"
    "return;}"
    "if(phase==='no_internet'){"
    "status.textContent='Connect to home Wi-Fi first';"
    "detail.textContent=j.message||'Connect to home Wi-Fi to check for updates.';"
    "setOtaCard('no_internet');"
    "if(btn){btn.disabled=false;}"
    "return;}"
    "if(phase==='failed'){"
    "status.textContent='Update check failed';"
    "detail.textContent=j.message||'Try again later.';"
    "setOtaCard('failed');"
    "if(btn){btn.disabled=false;}"
    "return;}"
    "status.textContent='Ready';"
    "detail.textContent='Tap Check for updates when connected to home Wi-Fi.';"
    "setOtaCard('idle');"
    "if(btn){btn.disabled=false;}}"
    "var otaPollTimer=0;"
    "async function pollOtaStatus(){"
    "try{"
    "var r=await fetch('/api/ota-status');"
    "if(!r.ok){return;}"
    "var j=await r.json();"
    "renderOtaStatus(j);"
    "if(j.phase==='up_to_date'||j.phase==='failed'||j.phase==='no_internet'||j.phase==='idle'){"
    "if(otaPollTimer){clearInterval(otaPollTimer);otaPollTimer=0;}}"
    "}catch(e){}}"
    "async function startOtaCheck(){"
    "var btn=document.getElementById('otaCheckBtn');"
    "if(btn){btn.disabled=true;}"
    "renderOtaStatus({phase:'checking',message:'Checking for updates…'});"
    "try{"
    "var r=await fetch('/api/ota-check',{method:'POST'});"
    "if(!r.ok){renderOtaStatus({phase:'failed',message:'Update check failed. Try again later.'});return;}"
    "if(otaPollTimer){clearInterval(otaPollTimer);}"
    "otaPollTimer=setInterval(pollOtaStatus,1000);"
    "pollOtaStatus();"
    "}catch(e){renderOtaStatus({phase:'failed',message:'Update check failed. Try again later.'});}}"
    "</script>";

static esp_err_t portal_send_page_header(httpd_req_t *req, const char *page_title) {
  esp_err_t err = httpd_resp_sendstr_chunk(req, "<header class='page-top'>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req,
                                 "<img class='logo' src='/img/deui-logo.png' width='48' height='48' "
                                 "alt='DEUI'/>");
  if (err != ESP_OK) {
    return err;
  }
  if (page_title != NULL && page_title[0] != '\0') {
    err = httpd_resp_sendstr_chunk(req, "<h1 class='page-title'>");
    if (err != ESP_OK) {
      return err;
    }
    err = httpd_resp_sendstr_chunk(req, page_title);
    if (err != ESP_OK) {
      return err;
    }
    err = httpd_resp_sendstr_chunk(req, "</h1>");
    if (err != ESP_OK) {
      return err;
    }
  }
  return httpd_resp_sendstr_chunk(req, "</header>");
}

static esp_err_t portal_begin(httpd_req_t *req, const char *title, const char *page_title, bool show_back) {
  esp_err_t err;

  httpd_resp_set_type(req, "text/html");
  err = httpd_resp_sendstr_chunk(req, "<!doctype html><html lang='en' class='dark'><head><meta charset='utf-8'>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req,
                                 "<meta name='viewport' content='width=device-width,initial-scale=1,"
                                 "viewport-fit=cover'>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "<meta name='theme-color' content='#171717'>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "<meta name='color-scheme' content='dark'>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "<link rel='icon' href='/favicon.ico' type='image/png'>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "<title>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, title);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "</title>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_FONT_CSS);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_CSS);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "</head><body><div class='wrap'>");
  if (err != ESP_OK) {
    return err;
  }

  err = portal_send_page_header(req, page_title);
  if (err != ESP_OK) {
    return err;
  }
  if (show_back) {
    err = httpd_resp_sendstr_chunk(req, "<a class='back' href='/'>&larr; Back</a>");
  }
  return err;
}

static esp_err_t portal_end(httpd_req_t *req) {
  esp_err_t err = httpd_resp_sendstr_chunk(req, "</div></body></html>");
  if (err != ESP_OK) {
    return err;
  }
  return httpd_resp_sendstr_chunk(req, NULL);
}

static void html_escape(const char *src, char *dst, size_t dst_size) {
  size_t out = 0;

  if (src == NULL || dst == NULL || dst_size == 0) {
    return;
  }
  while (*src != '\0' && out + 1 < dst_size) {
    if (*src == '&') {
      if (out + 5 >= dst_size) {
        break;
      }
      memcpy(dst + out, "&amp;", 5);
      out += 5;
    } else if (*src == '<') {
      if (out + 4 >= dst_size) {
        break;
      }
      memcpy(dst + out, "&lt;", 4);
      out += 4;
    } else if (*src == '>') {
      if (out + 4 >= dst_size) {
        break;
      }
      memcpy(dst + out, "&gt;", 4);
      out += 4;
    } else if (*src == '"') {
      if (out + 6 >= dst_size) {
        break;
      }
      memcpy(dst + out, "&quot;", 6);
      out += 6;
    } else {
      dst[out++] = *src;
    }
    src++;
  }
  dst[out] = '\0';
}

static void url_encode_component(const char *src, char *dst, size_t dst_size) {
  static const char hex[] = "0123456789ABCDEF";
  size_t out = 0;

  if (src == NULL || dst == NULL || dst_size == 0) {
    return;
  }
  while (*src != '\0' && out + 1 < dst_size) {
    const unsigned char ch = (unsigned char)*src;
    const bool safe =
        (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' ||
        ch == '_' || ch == '.' || ch == '~';
    if (safe) {
      dst[out++] = (char)ch;
    } else if (out + 3 < dst_size) {
      dst[out++] = '%';
      dst[out++] = hex[ch >> 4];
      dst[out++] = hex[ch & 0x0f];
    } else {
      break;
    }
    src++;
  }
  dst[out] = '\0';
}

esp_err_t deui_wifi_portal_send_home(httpd_req_t *req) {
  esp_err_t err = portal_begin(req, "DEUI", "SETUP", false);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "<nav class='menu' aria-label='SETUP'>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req,
                                 "<a class='menu-nav-btn' href='/wifi'>"
                                 "<span class='menu-nav-btn__label'>WIFI</span>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_MENU_CHEVRON_SVG);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "</a>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req,
                                 "<a class='menu-nav-btn' href='/weight'>"
                                 "<span class='menu-nav-btn__label'>STOP AT WEIGHT</span>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_MENU_CHEVRON_SVG);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "</a>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req,
                                 "<a class='menu-nav-btn' href='/updates'>"
                                 "<span class='menu-nav-btn__label'>SOFTWARE UPDATE</span>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_MENU_CHEVRON_SVG);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "</a></nav>");
  if (err != ESP_OK) {
    return err;
  }
  return portal_end(req);
}

esp_err_t deui_wifi_portal_send_wifi(httpd_req_t *req, const char *prefill_ssid) {
  char *const body = s_portal_wifi_body;
  char ssid_value_esc[66];
  const char *ssid_raw = (prefill_ssid != NULL && prefill_ssid[0] != '\0') ? prefill_ssid : "";
  const bool has_ssid = ssid_raw[0] != '\0';
  esp_err_t err;

  html_escape(ssid_raw, ssid_value_esc, sizeof(ssid_value_esc));

  err = portal_begin(req, "WIFI — DEUI", "WIFI", true);
  if (err != ESP_OK) {
    return err;
  }

  (void)snprintf(
      body, sizeof(s_portal_wifi_body),
      "<div class='wifi-form'>"
      "<form method='POST' action='/save'>"
      "<label class='control-label' for='ssid_display'>WIFI NAME</label>"
      "<a class='control-box control-select%s' id='ssid_display' href='/scan'>%s</a>"
      "<input type='hidden' name='ssid' value='%s'/>"
      "<div class='field-block'>"
      "<label class='control-label' for='password'>WIFI PASSWORD</label>"
      "<div class='field-input-wrap'>"
      "<input class='field-input' id='password' name='password' type='password' autocomplete='off' "
      "placeholder='Password'/>"
      "</div></div>"
      "<button class='btn' type='submit'>Save &amp; Connect</button>"
      "</form>"
      "<hr/>"
      "<form method='POST' action='/reset'>"
      "<button class='btn btn--danger' type='submit'>Reset Saved Network</button>"
      "</form></div>",
      has_ssid ? "" : " control-select--muted", has_ssid ? ssid_value_esc : "Select Network", ssid_value_esc);

  err = httpd_resp_sendstr_chunk(req, body);
  if (err != ESP_OK) {
    return err;
  }
  return portal_end(req);
}

esp_err_t deui_wifi_portal_send_weight(httpd_req_t *req) {
  char weight_display[8];
  char toggle_chunk[1280];
  const float stop_g = deui_weight_stop_get_target_g();
  const bool enabled = deui_weight_stop_is_enabled();
  esp_err_t err;

  (void)snprintf(weight_display, sizeof(weight_display), "%.0f", stop_g <= 0.0f ? 0.0f : stop_g);

  err = portal_begin(req, "STOP AT WEIGHT — DEUI", "Stop at Weight", true);
  if (err != ESP_OK) {
    return err;
  }

  (void)snprintf(
      toggle_chunk, sizeof(toggle_chunk),
      "<div class='control-block'>"
      "<label class='control-label'>Enable Stop at Weight</label>"
      "<div class='toggle-shell'>"
      "<div id='stopWeightToggle' class='toggle %s' role='group' aria-label='Enable stop at weight'>"
      "<div class='toggle-pill'></div>"
      "<div class='toggle-labels'>"
      "<span class='toggle-label toggle-label--on'>On</span>"
      "<span class='toggle-label toggle-label--off'>Off</span>"
      "</div>"
      "<div class='toggle-hit'>"
      "<button type='button' class='toggle-btn' onclick='setStopWeightEnabledRemote(true)'>On</button>"
      "<button type='button' class='toggle-btn' onclick='setStopWeightEnabledRemote(false)'>Off</button>"
      "</div>"
      "</div></div></div>"
      "<div class='weight-panel' id='weightPanel'%s>"
      "<div class='adjuster'>"
      "<button type='button' class='adj-btn' aria-label='Decrease weight' onclick='adjStopWeight(-1)'>"
      "<img class='minus-icon' src='/img/minus-dark.png' alt=''/>"
      "</button>"
      "<div class='adj-value' id='stopWeightVal'>%s</div>"
      "<button type='button' class='adj-btn' aria-label='Increase weight' onclick='adjStopWeight(1)'>"
      "<img class='plus-icon' src='/img/plus-dark.png' alt=''/>"
      "</button>"
      "</div></div>",
      enabled ? "toggle--on" : "toggle--off", enabled ? "" : " hidden", weight_display);

  err = httpd_resp_sendstr_chunk(req, toggle_chunk);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_WEIGHT_JS);
  if (err != ESP_OK) {
    return err;
  }
  return portal_end(req);
}

esp_err_t deui_wifi_portal_send_connecting(httpd_req_t *req, const char *ssid) {
  char ssid_esc[66];
  char body[768];
  esp_err_t err;

  html_escape(ssid != NULL ? ssid : "", ssid_esc, sizeof(ssid_esc));

  err = portal_begin(req, "Connecting — DEUI", "Connecting", false);
  if (err != ESP_OK) {
    return err;
  }

  (void)snprintf(
      body, sizeof(body),
      "<div class='connect-card' id='connectCard'>"
      "<div class='connect-body'>"
      "<div class='connect-spinner' aria-hidden='true'></div>"
      "<div class='connect-copy'>"
      "<p class='connect-status' id='connectStatus'>Connecting</p>"
      "<p class='connect-detail' id='connectDetail'>Joining \"%s\"...</p>"
      "<p class='connect-hint' id='connectHint'>"
      "This may take up to a minute. Keep your phone on the DEUI setup network."
      "</p></div></div></div>"
      "<div class='connect-actions' id='connectActions'>"
      "<a class='menu-nav-btn' href='/wifi'><span class='menu-nav-btn__label'>Try again</span>",
      ssid_esc);
  err = httpd_resp_sendstr_chunk(req, body);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_MENU_CHEVRON_SVG);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "</a>"
                                 "<a class='menu-nav-btn' href='/'>"
                                 "<span class='menu-nav-btn__label'>Back to setup</span>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_MENU_CHEVRON_SVG);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "</a></div>");
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_CONNECT_JS);
  if (err != ESP_OK) {
    return err;
  }
  return portal_end(req);
}

esp_err_t deui_wifi_portal_send_message(httpd_req_t *req, const char *title, const char *message) {
  char body[384];
  esp_err_t err;

  err = portal_begin(req, title != NULL ? title : "DEUI", title != NULL ? title : "DEUI", true);
  if (err != ESP_OK) {
    return err;
  }

  (void)snprintf(body, sizeof(body),
                 "<div class='card'><p class='lead' style='margin:0;color:var(--text-secondary);font-size:1rem;"
                 "font-weight:400;'>%s</p></div>",
                 message != NULL ? message : "");
  err = httpd_resp_sendstr_chunk(req, body);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "<a class='back' href='/'>Back to menu</a>");
  if (err != ESP_OK) {
    return err;
  }
  return portal_end(req);
}

esp_err_t deui_wifi_portal_send_updates(httpd_req_t *req) {
  char body[1024];
  char version_esc[48];
  esp_err_t err;

  html_escape(deui_ota_get_current_version(), version_esc, sizeof(version_esc));

  err = portal_begin(req, "SOFTWARE UPDATE — DEUI", "Software Update", true);
  if (err != ESP_OK) {
    return err;
  }

  (void)snprintf(
      body, sizeof(body),
      "<p class='page-heading'>Current version</p>"
      "<div class='card'><p style='margin:0;font-size:1.25rem;font-weight:500;color:var(--text);'>%s</p></div>"
      "<button class='btn' type='button' id='otaCheckBtn' onclick='startOtaCheck()'>Check for updates</button>"
      "<div class='connect-card' id='otaCard' style='margin-top:1rem;'>"
      "<div class='connect-body'>"
      "<div class='connect-spinner' id='otaSpinner' aria-hidden='true'></div>"
      "<div class='connect-copy'>"
      "<p class='connect-status' id='otaStatus'>Ready</p>"
      "<p class='connect-detail' id='otaDetail'>Tap Check for updates when connected to home Wi-Fi.</p>"
      "</div></div></div>",
      version_esc);

  err = httpd_resp_sendstr_chunk(req, body);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, PORTAL_OTA_JS);
  if (err != ESP_OK) {
    return err;
  }
  return portal_end(req);
}

esp_err_t deui_wifi_portal_send_scan(httpd_req_t *req, const wifi_ap_record_t *records, uint16_t count) {
  char chunk[256];
  esp_err_t err;

  err = portal_begin(req, "Select Network — DEUI", "Select Network", true);
  if (err != ESP_OK) {
    return err;
  }
  err = httpd_resp_sendstr_chunk(req, "<div class='card'>");
  if (err != ESP_OK) {
    return err;
  }

  if (count == 0 || records == NULL) {
    err = httpd_resp_sendstr_chunk(req, "<p class='empty'>No networks found. Try again in a moment.</p>");
    if (err != ESP_OK) {
      return err;
    }
  } else {
    err = httpd_resp_sendstr_chunk(req, "<ul class='net-list'>");
    if (err != ESP_OK) {
      return err;
    }
    for (uint16_t i = 0; i < count; ++i) {
      char ssid_esc[66];
      char ssid_enc[66];
      html_escape((const char *)records[i].ssid, ssid_esc, sizeof(ssid_esc));
      url_encode_component((const char *)records[i].ssid, ssid_enc, sizeof(ssid_enc));
      (void)snprintf(chunk, sizeof(chunk), "<li><a href='/wifi?ssid=%s'>%s</a></li>", ssid_enc, ssid_esc);
      err = httpd_resp_sendstr_chunk(req, chunk);
      if (err != ESP_OK) {
        return err;
      }
    }
    err = httpd_resp_sendstr_chunk(req, "</ul>");
    if (err != ESP_OK) {
      return err;
    }
  }

  err = httpd_resp_sendstr_chunk(req, "</div>");
  if (err != ESP_OK) {
    return err;
  }
  return portal_end(req);
}
