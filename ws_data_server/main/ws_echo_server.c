/* WebSocket Echo Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"


#include <esp_http_server.h>


#include "esp_check.h"

#include "driver/gpio.h"


#define RED_LED_GPIO   23
#define GREEN_LED_GPIO 22



#define UNKNOWN_COMMAND -1
#define START_TRANSFER   1
#define STOP_TRANSFER    2



/* A simple example that demonstrates using websocket echo server
 */
static const char *TAG = "ws_data_server";

const static char http_index_hml[] =
"<!DOCTYPE html><html lang=\"en\"><head>    <meta charset=\"UTF-8\">    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">    <title>ESP32 Web Socket</title>    <link rel=\"stylesheet\" href=\"index.css\">    </head><body>    <div class=\"wrapper\">        <h1 class=\"page-title\">ESP32 Web Socket Test</h1>        <div class=\"line-of-items\">            <div class=\"control-box medium-box\">                <div class=\"control-box-header\">Transfer Control</div>                <div class=\"line-of-items\">                    <div  class=\"col\">Status:</div>                        <div id=\"connection-status-label\"  class=\"col\"></div>                        <div>                        <input type=\"checkbox\" id=\"switch\" class=\"checkbox\"  />                        <label for=\"switch\" class=\"toggle\">                        </label>                    </div>                </div>                                <div class=\"line-of-items\"><span>Async Transfer</span></div>                <div class=\"line-of-items\">                    <div  class=\"col\">Size:</div>                        <input type=\"text\" class=\"control-input\" id=\"payload-size-input\" >                </div>                <div class=\"line-of-items\">                    <div  class=\"col\">Count:</div>                        <input type=\"text\" class=\"control-input\" id=\"frame-count-input\" >                </div>                                <div class=\"line-of-items\">                    <button id=\"start-update-button\" class=\"in-the-middle\" disabled>Start</button>                    <button id=\"stop-update-button\" class=\"in-the-middle\" disabled>Stop</button>                </div>                </div>            <div class=\"control-box large-box\">                <div class=\"control-box-header\">Frame Dispaly Control</div>                <div class=\"line-of-items\"><span>Async Transfer</span></div>                <div class=\"canvas-section medium-canvas\">                    <canvas id=\"graph-canvas\" width=\"350\" height=\"200\" >Your browser does not support the canvas element.</canvas>                </div>                <div>                    <button  id=\"test1-button\">Testing!</button>                    <button  id=\"clear-frame-canvas-button\">Clear</button>                </div>                                </div>        </div>        <div class=\"line-of-items\">            <div class=\"control-box very-large-box\">                <div class=\"control-box-header\">Frames Reception</div>                <div class=\"line-of-items\"><span>Async Transfer</span></div>                <div class=\"canvas-section large-canvas\">                    <canvas id=\"graph-canvas\" width=\"550\" height=\"200\" >Your browser does not support the canvas element.</canvas>                </div>               <div>                   <button  id=\"test2-button\">Testing!</button>                   <button  id=\"clear-signal-canvas-button\">Clear</button>              </div>            </div>        </div>        </div>       <script src=\"index.js\"></script>      </body></html>";


const static char http_css_file[] =
"html {    height: 100%;}body {    height: 100%;    background-image: linear-gradient(#4D1496, #9567bd);    background-repeat: no-repeat;    color: white;}.wrapper {    width: 80%;    margin: auto;}.page-title {    text-align: center;}.control-box {    display: flex;    flex-direction: column;    padding-bottom: 15px;    background-color: #50169c;    border: 1px solid #7D52B7;    border-radius: 0px 0px 10px 10px;}.control-box-header {    background-color: #DBD5F1;    color: #5C369F;    border-bottom: 1px solid #7D52B7;    text-align: center;}.medium-box {    width: 250px;}.large-box {    width: 450px;}.very-large-box {    width: 700px;}.line-of-items {    display: flex;    flex-direction: row;    justify-content: space-evenly;    margin: auto;    margin-bottom: 5px;    text-align: center;}.line-of-items .col {    width: 70px;}.in-the-middle {    justify-content: center;}.control-input {    width: 65px;    background-color: #7D52B7;    color: white;}.toggle {    position : relative ;    display : inline-block;    width : 50px;    height : 20px;    background-color: #DBD5F1;    border-radius: 30px;    border: 2px solid gray;}       .toggle:after {    content: '';    position: absolute;    width: 25px;    height: 18px;    border-radius: 50%;    background-color: gray;    top: 1px;     left: 1px;    transition:  all 0.25s;} .checkbox:checked + .toggle::after {    left : 25px; }       .checkbox:checked + .toggle {    background-color: green;}       .checkbox {     display : none;}.canvas-section {      height: 200px;    margin:  auto;}.medium-canvas {    width: 350px;}.large-canvas {    width: 550px;}#graph-canvas {    margin:  auto;    background-color: #7b49af;    border:1px solid #c3c3c3;}";


