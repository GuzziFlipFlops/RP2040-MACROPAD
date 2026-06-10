#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define WIFI_AP_SSID "ESP32-Robot-Arm"
#define WIFI_AP_CHANNEL 6
#define WIFI_AP_MAX_CONNECTIONS 4

#define SERVO_COUNT 7
#define ARM_SERVO_COUNT 5
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES LEDC_TIMER_16_BIT
#define LEDC_FREQUENCY_HZ 50
#define SERVO_PERIOD_US 20000
#define SERVO_MIN_US 500
#define SERVO_CENTER_US 1500
#define SERVO_MAX_US 2500

typedef enum {
    SERVO_LEFT_DRIVE = 0,
    SERVO_RIGHT_DRIVE,
    SERVO_ARM_BASE_MG996,
    SERVO_ARM_SHOULDER_SG90,
    SERVO_ARM_ELBOW_SG90,
    SERVO_ARM_WRIST_SG90,
    SERVO_ARM_CLAW_SG90,
} servo_id_t;

typedef struct {
    const char *name;
    gpio_num_t gpio;
    ledc_channel_t channel;
    bool continuous;
    int current_us;
} servo_t;

/*
 * GPIO map for a common ESP32 DevKit / ESP32-WROOM-32:
 * - GPIO 18: MG996 left drivetrain continuous-rotation servo
 * - GPIO 19: MG996 right drivetrain continuous-rotation servo
 * - GPIO 21: MG996 arm base/large joint servo
 * - GPIO 22: SG90 arm shoulder servo
 * - GPIO 23: SG90 arm elbow servo
 * - GPIO 25: SG90 arm wrist servo
 * - GPIO 26: SG90 arm claw/gripper servo
 *
 * Avoid using ESP32 strapping pins for servo outputs. Power servos from a
 * separate 5-6 V supply with common ground to the ESP32.
 */
static servo_t s_servos[SERVO_COUNT] = {
    [SERVO_LEFT_DRIVE] = {"left_drive_mg996", GPIO_NUM_18, LEDC_CHANNEL_0, true, SERVO_CENTER_US},
    [SERVO_RIGHT_DRIVE] = {"right_drive_mg996", GPIO_NUM_19, LEDC_CHANNEL_1, true, SERVO_CENTER_US},
    [SERVO_ARM_BASE_MG996] = {"arm_base_mg996", GPIO_NUM_21, LEDC_CHANNEL_2, false, SERVO_CENTER_US},
    [SERVO_ARM_SHOULDER_SG90] = {"arm_shoulder_sg90", GPIO_NUM_22, LEDC_CHANNEL_3, false, SERVO_CENTER_US},
    [SERVO_ARM_ELBOW_SG90] = {"arm_elbow_sg90", GPIO_NUM_23, LEDC_CHANNEL_4, false, SERVO_CENTER_US},
    [SERVO_ARM_WRIST_SG90] = {"arm_wrist_sg90", GPIO_NUM_25, LEDC_CHANNEL_5, false, SERVO_CENTER_US},
    [SERVO_ARM_CLAW_SG90] = {"arm_claw_sg90", GPIO_NUM_26, LEDC_CHANNEL_6, false, SERVO_CENTER_US},
};

static const int s_arm_servo_ids[ARM_SERVO_COUNT] = {
    SERVO_ARM_BASE_MG996,
    SERVO_ARM_SHOULDER_SG90,
    SERVO_ARM_ELBOW_SG90,
    SERVO_ARM_WRIST_SG90,
    SERVO_ARM_CLAW_SG90,
};

static const char *TAG = "robot_arm";

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int angle_to_us(int angle)
{
    angle = clamp_int(angle, 0, 180);
    return SERVO_MIN_US + ((SERVO_MAX_US - SERVO_MIN_US) * angle) / 180;
}

static uint32_t pulse_us_to_duty(int pulse_us)
{
    pulse_us = clamp_int(pulse_us, SERVO_MIN_US, SERVO_MAX_US);
    const uint32_t max_duty = (1UL << LEDC_DUTY_RES) - 1;
    return (uint32_t)(((uint64_t)pulse_us * max_duty) / SERVO_PERIOD_US);
}

static esp_err_t servo_write_us(servo_id_t id, int pulse_us)
{
    if (id < 0 || id >= SERVO_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    pulse_us = clamp_int(pulse_us, SERVO_MIN_US, SERVO_MAX_US);
    s_servos[id].current_us = pulse_us;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_MODE, s_servos[id].channel, pulse_us_to_duty(pulse_us)), TAG, "set duty");
    return ledc_update_duty(LEDC_MODE, s_servos[id].channel);
}

