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


/* A simple example that demonstrates using websocket echo server
 */
static const char *TAG = "ws_echo_server";

const static char http_index_hml[] =
"<!DOCTYPE html><html lang=\"en\"><head>    <meta charset=\"UTF-8\">    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">    <title>ESP32 Web Socket</title>    <style>        html {            height: 100%;        }        body {            height: 100%;            background-image: linear-gradient(#4D1496, #7D32BF);            background-repeat: no-repeat;            color: white;        }        .wrapper {            width: 40%;            margin: auto;        }        .control-box {            display: flex;            flex-direction: column;                     border: 1px solid #7D52B7;            border-radius: 0px 0px 10px 10px;        }        .control-box-header {            background-color: #DBD5F1;            color: #5C369F;            border-bottom: 1px solid #7D52B7;        }        .medium-box {            width: 250px;        }        .line-of-items {            display: flex;            margin: auto;            margin-bottom: 5px;            text-align: center;        }        .line-of-items .col {            width: 70px;        }        .in-the-middle {            justify-content: center;        }        .slidecontainer {             width: 100%;         }           </style></head><body>    <div class=\"wrapper\">        <h1>ESP32 Web Socket Test</h1>        <div class=\"control-box medium-box\">            <div class=\"control-box-header\">Whole House</div>            <div class=\"line-of-items\">                <div  class=\"col\">Status:</div>                    <div id=\"connection-status-label\"  class=\"col\"></div>                </div>                        <div class=\"line-of-items\"><div>Automatic Update</div></div>                        <div class=\"line-of-items\">                <button id=\"start-update-button\" class=\"in-the-middle\">Start</button>                <button id=\"stop-update-button\" class=\"in-the-middle\">Stop</button>            </div>            <div class=\"line-of-items\"><div>Transfer Rate</div></div>            <div class=\"line-of-items\">                            <div class=\"slidecontainer\">                    <input type=\"range\" min=\"1\" max=\"100\" value=\"50\" class=\"slider\" id=\"myRange\">                    <p>Value: <span id=\"transferspeed\"></span></p>                </div>            </div>            </div>    </div>       <script>        		const wsUri = \"ws://10.0.0.250/ws\";        console.log('Will try to open the WebSocket on ' + wsUri);    	const websocket = new WebSocket(wsUri);				              var activeUpdate = false;				const displayConnectionStatus = (state) => {						const connectStatus = document.querySelector(\"#connection-status-label\");			connectStatus.innerHTML = state;		};				websocket.onopen = (e) => {		 console.log('CONNECTED Event');		 displayConnectionStatus('Connected');		};		websocket.onclose = (e) => {		   console.log('DISCONNECTED Event');		   displayConnectionStatus('Connected');		};				websocket.onmessage = (e) => {		  console.log('onmessage Event');		  writeToScreen(`RECEIVED: ${e.data}`);		};		websocket.onerror = (e) => {		  console.log('onerror Event');		  displayConnectionStatus('Error');		};        	             const toggleUpdateButtons = (running) => {            document.getElementById(\"start-update-button\").disabled = running;            document.getElementById(\"stop-update-button\").disabled = !running;                   };            const startTransferButtonClickHandler  = () => {            console.log(\"Start transfer Handler\");            activeUpdate = true;            websocket.send('Start Daniel!');            toggleUpdateButtons(activeUpdate);                            };        const stopTransferButtonClickHandler  = () => {            console.log(\"Stop transfer Handler\");            activeUpdate = false;            toggleUpdateButtons(activeUpdate);                  };           const updateValueHandler  = () => {            console.log(\"Handler of update of value: Request to \" + url);                       if (activeUpdate)        setTimeout(updateValueHandler, 300);                    };                 const updateTransferRateHandler  = () => {            transferspeed.innerHTML = slider.value;         };                    const startTransferButton = document.getElementById(\"start-update-button\");           startTransferButton.addEventListener(\"click\", startTransferButtonClickHandler );           const stopTransferButton = document.getElementById(\"stop-update-button\");           stopTransferButton.addEventListener(\"click\", stopTransferButtonClickHandler );           const slider = document.getElementById(\"myRange\");           slider.addEventListener(\"input\", updateTransferRateHandler );           	     var transferspeed = document.getElementById(\"transferspeed\");          transferspeed.innerHTML = slider.value;          toggleUpdateButtons(activeUpdate);    </script>    </body></html>";






static void configure_led(void);
static void blink_green_led_task(void* arg);
static esp_err_t indexpage_get_handler(httpd_req_t *req);
static void ws_async_send(void *arg);
static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req);
static esp_err_t echo_handler(httpd_req_t *req);
static httpd_handle_t start_webserver(void);
static esp_err_t stop_webserver(httpd_handle_t server);
static void transfer_to_client_led_task(void* arg);

static esp_err_t trigger_async_send2();








static uint8_t s_green_led_state = 0;
static uint8_t s_red_led_state = 0;

static uint8_t s_transfer_active = 0;


/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static void displayHTTPReq(httpd_req_t *req);
static void displayResp_arg(struct async_resp_arg *resp_arg);



struct async_resp_arg* clientRespArg = NULL;


static void displayHTTPReq(httpd_req_t *req)
{
	printf("\thandle %p\n", req->handle );

	int newFd =  httpd_req_to_sockfd(req);
	printf("\tfd %i\n", newFd );

}

static void displayResp_arg(struct async_resp_arg *resp_arg)
{
	printf("\thandle %p\n", resp_arg->hd );
	printf("\tfd %i\n", resp_arg->fd );
}



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
    while(1) {

        if (s_transfer_active)
        {
        	if (clientRespArg == NULL)
        	{
        		//Should be created before!!!

        		 ESP_LOGI(TAG, "clientRespArg is NULL");
      		     gpio_set_level(RED_LED_GPIO, 1);

        	}
        	else
        	{


        		  s_green_led_state = !s_green_led_state;
        		            // s_red_led_state = !s_red_led_state;
        		   gpio_set_level(GREEN_LED_GPIO, s_green_led_state);
        		   gpio_set_level(RED_LED_GPIO, 0);

        		   trigger_async_send2();
        		   ESP_LOGI(TAG, "trigger_async_send2!!!");

        	}


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



/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    static const char * data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
        free(resp_arg);
    }
    return ret;
}

static esp_err_t trigger_async_send2()
{


	if (clientRespArg == NULL)
    {
		  return ESP_ERR_NO_MEM;
    }

	/*
	 struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
	    if (resp_arg == NULL) {
	        return ESP_ERR_NO_MEM;
	    }
	    resp_arg->hd = req->handle;
	    resp_arg->fd = httpd_req_to_sockfd(req);
	    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg); */
	    esp_err_t ret = httpd_queue_work(clientRespArg->hd, ws_async_send, clientRespArg);


	    return ret;

}

/*
 * This handler echos back the received ws data
 * and triggers an async send if certain message received
 */
static esp_err_t echo_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    else 
    {  
          ESP_LOGI(TAG, "req->method is %d", req->method);
    }


    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    //esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);


    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);



    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        //buf = calloc(1, ws_pkt.len + 1);
        buf = calloc(1, ws_pkt.len + 100);


        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }
    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);

    displayHTTPReq(req);


    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Trigger async") == 0) {
        free(buf);
        ESP_LOGI(TAG, "Invoked trigger_async_send...");

        return trigger_async_send(req->handle, req);
    }

    ESP_LOGI(TAG, "Invoked httpd_ws_send_frame...");






    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = echo_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};