const static char http_js_file[] =
"const MAX_PAYLOAD_SIZE = 100000;const MAX_FRAME_COUNT  = 10;const GRAPH_WIDTH   = 350;const GRAPH_HEIGHT  = 200;const GRAPH_BORDER_MARGIN        = 20;const GRAPH_Y_AXIS_OFFSET    = (4*GRAPH_HEIGHT)/5 + GRAPH_BORDER_MARGIN;const GRAPH_X_AXIS_START_POS     = GRAPH_BORDER_MARGIN; const GRAPH_X_AXIS_END_POS       = GRAPH_WIDTH  - GRAPH_BORDER_MARGIN;const GRAPH_Y_AXIS_START_POS     = GRAPH_BORDER_MARGIN; const GRAPH_Y_AXIS_END_POS       = GRAPH_HEIGHT  - GRAPH_BORDER_MARGIN;const GRAPH_X_SPACER             = 10;const DRAWABLE_WIDTH = GRAPH_X_AXIS_END_POS - GRAPH_X_AXIS_START_POS;const DRAWABLE_HEIGHT = GRAPH_Y_AXIS_OFFSET - GRAPH_BORDER_MARGIN;const wsUri = \"ws://10.0.0.250/ws\";var websocket;var activeUpdate = false;let dataPoints = [];var canvas = document.getElementById(\"graph-canvas\");var ctx = canvas.getContext(\"2d\");var transferActive = false;var framesReceived = [];var drawSolution = {    revalidated: false,     nbrBars: 0,    barsWidth: 0,    barsMaxHeigt: 0,    barsValueToHeightRatio: 0};    const displayConnectionStatus = (state) => {        const connectStatus = document.querySelector(\"#connection-status-label\");    connectStatus.innerHTML = state;};const writeToScreen = (message) => {        console.log('Write Message ' + message);};function initWebSocket() {    websocket = new WebSocket(wsUri);        websocket.binaryType = \"arraybuffer\";            websocket.onopen = function(evt) {        console.log('CONNECTED Event');        displayConnectionStatus('Connected');    };    websocket.onmessage  = function(evt) {                const now = Math.floor(performance.now() * 10);        console.log('onmessage Event: ' + now);        if (evt.data instanceof ArrayBuffer) {            const dataArray = evt.data;            console.log(\"ArrayBuffer received: lenght is \" + dataArray.byteLength);            framesReceived.push({\"time\":now , \"size\":dataArray.byteLength});        } else          console.log(\"No info on the type of data received!\");                          };                    websocket.onclose = function(evt) {        console.log('DISCONNECTED Event');       displayConnectionStatus('Connected');    };            websocket.onerror = function(evt) {           console.log('onerror Event');          displayConnectionStatus('Error');    };}const setStateUpdateButtons = (startButton,StopButton) => {    document.getElementById(\"start-update-button\").disabled = !startButton;;    document.getElementById(\"stop-update-button\").disabled = !StopButton;   };const toggleUpdateButtons = (running) => {    document.getElementById(\"start-update-button\").disabled = running;    document.getElementById(\"stop-update-button\").disabled = !running;   };const startTransferButtonClickHandler  = () => {    console.log(\"Start transfer Handler\");    activeUpdate = true;    const size = document.getElementById(\"payload-size-input\").value;    const frameCount = document.getElementById(\"frame-count-input\").value;          console.log(\"Value of payload size is \" + size);   console.log(\"Value of frame count is \" + frameCount);       if (size <= 0 || size > MAX_PAYLOAD_SIZE) {        alert('Enter a greater than 0 and less that 100K size of the payload to be returned');        return;   }   if (frameCount <= 0 || frameCount > MAX_FRAME_COUNT) {        alert('Enter a greater than 0 and less that 10 for the number of frames to be returned');        return;   }   websocket.send('start:' + size + \":\" + frameCount);   toggleUpdateButtons(activeUpdate);    };const stopTransferButtonClickHandler  = () => {    console.log(\"Stop transfer Handler\");    activeUpdate = false;    websocket.send('stop');    toggleUpdateButtons(activeUpdate);}; const updateValueHandler  = () => {    console.log(\"Handler of update of value: Request to \" + url);    if (activeUpdate)        setTimeout(updateValueHandler, 300);            }; const connectionSwitchHandler  = (event) => {    const connectSwitch = document.getElementById(\"switch\");    console.log('connectionSwitchHandler');    if (event.currentTarget.checked) {        console.log('checked');        initWebSocket();        setStateUpdateButtons(true,false);    } else {        console.log('not checked');        websocket.close(1000,\"Work Completed\");        setStateUpdateButtons(false,false);    } };function calculateDrawing() {    function getSizes(){        return framesReceived.map(d => d.size);      }    function getTimes(){        return framesReceived.map(d => d.time);    }    console.log(framesReceived.length + \" frames to draw\");    console.log(dataPoints.length + \" points to draw\");    drawSolution.nbrBars = framesReceived.length;    drawSolution.barsWidth = DRAWABLE_WIDTH / 10;        let maxValue = Math.max(...getSizes());    let startTime = Math.min(...getTimes());    let endTime = Math.max(...getTimes());        drawSolution.timeRange = endTime - startTime;    console.log(`The max value is ${maxValue}`);    framesReceived.forEach( (element) => {element.time = element.time - startTime; });        drawSolution.barsMaxHeigt = DRAWABLE_HEIGHT;    drawSolution.barsValueToHeightRatio = DRAWABLE_HEIGHT / maxValue;    drawSolution.revalidated = true;}function clearCanvas(ctx){    ctx.fillStyle = \"#7b49af\";    ctx.fillRect(0, 0,  GRAPH_WIDTH, GRAPH_HEIGHT);}function drawAxis(ctx){    ctx.beginPath();    ctx.moveTo(GRAPH_X_AXIS_START_POS, GRAPH_Y_AXIS_OFFSET+2);    ctx.lineTo(GRAPH_X_AXIS_END_POS, GRAPH_Y_AXIS_OFFSET+2);    ctx.lineWidth = 2;    ctx.strokeStyle = '#00ff00';    ctx.stroke();    ctx.beginPath();    ctx.moveTo(GRAPH_X_AXIS_START_POS-2, GRAPH_Y_AXIS_START_POS);    ctx.lineTo(GRAPH_X_AXIS_START_POS-2, GRAPH_Y_AXIS_END_POS);    ctx.strokeStyle = '#00ff00';    ctx.stroke();    ctx.font = \"14px Arial\";    ctx.fillStyle = \"white\";       let  x = GRAPH_X_AXIS_START_POS - (GRAPH_BORDER_MARGIN/2) - 5;    const dy = (GRAPH_Y_AXIS_END_POS - GRAPH_Y_AXIS_START_POS) / 4;    let y = GRAPH_Y_AXIS_START_POS;    for (let i=4;i>=0;i--)    {        ctx.fillText(i,x,y);        y += dy;    }    y = GRAPH_Y_AXIS_OFFSET + (GRAPH_BORDER_MARGIN/2) + 5;    const dx = (GRAPH_X_AXIS_END_POS - GRAPH_X_AXIS_START_POS) / 4;    x = GRAPH_X_AXIS_START_POS + dx;    for (let i=1;i<5;i++)    {        ctx.fillText(i,x,y);        x += dx;     }}function drawData(ctx){    if (!drawSolution.revalidated) {        calculateDrawing();        console.log(\"The values needs to be re calculated\");    }    ctx.font = \"14px Arial\";    ctx.fillStyle = \"white\";    drawSolution.barsValueToHeightRatio = 1;       let xBar = GRAPH_X_AXIS_START_POS;            let barWidth = drawSolution.barsWidth;    const graphWidth = GRAPH_X_AXIS_END_POS - GRAPH_X_AXIS_START_POS;       console.log(`${framesReceived.length} frames to draw...`);    let prevX = GRAPH_X_AXIS_START_POS;    let overlapCount = 1;    for (let i = 0; i < framesReceived.length; i++) {        ctx.beginPath();        ctx.lineWidth = \"1\";        ctx.strokeStyle = \"#9068ff\";        const barHeight = framesReceived[i].size * drawSolution.barsValueToHeightRatio;        const yStart = GRAPH_Y_AXIS_OFFSET  - barHeight;        const xRatio = framesReceived[i].time / (drawSolution.timeRange+100);        xBar = GRAPH_X_AXIS_START_POS + (xRatio * graphWidth);        console.log(`time = ${framesReceived[i].time} ratio = ${xRatio} x = ${xBar}  Height: ${barHeight}`);        var grd = ctx.createLinearGradient(0, 0, 0, barHeight/2);        grd.addColorStop(0, \"#b068ff\");        grd.addColorStop(1, \"#f098ff\");        ctx.fillStyle = grd;        ctx.fillRect(xBar, yStart, drawSolution.barsWidth, barHeight);        ctx.rect(xBar, yStart,  drawSolution.barsWidth,  barHeight);                        ctx.stroke();        if ((xBar-prevX) < 5){            overlapCount++;            console.log(\"Bat overlap \" + overlapCount);            ctx.fillStyle = \"white\";            ctx.fillText(overlapCount,xBar + (drawSolution.barsWidth/2),yStart + barHeight/2);        } else {            overlapCount = 1;        }               prevX = xBar;    }    drawSolution.revalidated = false;}function redrawCanvas(ctx){    calculateDrawing();    clearCanvas(ctx);    drawAxis(ctx);    drawData(ctx);}function testingDrawing() {    console.log(\"testingDrawing\");    redrawCanvas(ctx);}function clearFrameCanvasHandler() {    framesReceived.length = 0;    console.log(\"clearFrameCanvasHandler: Size is now \" + framesReceived.length);    clearCanvas(ctx);    drawAxis(ctx);}function testingDrawing2() {    console.log(\"testingDrawing\");    redrawCanvas(ctx);}function clearSignalCanvasHandler() {    console.log(\"clearSignalCanvasHandler\");}  function setEventHandlers() {   const startTransferButton = document.getElementById(\"start-update-button\");   startTransferButton.addEventListener(\"click\", startTransferButtonClickHandler );   const stopTransferButton = document.getElementById(\"stop-update-button\");   stopTransferButton.addEventListener(\"click\", stopTransferButtonClickHandler );   const connectSwitch = document.getElementById(\"switch\");   connectSwitch.addEventListener(\"change\", connectionSwitchHandler );   connectSwitch.value = true;   const testButton = document.getElementById(\"test1-button\");   testButton.addEventListener(\"click\", testingDrawing );   const clearFrameButton = document.getElementById(\"clear-frame-canvas-button\");   clearFrameButton.addEventListener(\"click\", clearFrameCanvasHandler );   const testButton2 = document.getElementById(\"test2-button\");   testButton2.addEventListener(\"click\", testingDrawing2 );   const clearSignalButton = document.getElementById(\"clear-signal-canvas-button\");   clearSignalButton.addEventListener(\"click\", clearSignalCanvasHandler );         document.getElementById(\"payload-size-input\").value = 100;    document.getElementById(\"frame-count-input\").value = 1; }   setEventHandlers(); redrawCanvas(ctx);";