static esp_err_t servo_write_angle(servo_id_t id, int angle)
{
    return servo_write_us(id, angle_to_us(angle));
}

static int speed_to_pulse_us(int speed)
{
    speed = clamp_int(speed, -100, 100);
    return SERVO_CENTER_US + (speed * 5);
}

static void stop_drivetrain(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(servo_write_us(SERVO_LEFT_DRIVE, SERVO_CENTER_US));
    ESP_ERROR_CHECK_WITHOUT_ABORT(servo_write_us(SERVO_RIGHT_DRIVE, SERVO_CENTER_US));
}

static void set_drive(int x, int y)
{
    x = clamp_int(x, -100, 100);
    y = clamp_int(y, -100, 100);

    int left = clamp_int(y + x, -100, 100);
    int right = clamp_int(y - x, -100, 100);

    /*
     * Right drivetrain servos are commonly mounted mirrored from left servos.
     * If your robot spins when it should drive straight, remove this minus.
     */
    ESP_ERROR_CHECK_WITHOUT_ABORT(servo_write_us(SERVO_LEFT_DRIVE, speed_to_pulse_us(left)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(servo_write_us(SERVO_RIGHT_DRIVE, speed_to_pulse_us(-right)));
}

static void nudge_arm_servo(int arm_index, int amount)
{
    if (arm_index < 0 || arm_index >= ARM_SERVO_COUNT) {
        return;
    }

    servo_id_t id = (servo_id_t)s_arm_servo_ids[arm_index];
    int pulse = s_servos[id].current_us + amount;
    ESP_ERROR_CHECK_WITHOUT_ABORT(servo_write_us(id, pulse));
}

static esp_err_t init_servos(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer");

    for (int i = 0; i < SERVO_COUNT; i++) {
        ledc_channel_config_t channel = {
            .gpio_num = s_servos[i].gpio,
            .speed_mode = LEDC_MODE,
            .channel = s_servos[i].channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER,
            .duty = pulse_us_to_duty(SERVO_CENTER_US),
            .hpoint = 0,
        };
        ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "channel");
        ESP_RETURN_ON_ERROR(servo_write_us((servo_id_t)i, SERVO_CENTER_US), TAG, "center");
    }

    return ESP_OK;
}

static void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started. SSID: %s, URL: http://192.168.4.1", WIFI_AP_SSID);
}

static int get_query_int(httpd_req_t *req, const char *key, int fallback)
{
    char query[160] = {0};
    char value[24] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return fallback;
    }

    if (httpd_query_key_value(query, key, value, sizeof(value)) != ESP_OK) {
        return fallback;
    }

    return atoi(value);
}

