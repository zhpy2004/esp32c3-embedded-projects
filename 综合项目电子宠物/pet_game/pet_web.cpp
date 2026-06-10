/**
 * @file    pet_web.cpp
 * @brief   WiFi Web Server 实现 — 内嵌 HTML 控制台 + REST API
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "time.h"
#include "pet_web.h"
#include "pet_core.h"
#include "pet_save.h"
#include "game_config.h"
#include "wifi_ntp.h"
#include "ds1307.h"

/* ============================================================
 * Web Server
 * ============================================================ */
static WebServer s_server(80);

/* ============================================================
 * 内嵌 HTML 页面 (手机端作弊控制台)
 * ============================================================ */
static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pet Cheat</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;max-width:420px;margin:0 auto;padding:12px;background:#1a1a2e;color:#eee}
h2{text-align:center;color:#e94560;margin:12px 0}
.card{background:#16213e;padding:12px;border-radius:10px;margin-bottom:12px}
.label{display:flex;justify-content:space-between;font-size:14px;margin-bottom:2px}
.bar{background:#0f3460;border-radius:4px;height:18px;margin-bottom:8px}
.bar div{height:100%;border-radius:4px;transition:width .3s}
.hp-f{background:#e94560}
.md-f{background:#ffc947}
.coins{font-size:22px;text-align:center;color:#ffc947;margin:8px 0}
.st{text-align:center;font-size:14px;color:#aaa}
.dead{color:#e94560;font-weight:bold}
.env{font-size:13px;color:#8899aa;text-align:center;margin-top:6px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px}
button{padding:12px;border:none;border-radius:8px;font-size:15px;cursor:pointer;color:#fff;transition:opacity .2s}
button:active{opacity:.7}
.bc{background:#e94560}.bh{background:#0f3460}.bm{background:#c98400}
.ba{background:#533483}.br{background:#2eb872;grid-column:span 2}
.sli{margin:10px 0}
.sli label{font-size:13px;display:flex;justify-content:space-between}
input[type=range]{width:100%;margin:4px 0;accent-color:#e94560}
</style>
</head>
<body>
<h2>Pet Cheat Console</h2>
<div class="card">
 <div class="label"><span>HP</span><span id="hv">0</span></div>
 <div class="bar"><div class="hp-f" id="hb" style="width:0%"></div></div>
 <div class="label"><span>Mood</span><span id="mv">0</span></div>
 <div class="bar"><div class="md-f" id="mb" style="width:0%"></div></div>
 <div class="coins">Coins: <span id="co">0</span></div>
 <div class="st" id="st"></div>
 <div class="env" id="ev"></div>
</div>
<div class="grid">
 <button class="bc" onclick="C('coin500')">+500 Coins</button>
 <button class="bc" onclick="C('coin1k')">+1000 Coins</button>
 <button class="bh" onclick="C('hp_max')">HP Max</button>
 <button class="bm" onclick="C('mood_max')">Mood Max</button>
 <button class="ba" onclick="C('all_max')">All Max</button>
 <button class="bc" onclick="C('reset_coin')">Reset Coins</button>
 <button class="br" onclick="C('revive')">Revive Pet</button>
</div>
<div class="card">
 <div class="sli">
  <label><span>HP</span><span id="shv">50</span></label>
  <input type="range" id="sh" min="0" max="100" value="50"
   oninput="document.getElementById('shv').textContent=this.value"
   onchange="C('set_hp?v='+this.value)">
 </div>
 <div class="sli">
  <label><span>Mood</span><span id="smv">50</span></label>
  <input type="range" id="sm" min="0" max="100" value="50"
   oninput="document.getElementById('smv').textContent=this.value"
   onchange="C('set_mood?v='+this.value)">
 </div>
</div>
<script>
function C(c){fetch('/api/cheat/'+c).then(r=>r.json()).then(U)}
function R(){fetch('/api/status').then(r=>r.json()).then(U)}
function U(d){
 document.getElementById('hv').textContent=d.hp;
 document.getElementById('hb').style.width=d.hp+'%';
 document.getElementById('mv').textContent=d.mood;
 document.getElementById('mb').style.width=d.mood+'%';
 document.getElementById('co').textContent=d.coins;
 document.getElementById('sh').value=d.hp;
 document.getElementById('shv').textContent=d.hp;
 document.getElementById('sm').value=d.mood;
 document.getElementById('smv').textContent=d.mood;
 var s=document.getElementById('st');
 s.textContent=d.alive?'Alive':'DEAD';
 s.className=d.alive?'st':'st dead';
 document.getElementById('ev').textContent=
  'T:'+d.temp+'°C  H:'+d.hum+'%  L:'+d.lux+'lx';
}
setInterval(R,2000);R();
</script>
</body>
</html>
)rawliteral";

/* ============================================================
 * 工具: 发送 JSON 状态
 * ============================================================ */
static void send_status(void)
{
    pet_status_t *st  = pet_get_status();
    env_data_t   *env = pet_get_env();

    char json[160];
    snprintf(json, sizeof(json),
             "{\"hp\":%d,\"mood\":%d,\"coins\":%lu,"
             "\"alive\":%s,\"temp\":%d,\"hum\":%d,\"lux\":%u}",
             st->hp, st->mood, (unsigned long)st->coins,
             st->alive ? "true" : "false",
             env->temp, env->humidity, env->lux);

    s_server.send(200, "application/json", json);
}

/* ============================================================
 * 工具: 作弊后存档 + 返回状态
 * ============================================================ */
static void cheat_and_reply(void)
{
    save_write();
    send_status();
    Serial.println("[Web] Cheat applied, saved");
}

/* ============================================================
 * HTTP 路由处理
 * ============================================================ */

static void handle_root(void)
{
    s_server.send_P(200, "text/html", HTML_PAGE);
}

static void handle_status(void)
{
    send_status();
}

static void handle_coin500(void)
{
    pet_cheat_add_coins(500);
    cheat_and_reply();
}

static void handle_coin1k(void)
{
    pet_cheat_add_coins(1000);
    cheat_and_reply();
}

static void handle_hp_max(void)
{
    pet_cheat_set_hp(HP_MAX);
    cheat_and_reply();
}

static void handle_mood_max(void)
{
    pet_cheat_set_mood(MOOD_MAX);
    cheat_and_reply();
}

static void handle_all_max(void)
{
    pet_cheat_max_all();
    cheat_and_reply();
}

static void handle_reset_coin(void)
{
    pet_cheat_set_coins(0);
    cheat_and_reply();
}

static void handle_revive(void)
{
    pet_cheat_revive();
    cheat_and_reply();
}

static void handle_set_hp(void)
{
    int val = s_server.arg("v").toInt();
    pet_cheat_set_hp((int16_t)val);
    cheat_and_reply();
}

static void handle_set_mood(void)
{
    int val = s_server.arg("v").toInt();
    pet_cheat_set_mood((int16_t)val);
    cheat_and_reply();
}

/* ============================================================
 * Public API
 * ============================================================ */

extern "C" void web_init(void)
{
    Serial.printf("[WiFi] Connecting to %s ...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: %s\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Connection failed, web cheat unavailable");
    }

    /* 注册路由 */
    s_server.on("/",                   handle_root);
    s_server.on("/api/status",         handle_status);
    s_server.on("/api/cheat/coin500",  handle_coin500);
    s_server.on("/api/cheat/coin1k",   handle_coin1k);
    s_server.on("/api/cheat/hp_max",   handle_hp_max);
    s_server.on("/api/cheat/mood_max", handle_mood_max);
    s_server.on("/api/cheat/all_max",  handle_all_max);
    s_server.on("/api/cheat/reset_coin", handle_reset_coin);
    s_server.on("/api/cheat/revive",   handle_revive);
    s_server.on("/api/cheat/set_hp",   handle_set_hp);
    s_server.on("/api/cheat/set_mood", handle_set_mood);

    s_server.begin();
    Serial.println("[Web] Server started on port 80");
}

extern "C" void web_run(void)
{
    /* WiFi 断线自动重连 */
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
    }

    s_server.handleClient();
}

extern "C" bool web_ntp_sync(void)
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[NTP] WiFi not connected");
        return false;
    }

    configTime(NTP_GMT_OFFSET, NTP_DST_OFFSET, NTP_SERVER1, NTP_SERVER2);
    delay(2000);

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5000)) {
        Serial.println("[NTP] Failed to get time");
        return false;
    }

    Serial.printf("[NTP] Got: %d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    ds1307_time_t t;
    t.year  = (uint8_t)((timeinfo.tm_year + 1900) - 2000);
    t.month = (uint8_t)(timeinfo.tm_mon + 1);
    t.date  = (uint8_t)timeinfo.tm_mday;
    t.hour  = (uint8_t)timeinfo.tm_hour;
    t.min   = (uint8_t)timeinfo.tm_min;
    t.sec   = (uint8_t)timeinfo.tm_sec;
    ds1307_set_time(&t);

    return true;
}