typedef struct transferCommand {
    int action;
    int size;
    int nbrFrames;
} transferCommand_t;



static void configure_led(void);
static void blink_green_led_task(void* arg);
static esp_err_t indexpage_get_handler(httpd_req_t *req);
static esp_err_t css_get_handler(httpd_req_t *req);
static esp_err_t js_get_handler(httpd_req_t *req);

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req);
static esp_err_t trigger_async_send_multi(httpd_req_t *req, int payloadSize, int nbrFrames);
static esp_err_t websocket_handler(httpd_req_t *req);
static httpd_handle_t start_webserver(void);
static esp_err_t stop_webserver(httpd_handle_t server);
static void transfer_to_client_led_task(void* arg);

static int parse_ws_command(char* command, transferCommand_t* tf);
static int isStartCommand(const char* command);
static int isStopCommand(const char* command);



static uint8_t s_green_led_state = 0;
static uint8_t s_red_led_state = 0;
static uint8_t s_transfer_active = 0;


 static uint8_t *adcDataBuffer = NULL; 

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    int payloadSize;
    int nbrFrames;
    uint8_t* payloadBuffer;
};




static void configure_led(void)
{
    ESP_LOGI(TAG, "Web Server configured to blink green LED on GPIO #22");

    gpio_reset_pin(RED_LED_GPIO);
    gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);

    gpio_reset_pin(GREEN_LED_GPIO);
    gpio_set_direction(GREEN_LED_GPIO, GPIO_MODE_OUTPUT);
}