static esp_err_t send_text(httpd_req_t *req, const char *text)
{
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, text);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    static const char html[] =
"<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no\">"
"<title>ESP32 Robot Arm</title><style>"
":root{font-family:Arial,Helvetica,sans-serif;color:#172026;background:#f5f7f3;--ink:#172026;--muted:#5d686e;--line:#cdd5cc;--green:#2f7d57;--blue:#245c9c;--red:#ad3d3d;--panel:#ffffff}"
"*{box-sizing:border-box}body{margin:0;min-height:100vh}.top{position:sticky;top:0;z-index:5;background:#172026;color:#fff;padding:12px 16px;display:flex;justify-content:space-between;gap:12px;align-items:center}.top h1{font-size:18px;margin:0;font-weight:700}.top span{font-size:13px;color:#d5ddd6}"
"main{display:grid;grid-template-columns:1.05fr .95fr;gap:14px;padding:14px;max-width:1180px;margin:0 auto}.band{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px}.wide{grid-column:1/-1}h2{font-size:17px;margin:0 0 10px}h3{font-size:14px;margin:12px 0 8px;color:#243139}.hint{color:var(--muted);font-size:13px;margin:4px 0 12px}.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}.split{display:grid;grid-template-columns:1fr 1fr;gap:14px}"
".joywrap{display:flex;gap:14px;align-items:center;flex-wrap:wrap}.joy{width:min(42vw,250px);height:min(42vw,250px);max-width:250px;max-height:250px;min-width:190px;min-height:190px;border:2px solid #819087;background:radial-gradient(circle,#fff 0,#eef3ee 70%);border-radius:50%;position:relative;touch-action:none;box-shadow:inset 0 0 20px rgba(0,0,0,.08)}.knob{width:74px;height:74px;border-radius:50%;background:#2f7d57;position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);box-shadow:0 8px 18px rgba(0,0,0,.24)}#armJoy .knob{background:#245c9c}.readout{font:700 16px monospace;color:#1f2c32;min-width:150px}"
"button,select,input{font:inherit}button{border:0;border-radius:6px;background:#2f7d57;color:#fff;padding:9px 12px;font-weight:700;cursor:pointer}button.secondary{background:#245c9c}button.danger{background:#ad3d3d}button.ghost{background:#e8eee8;color:#172026;border:1px solid var(--line)}button:active{transform:translateY(1px)}select,input[type=number]{border:1px solid #aeb8af;border-radius:6px;padding:8px;background:#fff;min-height:38px}label{font-size:13px;color:#334047;font-weight:700;display:flex;flex-direction:column;gap:4px}.slider{display:grid;grid-template-columns:145px 1fr 82px;align-items:center;gap:10px;margin:9px 0}.slider input[type=range]{width:100%}.slider output{font:700 13px monospace;color:#253037}"
".blocks{display:grid;grid-template-columns:260px 1fr;gap:14px}.palette,.workspace{min-height:230px}.workspace{background:#f9fbf8;border:1px dashed #9eaaa1;border-radius:8px;padding:10px}.block{background:#fff;border:1px solid #c4cec5;border-left:6px solid #2f7d57;border-radius:8px;padding:10px;margin-bottom:10px}.block.servo{border-left-color:#245c9c}.blockHead{display:flex;justify-content:space-between;gap:8px;align-items:center;font-weight:700;margin-bottom:8px}.blockGrid{display:grid;grid-template-columns:repeat(4,minmax(110px,1fr));gap:8px}.blockGrid label{min-width:0}.status{min-height:22px;color:#253037;font-size:13px}.active{outline:3px solid #e4b34a}"
"@media(max-width:800px){main,.split,.blocks{grid-template-columns:1fr}.wide{grid-column:auto}.slider{grid-template-columns:1fr}.joy{width:74vw;height:74vw}.top{align-items:flex-start;flex-direction:column}}"
"</style></head><body><header class=\"top\"><h1>ESP32 Robot Arm</h1><span>Join Wi-Fi: ESP32-Robot-Arm · Open 192.168.4.1</span></header><main>"
"<section class=\"band\"><h2>Robot Drive</h2><p class=\"hint\">Left joystick drives the two MG996 drivetrain servos.</p><div class=\"joywrap\"><div class=\"joy\" id=\"driveJoy\"><div class=\"knob\"></div></div><div><div class=\"readout\" id=\"driveRead\">X 0 · Y 0</div><button class=\"danger\" onclick=\"stopDrive()\">Stop</button></div></div></section>"
"<section class=\"band\"><h2>Arm Joystick</h2><p class=\"hint\">Pick one arm output, then use the joystick up/down for forward/backward PWM nudges.</p><div class=\"row\"><label>Arm output<select id=\"armSelect\"><option value=\"0\">MG996 base</option><option value=\"1\">SG90 shoulder</option><option value=\"2\">SG90 elbow</option><option value=\"3\">SG90 wrist</option><option value=\"4\">SG90 claw</option></select></label></div><div class=\"joywrap\"><div class=\"joy\" id=\"armJoy\"><div class=\"knob\"></div></div><div class=\"readout\" id=\"armRead\">PWM nudge 0</div></div></section>"
"<section class=\"band wide\"><h2>Arm Outputs</h2><p class=\"hint\">Use angles or direct PWM pulse widths for each arm servo output.</p><div id=\"sliders\"></div></section>"
"<section class=\"band wide\"><h2>Program Your Robot</h2><p class=\"hint\">Add blocks, erase the coding space, then play the program. Blocks run one at a time.</p><div class=\"blocks\"><div class=\"palette\"><h3>Add Blocks</h3><div class=\"row\"><button onclick=\"addDriveBlock()\">Drive block</button><button class=\"secondary\" onclick=\"addServoBlock()\">Servo block</button><button class=\"danger\" onclick=\"clearBlocks()\">Erase</button></div><h3>Play</h3><div class=\"row\"><button onclick=\"runProgram()\">Go</button><button class=\"danger\" onclick=\"cancelProgram()\">Stop</button></div><p class=\"status\" id=\"programStatus\"></p></div><div class=\"workspace\" id=\"workspace\"></div></div></section>"
"</main><script>"
"const servoNames=['MG996 base','SG90 shoulder','SG90 elbow','SG90 wrist','SG90 claw'];let programRunning=false;let activeBlock=null;"
"function api(path){return fetch(path,{cache:'no-store'}).catch(()=>{});}function clamp(v,min,max){return Math.max(min,Math.min(max,v));}"
"function makeJoy(el,onMove,onEnd){const knob=el.querySelector('.knob');let dragging=false;function set(clientX,clientY){const r=el.getBoundingClientRect();const cx=r.left+r.width/2,cy=r.top+r.height/2;let dx=clientX-cx,dy=clientY-cy;const max=(r.width-74)/2;const d=Math.hypot(dx,dy);if(d>max){dx=dx/d*max;dy=dy/d*max;}knob.style.left=(50+dx/r.width*100)+'%';knob.style.top=(50+dy/r.height*100)+'%';onMove(Math.round(dx/max*100),Math.round(-dy/max*100));}function reset(){dragging=false;knob.style.left='50%';knob.style.top='50%';onEnd&&onEnd();}el.addEventListener('pointerdown',e=>{dragging=true;el.setPointerCapture(e.pointerId);set(e.clientX,e.clientY);});el.addEventListener('pointermove',e=>{if(dragging)set(e.clientX,e.clientY);});el.addEventListener('pointerup',reset);el.addEventListener('pointercancel',reset);}"
"let driveTimer=0;makeJoy(document.getElementById('driveJoy'),(x,y)=>{document.getElementById('driveRead').textContent=`X ${x} · Y ${y}`;clearTimeout(driveTimer);driveTimer=setTimeout(()=>api(`/drive?x=${x}&y=${y}`),35);},()=>stopDrive());function stopDrive(){document.getElementById('driveRead').textContent='X 0 · Y 0';api('/drive?x=0&y=0');}"
"let armTimer=0;makeJoy(document.getElementById('armJoy'),(x,y)=>{const n=clamp(Math.round(y*4),-400,400);document.getElementById('armRead').textContent=`PWM nudge ${n} us`;clearTimeout(armTimer);armTimer=setTimeout(()=>api(`/arm/nudge?servo=${armSelect.value}&amount=${n}`),80);},()=>{document.getElementById('armRead').textContent='PWM nudge 0';});"
"function buildSliders(){const root=document.getElementById('sliders');servoNames.forEach((name,i)=>{const row=document.createElement('div');row.className='slider';row.innerHTML=`<label>${name}</label><input type=\"range\" min=\"0\" max=\"180\" value=\"90\" id=\"angle${i}\"><output id=\"out${i}\">90° · 1500 us</output><label>PWM us<input type=\"number\" min=\"500\" max=\"2500\" value=\"1500\" id=\"pwm${i}\"></label>`;root.appendChild(row);const angle=row.querySelector(`#angle${i}`),pwm=row.querySelector(`#pwm${i}`),out=row.querySelector(`#out${i}`);function setOut(a,us){out.textContent=`${a}° · ${us} us`;}angle.addEventListener('input',()=>{const a=+angle.value,us=Math.round(500+(2000*a/180));pwm.value=us;setOut(a,us);api(`/arm?servo=${i}&angle=${a}`);});pwm.addEventListener('change',()=>{const us=clamp(+pwm.value,500,2500);pwm.value=us;const a=Math.round((us-500)*180/2000);angle.value=a;setOut(a,us);api(`/arm?servo=${i}&us=${us}`);});});}buildSliders();"
"function addDriveBlock(){const b=document.createElement('div');b.className='block drive';b.innerHTML=`<div class=\"blockHead\"><span>Drive drivetrain motor</span><button class=\"ghost\" onclick=\"this.closest('.block').remove()\">Remove</button></div><div class=\"blockGrid\"><label>Motor<select class=\"motor\"><option value=\"both\">both</option><option value=\"left\">left MG996</option><option value=\"right\">right MG996</option></select></label><label>Direction<select class=\"direction\"><option value=\"forward\">forward</option><option value=\"backward\">backward</option><option value=\"stop\">stop</option></select></label><label>Speed<input class=\"speed\" type=\"number\" min=\"0\" max=\"100\" value=\"70\"></label><label>Time ms<input class=\"time\" type=\"number\" min=\"100\" max=\"30000\" value=\"1000\"></label></div>`;workspace.appendChild(b);}"
"function addServoBlock(){const b=document.createElement('div');b.className='block servo';b.innerHTML=`<div class=\"blockHead\"><span>Move one arm servo</span><button class=\"ghost\" onclick=\"this.closest('.block').remove()\">Remove</button></div><div class=\"blockGrid\"><label>Servo<select class=\"servo\">${servoNames.map((n,i)=>`<option value=\"${i}\">${n}</option>`).join('')}</select></label><label>Angle<input class=\"angle\" type=\"number\" min=\"0\" max=\"180\" value=\"90\"></label><label>Pause ms<input class=\"time\" type=\"number\" min=\"100\" max=\"30000\" value=\"600\"></label></div>`;workspace.appendChild(b);}"
"function clearBlocks(){cancelProgram();workspace.innerHTML='';programStatus.textContent='Coding space erased.';}function wait(ms){return new Promise(r=>setTimeout(r,ms));}"
"async function runProgram(){if(programRunning)return;programRunning=true;programStatus.textContent='Running program...';const blocks=[...workspace.children];for(const b of blocks){if(!programRunning)break;activeBlock&&activeBlock.classList.remove('active');activeBlock=b;b.classList.add('active');if(b.classList.contains('drive')){const motor=b.querySelector('.motor').value,dir=b.querySelector('.direction').value,s=clamp(+b.querySelector('.speed').value,0,100),t=clamp(+b.querySelector('.time').value,100,30000);let left=0,right=0;if(dir==='forward'){left=s;right=s;}if(dir==='backward'){left=-s;right=-s;}if(motor==='left')right=0;if(motor==='right')left=0;await api(`/motor?left=${left}&right=${right}`);await wait(t);await api('/drive?x=0&y=0');}else{const servo=+b.querySelector('.servo').value,angle=clamp(+b.querySelector('.angle').value,0,180),t=clamp(+b.querySelector('.time').value,100,30000);await api(`/arm?servo=${servo}&angle=${angle}`);await wait(t);}}activeBlock&&activeBlock.classList.remove('active');activeBlock=null;programRunning=false;stopDrive();programStatus.textContent='Program finished.';}"
"function cancelProgram(){programRunning=false;activeBlock&&activeBlock.classList.remove('active');activeBlock=null;stopDrive();programStatus.textContent='Program stopped.';}"
"addDriveBlock();addServoBlock();"
"</script></body></html>";

    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t drive_get_handler(httpd_req_t *req)
{
    int x = get_query_int(req, "x", 0);
    int y = get_query_int(req, "y", 0);
    set_drive(x, y);
    return send_text(req, "ok\n");
}

