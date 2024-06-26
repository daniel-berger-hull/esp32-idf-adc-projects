
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"

#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"

#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_check.h"

#include "esp_random.h"


#include "driver/gpio.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"


#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          //Multisampling

static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_6;     //GPIO34 if ADC1, GPIO14 if ADC2
//static const adc_channel_t channel = ADC_CHANNEL_7;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;


 uint32_t current_adc_reading = 0;





const static char http_index_hml[] =
"<!DOCTYPE html><html lang=\"en\"><head>    <meta charset=\"UTF-8\">    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">    <title>Pool Monitor</title>    <style>        html {            height: 100%;        }        body {            height: 100%;            background-image: linear-gradient(#4D1496, #7D32BF);            background-repeat: no-repeat;            color: white;        }        .wrapper {            width: 40%;            margin: auto;        }        .control-box {            display: flex;            flex-direction: column;                     border: 1px solid #7D52B7;            border-radius: 0px 0px 10px 10px;        }        .control-box-header {            background-color: #DBD5F1;            color: #5C369F;            border-bottom: 1px solid #7D52B7;        }        .medium-box {            width: 250px;        }        .line-of-items {            display: flex;            margin: auto;            margin-bottom: 5px;            text-align: center;        }        .line-of-items .col {            width: 50px;        }        .in-the-middle {            justify-content: center;        }    </style></head><body>    <div class=\"wrapper\">        <h1>ESP32 ADC Reading</h1>        <div class=\"control-box medium-box\">            <div class=\"control-box-header\">Whole House</div>            <div class=\"line-of-items\">                <div  class=\"col\">Current</div>                    <div id=\"current-Value-label\"  class=\"col\">20</div>                    <div  class=\"col\">mV</div>                </div>                        <div class=\"line-of-items\"><div>Automatic Update</div></div>                        <div class=\"line-of-items\">                <button id=\"start-update-button\" class=\"in-the-middle\">Start</button>                <button id=\"stop-update-button\" class=\"in-the-middle\">Stop</button>            </div>            </div>    </div>       <script>                const Http = new XMLHttpRequest();        const url='http://10.0.0.250/update';                              var activeUpdate = false;        const toggleUpdateButtons = (running) => {            document.getElementById(\"start-update-button\").disabled = running;            document.getElementById(\"stop-update-button\").disabled = !running;                   };            const startUpdateButtonClickHandler  = () => {            console.log(\"Start auto update\");            activeUpdate = true;            toggleUpdateButtons(activeUpdate);            setTimeout(updateValueHandler, 300);        };        const stopUpdateButtonClickHandler  = () => {            console.log(\"Stop auto update\");            activeUpdate = false;            toggleUpdateButtons(activeUpdate);        };           const updateValueHandler  = () => {            console.log(\"Handler of update of value: Request to \" + url);            Http.open(\"GET\", url);            Http.send();             if (activeUpdate)        setTimeout(updateValueHandler, 300);                    };         Http.onreadystatechange = (e) => {            const valueLabel = document.getElementById(\"current-Value-label\");            valueLabel.innerHTML = Http.responseText;                     };           const startUpdateButton = document.getElementById(\"start-update-button\");           startUpdateButton.addEventListener(\"click\", startUpdateButtonClickHandler );           const stopUpdateButton = document.getElementById(\"stop-update-button\");           stopUpdateButton.addEventListener(\"click\", stopUpdateButtonClickHandler );           toggleUpdateButtons(activeUpdate);    </script>    </body></html>";



static void initADC();
static void check_efuse();
static void print_char_val_type(esp_adc_cal_value_t val_type);
static void adc_read_thread(void* arg);
static void blink_green_led_task(void* arg);



//static esp_err_t any_handler(httpd_req_t *req);
static esp_err_t indexpage_get_handler(httpd_req_t *req);
static esp_err_t update_get_handler(httpd_req_t *req);


static const httpd_uri_t indexpage = {
    .uri       = "/index.html",
    .method    = HTTP_GET,
    .handler   = indexpage_get_handler,
    .user_ctx  =  http_index_hml    
};



static const httpd_uri_t update_get = {
    .uri       = "/update",
    .method    = HTTP_GET,
    .handler   = update_get_handler,
    .user_ctx  = "Hello World!"
};