static void transfer_to_client_led_task(void* arg)
{

    ESP_LOGI(TAG, "Starting transfer thread...");

    s_transfer_active = 1;

    while(1) {

        if (s_transfer_active)
        {

             s_green_led_state = !s_green_led_state;
        		            // s_red_led_state = !s_red_led_state;
        		   gpio_set_level(GREEN_LED_GPIO, s_green_led_state);
        		   gpio_set_level(RED_LED_GPIO, 0);
        	

        }
        else
        	gpio_set_level(GREEN_LED_GPIO, 0);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


/* An HTTP GET handler */
static esp_err_t indexpage_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }



      httpd_resp_set_type(req, "text/html");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t indexpage = {
    .uri       = "/index.html",
    .method    = HTTP_GET,
    .handler   = indexpage_get_handler,
    .user_ctx  =  http_index_hml    
};

static esp_err_t css_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

     ESP_LOGW(TAG, "Calling the CSS file handler");

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }



    httpd_resp_set_type(req, "text/css");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t csspage = {
    .uri       = "/index.css",
    .method    = HTTP_GET,
    .handler   = css_get_handler,
    .user_ctx  =  http_css_file    
};




static esp_err_t js_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

     ESP_LOGW(TAG, "Calling the javascript file handler");

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }



    httpd_resp_set_type(req, "application/javascript");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t jspage = {
    .uri       = "/index.js",
    .method    = HTTP_GET,
    .handler   = js_get_handler,
    .user_ctx  =  http_js_file    
};