static esp_err_t motor_get_handler(httpd_req_t *req)
{
    int left = clamp_int(get_query_int(req, "left", 0), -100, 100);
    int right = clamp_int(get_query_int(req, "right", 0), -100, 100);
    ESP_ERROR_CHECK_WITHOUT_ABORT(servo_write_us(SERVO_LEFT_DRIVE, speed_to_pulse_us(left)));
    ESP_ERROR_CHECK_WITHOUT_ABORT(servo_write_us(SERVO_RIGHT_DRIVE, speed_to_pulse_us(-right)));
    return send_text(req, "ok\n");
}

static esp_err_t arm_get_handler(httpd_req_t *req)
{
    int arm_index = get_query_int(req, "servo", 0);
    if (arm_index < 0 || arm_index >= ARM_SERVO_COUNT) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad servo");
        return ESP_FAIL;
    }

    char query[160] = {0};
    servo_id_t id = (servo_id_t)s_arm_servo_ids[arm_index];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK && strstr(query, "us=") != NULL) {
        servo_write_us(id, get_query_int(req, "us", SERVO_CENTER_US));
    } else {
        servo_write_angle(id, get_query_int(req, "angle", 90));
    }

    return send_text(req, "ok\n");
}

static esp_err_t arm_nudge_get_handler(httpd_req_t *req)
{
    int arm_index = get_query_int(req, "servo", 0);
    int amount = clamp_int(get_query_int(req, "amount", 0), -400, 400);
    nudge_arm_servo(arm_index, amount);
    return send_text(req, "ok\n");
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;

    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_get_handler},
        {.uri = "/drive", .method = HTTP_GET, .handler = drive_get_handler},
        {.uri = "/motor", .method = HTTP_GET, .handler = motor_get_handler},
        {.uri = "/arm", .method = HTTP_GET, .handler = arm_get_handler},
        {.uri = "/arm/nudge", .method = HTTP_GET, .handler = arm_nudge_get_handler},
    };

    for (int i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &routes[i]));
    }

    return server;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(init_servos());
    stop_drivetrain();
    wifi_init_softap();
    start_webserver();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