/* This method return an analog read to the client page */
static esp_err_t start_transfer_handler(httpd_req_t *req)
{
    char buffer[20];

     ESP_LOGI(TAG, "start_transfer_handler");
 

     //


     /*
     static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)


     typedef void* httpd_handle_t;


         if (resp_arg == NULL) {
             return ESP_ERR_NO_MEM;
         }
         resp_arg->hd = req->handle;
         resp_arg->fd = httpd_req_to_sockfd(req);


        */


     clientRespArg = malloc(sizeof(struct async_resp_arg));
     clientRespArg ->hd = req->handle;
     clientRespArg ->fd = httpd_req_to_sockfd(req);

     ESP_LOGI(TAG, "Created the clientRespArg, handle is %p", clientRespArg ->hd);





         //sprintf(buffer,"\thandle %p\n", req->handle );

         //printf("\thandle %p\n", req->handle );

         displayHTTPReq(req);


     ///////////////////////
    

    const char* resp_str = (const char*)buffer;
    
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    s_transfer_active = 1;

    return ESP_OK;
}




static const httpd_uri_t start_transfer = {
    .uri       = "/start",
    .method    = HTTP_GET,
    .handler   = start_transfer_handler,
    .user_ctx  = "Hello World!"
};


/* This method return an analog read to the client page */
static esp_err_t stop_transfer_handler(httpd_req_t *req)
{
    char buffer[20];

     ESP_LOGI(TAG, "stop_transfer_handler");
 
    
    sprintf(buffer,"%s\n", "Stop Transfer" );

    const char* resp_str = (const char*)buffer;
    

     displayHTTPReq(req);



     if (clientRespArg != NULL)
     {
    	  //free(clientRespArg);
    	  ESP_LOGI(TAG, "Deleted the clientRespArg...");
     }




    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    s_transfer_active = 0;

    return ESP_OK;
}

static const httpd_uri_t stop_transfer = {
    .uri       = "/stop",
    .method    = HTTP_GET,
    .handler   = stop_transfer_handler,
    .user_ctx  = "Hello World!"
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
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &start_transfer);
        httpd_register_uri_handler(server, &stop_transfer);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}


static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
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

    server = start_webserver();

     xTaskCreate(transfer_to_client_led_task, "transfer_to_client_led_task", 2048, NULL, 5, NULL);


     configure_led();

       while (server) {
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