static void ws_flex_async_send(void *arg)
{

    struct async_resp_arg *resp_arg = arg;

    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    resp_arg->payloadBuffer = calloc(1, resp_arg->payloadSize + 1);



	printf("Content of async_resp_arg: Payload len of %d\tBuffer Address %p\tFirst Block %d\n", 
            resp_arg->payloadSize,
            resp_arg->payloadBuffer,
            resp_arg->payloadBuffer[1]);

    httpd_ws_frame_t ws_pkt;


    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    //ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;


    ws_pkt.len = resp_arg->payloadSize;
    ws_pkt.payload = resp_arg->payloadBuffer;


    uint8_t value = 1;
    printf("ws_flex_async_send: payload is  %d\n",ws_pkt.len );


       for (int i=0;i<resp_arg->nbrFrames;i++)
       {
           if (s_transfer_active == 0)  break;

         memset(resp_arg->payloadBuffer, value++, resp_arg->payloadSize); 

            httpd_ws_send_frame_async(hd, fd, &ws_pkt);
            vTaskDelay(200 / portTICK_PERIOD_MS);
       }

    free(resp_arg->payloadBuffer);
    free(resp_arg);
}

static esp_err_t trigger_async_send_multi(httpd_req_t *req, int payloadSize, int nbrFrames)
{
   ESP_LOGI(TAG, "Invoked trigger_async_send_multi...");

    httpd_handle_t handle = req->handle;

    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    resp_arg->payloadSize = payloadSize;
    resp_arg->payloadBuffer = adcDataBuffer;
    resp_arg->nbrFrames = nbrFrames;


    s_transfer_active = 1;
    esp_err_t ret = httpd_queue_work(handle, ws_flex_async_send, resp_arg);
    if (ret != ESP_OK) {
         free(resp_arg);
         s_transfer_active = 0;
    
    }
      
   
    return ret;

}





static int isStartCommand(const char* command)
{
  const char startCommand[] =  "start";

  if (command == NULL)  return 0;
  
   // Case Too short: The command sent can't be 'start' as it is too short...
   if ( strlen(command) < strlen(startCommand))  return 0;

   for (int i=0;i<strlen(startCommand);i++)
   {
   	if (startCommand[i] != command[i])  return 0;
   }

   return 1;	    
}

static int isStopCommand(const char* command)
{
  const char startCommand[] =  "stop";

  if (command == NULL)  return 0;
  
   // Case Too short: The command sent can't be 'start' as it is too short...
   if ( strlen(command) < strlen(startCommand))  return 0;

   for (int i=0;i<strlen(startCommand);i++)
   {
   	if (startCommand[i] != command[i])  return 0;
   }

   return 1;	    
}




static int parse_ws_command(char* command, transferCommand_t* tf)
{

    if (tf == NULL)  return UNKNOWN_COMMAND;

    if (  isStartCommand(command)  )
    {

        ESP_LOGE(TAG, "In start...");
        tf->action = START_TRANSFER;

        char *ret = strchr(command, ':');

        if (ret == NULL)
       {
             tf->size = 1;
             tf->nbrFrames = 1;
             printf("No size of packed to return in command\n");
             return START_TRANSFER;
        }


        int converted;
        int converted2 = 0;
            
        sscanf(ret+1, "%d", &converted); // Using sscanf
        printf("Found a parameter %s: Convered is %d\n",ret+1, converted);
            
        if (converted != 0)
        {
            tf->size = converted;
        }
        else
        {
             tf->size = 1;
         }
        
        // Look further to try to find a number of frame to return...
        char *ret2 = strchr(ret+1, ':');

        if (ret2 == NULL)
        {
            printf("No frame count found\n");
            tf->nbrFrames = 1;
            return START_TRANSFER;
        }

        printf("Found a Nbr Frame command:  %s\n", ret2+1);
        sscanf(ret2+1, "%d", &converted2);
         printf("Converted (Nbr Frame) is %d\n", converted2 );
        tf->nbrFrames = converted2;
        return START_TRANSFER;
    }
    //else if (  strcmp( command ,"stop") == 0  )
    else if (  isStopCommand(command )   )
    {
        ESP_LOGE(TAG, "In stop...");
         tf->action = STOP_TRANSFER;
         return STOP_TRANSFER;
    }
    else
    {
         ESP_LOGE(TAG, "In unknown...");
        tf->action = UNKNOWN_COMMAND;
        return UNKNOWN_COMMAND;
    }
       
}




/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t websocket_handler(httpd_req_t *req)
{
    uint8_t *buf = NULL;
    // static const char * buf = "Async data";
    
    uint16_t returnFrameZise = 0;
    
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);





    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    

   // Need to understand why there is a buffer (i.e buf) required here, as it is not the data that is sent back to the HTML
   // trigger_async_send_multi and other sub method create the data and buffer to be used.. So not sure why it is there...
   // Warning: if you remove the 'buf', there is nothing returned to the client!
    if (ws_pkt.len) {
         buf = calloc(1, ws_pkt.len + 1);

          if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
         
         memset(buf, 0, ws_pkt.len + 1);
        ws_pkt.payload = buf;
    
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len + 1);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }    
    }

    
    transferCommand_t command;
    memset(&command, 0, sizeof(transferCommand_t));



    int returnVal = parse_ws_command( (char*)ws_pkt.payload , &command );

    switch (returnVal)
    {
        case START_TRANSFER:
            ESP_LOGW(TAG, "Received command start, size is %d",command.size);
            ret =  trigger_async_send_multi(req,command.size, command.nbrFrames);
            break;

        case STOP_TRANSFER:
            ESP_LOGW(TAG, "Received command stop");
            s_transfer_active = 0;
            break;

        case UNKNOWN_COMMAND:
            ESP_LOGW(TAG, "Can't say what is in the payload 2...");
            break;

    default:
        break;
    }


    //ret =  trigger_async_send_multi(req,command.size);
    free(buf);

    return ret;
}

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = websocket_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};



static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &indexpage);    // This method will serve the control web page
        httpd_register_uri_handler(server, &csspage);    // This method will serve the control web page
        httpd_register_uri_handler(server, &jspage);    // This method will serve the control web page
        httpd_register_uri_handler(server, &ws);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


static esp_err_t stop_webserver(httpd_handle_t server)
{
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void app_main(void)
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

   configure_led();

    server = start_webserver();

     xTaskCreate(transfer_to_client_led_task, "transfer_to_client_led_task", 2048, NULL, 5, NULL);


     

       while (server) {
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
