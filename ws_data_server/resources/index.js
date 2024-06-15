const MAX_PAYLOAD_SIZE = 100000;
const MAX_FRAME_COUNT  = 10;

const GRAPH_WIDTH   = 350;
const GRAPH_HEIGHT  = 200;

const GRAPH_BORDER_MARGIN        = 35;

const GRAPH_Y_AXIS_OFFSET    = (4*GRAPH_HEIGHT)/5 + GRAPH_BORDER_MARGIN;
const GRAPH_X_AXIS_START_POS     = GRAPH_BORDER_MARGIN; 
const GRAPH_X_AXIS_END_POS       = GRAPH_WIDTH  - GRAPH_BORDER_MARGIN;
const GRAPH_Y_AXIS_START_POS     = GRAPH_BORDER_MARGIN; 
const GRAPH_Y_AXIS_END_POS       = GRAPH_HEIGHT  - GRAPH_BORDER_MARGIN;

const GRAPH_X_SPACER             = 10;


const DRAWABLE_WIDTH = GRAPH_X_AXIS_END_POS - GRAPH_X_AXIS_START_POS;
const DRAWABLE_HEIGHT = GRAPH_Y_AXIS_OFFSET - GRAPH_BORDER_MARGIN;

const BUTTON_DISABLED = false;
const BUTTON_ENABLED  = true;




const wsUri = "ws://10.0.0.250/ws";
var websocket;
var activeUpdate = false;

let dataPoints = [];
var canvas = document.getElementById("graph-canvas");
var ctx = canvas.getContext("2d");
var transferActive = false;

var frameCount = 0;
var framesReceived = [];

var drawSolution = {
    revalidated: false, 
    nbrBars: 0,
    maxSizeValue: 0,
    barsWidth: 0,
    barsMaxHeigth: 0,
    barsValueToHeightRatio: 0
};
    
const displayConnectionStatus = (state) => {
    
    const connectStatus = document.querySelector("#connection-status-label");
    connectStatus.innerHTML = state;
};

const writeToScreen = (message) => {
    
    console.log('Write Message ' + message);
};


function isFrameCountReached(currentFrames, frameCountTarget) {
     return currentFrames == frameCountTarget;
}

function initWebSocket() {
    websocket = new WebSocket(wsUri);
    
    websocket.binaryType = "arraybuffer";
        
    websocket.onopen = function(evt) {
        console.log('CONNECTED Event');
        displayConnectionStatus('Connected');
        frameCount = 0;
    };

    websocket.onmessage  = function(evt) {
        
        const now = Math.floor(performance.now() * 10);
        console.log('onmessage Event: ' + now);

        if (evt.data instanceof ArrayBuffer) {
            const dataArray = evt.data;

            framesReceived.push({"time":now , "size":dataArray.byteLength});
            console.log("ArrayBuffer received: lenght is " + dataArray.byteLength + "  framesReceived is "+ framesReceived.length);


            if (isFrameCountReached(framesReceived.length,frameCount)) {
                console.log("Readed!");
                setStateUpdateButtons(BUTTON_ENABLED,BUTTON_DISABLED);
                redrawCanvas(ctx);
            }

    
        } else
          console.log("No info on the type of data received!");

       

        
       
    };
        
        
    websocket.onclose = function(evt) {
        console.log('DISCONNECTED Event');
       displayConnectionStatus('Connected');
    };
        
    websocket.onerror = function(evt) {
           console.log('onerror Event');
          displayConnectionStatus('Error');
    };
}

const setStateUpdateButtons = (startButton,StopButton) => {

    document.getElementById("start-update-button").disabled = !startButton;;
    document.getElementById("stop-update-button").disabled = !StopButton;
   
};


const toggleUpdateButtons = (running) => {

    document.getElementById("start-update-button").disabled = running;
    document.getElementById("stop-update-button").disabled = !running;
   
};

const startTransferButtonClickHandler  = () => {
    console.log("Start transfer Handler");
    activeUpdate = true;
    frameCount = 0;
    framesReceived.length = 0;

    const size = document.getElementById("payload-size-input").value;
    frameCount = document.getElementById("frame-count-input").value;
    
   
   console.log("Value of payload size is " + size);
   console.log("Value of frame count is " + frameCount);
    

   if (size <= 0 || size > MAX_PAYLOAD_SIZE) {
        alert('Enter a greater than 0 and less that 100K size of the payload to be returned');
        return;
   }

   if (frameCount <= 0 || frameCount > MAX_FRAME_COUNT) {
        alert('Enter a greater than 0 and less that 10 for the number of frames to be returned');
        return;
   }

   websocket.send('start:' + size + ":" + frameCount);
   toggleUpdateButtons(activeUpdate);    
};

const stopTransferButtonClickHandler  = () => {
    console.log("Stop transfer Handler");
    activeUpdate = false;

    websocket.send('stop');
    toggleUpdateButtons(activeUpdate);
};

 const updateValueHandler  = () => {
    console.log("Handler of update of value: Request to " + url);

    if (activeUpdate)        setTimeout(updateValueHandler, 300);           
 };



 const connectionSwitchHandler  = (event) => {

    const connectSwitch = document.getElementById("switch");
    console.log('connectionSwitchHandler');

    if (event.currentTarget.checked) {
        console.log('checked');
        initWebSocket();
        setStateUpdateButtons(BUTTON_ENABLED,BUTTON_DISABLED);
     
    } else {
        console.log('not checked');
        websocket.close(1000,"Work Completed");
        setStateUpdateButtons(BUTTON_DISABLED,BUTTON_DISABLED);
    }
 };




function calculateDrawing() {

    function getSizes(){
        return framesReceived.map(d => d.size);
      }

    function getTimes(){
        return framesReceived.map(d => d.time);
    }

    console.log(framesReceived.length + " frames to draw");
    console.log(dataPoints.length + " points to draw");

    drawSolution.nbrBars = framesReceived.length;
    drawSolution.barsWidth = DRAWABLE_WIDTH / 10;
    

    let maxValue = Math.max(...getSizes());
    let startTime = Math.min(...getTimes());
    let endTime = Math.max(...getTimes());
    
    drawSolution.timeRange = endTime - startTime;

    console.log(`The max value is ${maxValue}`);

    framesReceived.forEach( (element) => {element.time = element.time - startTime; });
    
    drawSolution.maxSizeValue = maxValue;
    drawSolution.barsMaxHeigth = DRAWABLE_HEIGHT;
    drawSolution.barsValueToHeightRatio = DRAWABLE_HEIGHT / maxValue;

    drawSolution.revalidated = true;
}

function clearCanvas(ctx){

    ctx.fillStyle = "#7b49af";
    ctx.fillRect(0, 0,  GRAPH_WIDTH, GRAPH_HEIGHT);
}

function drawAxis(ctx){

    ctx.beginPath();
    ctx.moveTo(GRAPH_X_AXIS_START_POS, GRAPH_Y_AXIS_OFFSET+2);
    ctx.lineTo(GRAPH_X_AXIS_END_POS, GRAPH_Y_AXIS_OFFSET+2);

    ctx.lineWidth = 2;
    ctx.strokeStyle = '#00ff00';
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(GRAPH_X_AXIS_START_POS-2, GRAPH_Y_AXIS_START_POS);
    ctx.lineTo(GRAPH_X_AXIS_START_POS-2, GRAPH_Y_AXIS_END_POS);
    ctx.strokeStyle = '#00ff00';
    ctx.stroke();

    ctx.font = "14px Arial";
    ctx.fillStyle = "white";
   
    let  x = GRAPH_X_AXIS_START_POS - (GRAPH_BORDER_MARGIN/2) - 5;
    const dy = (GRAPH_Y_AXIS_END_POS - GRAPH_Y_AXIS_START_POS) / 4;
    let y = GRAPH_Y_AXIS_START_POS;

    const dyLabel = Math.floor(drawSolution.maxSizeValue  / 4);

    let yLabel = drawSolution.maxSizeValue;
    for (let i=4;i>=0;i--)
    {
        ctx.fillText(yLabel,x,y);
        y += dy;
        yLabel -= dyLabel;
    }

    y = GRAPH_Y_AXIS_OFFSET + (GRAPH_BORDER_MARGIN/2) + 5;
    const dx = (GRAPH_X_AXIS_END_POS - GRAPH_X_AXIS_START_POS) / 4;
    x = GRAPH_X_AXIS_START_POS + dx;

    for (let i=1;i<5;i++)
    {
        ctx.fillText(i,x,y);
        x += dx;
     }
}



function drawData(ctx){

    if (!drawSolution.revalidated) {
        calculateDrawing();
        console.log("The values needs to be re calculated");
    }


    ctx.font = "14px Arial";
    ctx.fillStyle = "white";

    drawSolution.barsValueToHeightRatio = 1;

   
    let xBar = GRAPH_X_AXIS_START_POS;
    
    
    let barWidth = drawSolution.barsWidth;

    const graphWidth = GRAPH_X_AXIS_END_POS - GRAPH_X_AXIS_START_POS;

 
  

    console.log(`${framesReceived.length} frames to draw...`);

    let prevX = GRAPH_X_AXIS_START_POS;
    let overlapCount = 1;
    for (let i = 0; i < framesReceived.length; i++) {

        ctx.beginPath();
        ctx.lineWidth = "1";
        ctx.strokeStyle = "#9068ff";


        const barHeight = framesReceived[i].size * drawSolution.barsValueToHeightRatio;
        const yStart = GRAPH_Y_AXIS_OFFSET  - barHeight;

        const xRatio = framesReceived[i].time / (drawSolution.timeRange+100);
        xBar = GRAPH_X_AXIS_START_POS + (xRatio * graphWidth);

        console.log(`time = ${framesReceived[i].time} ratio = ${xRatio} x = ${xBar}  Height: ${barHeight}`);

        var grd = ctx.createLinearGradient(0, 0, 0, barHeight/2);
        grd.addColorStop(0, "#b068ff");
        grd.addColorStop(1, "#f098ff");


        ctx.fillStyle = grd;
        ctx.fillRect(xBar, yStart, drawSolution.barsWidth, barHeight);
        ctx.rect(xBar, yStart,  drawSolution.barsWidth,  barHeight);
                
        ctx.stroke();

        if ((xBar-prevX) < 5){

            overlapCount++;
            console.log("Bat overlap " + overlapCount);
            ctx.fillStyle = "white";
            ctx.fillText(overlapCount,xBar + (drawSolution.barsWidth/2),yStart + barHeight/2);

        } else {
            overlapCount = 1;
        }

       
        prevX = xBar;


    }

    drawSolution.revalidated = false;
}


function redrawCanvas(ctx){
    calculateDrawing();
    clearCanvas(ctx);
    drawAxis(ctx);
    drawData(ctx);
}


function testingDrawing() {

    console.log("testingDrawing");
    redrawCanvas(ctx);

}



function clearFrameCanvasHandler() {

    framesReceived.length = 0;

    console.log("clearFrameCanvasHandler: Size is now " + framesReceived.length);
    clearCanvas(ctx);
    drawAxis(ctx);
}



function testingDrawing2() {

    console.log("testingDrawing");
    redrawCanvas(ctx);
}

function clearSignalCanvasHandler() {

    console.log("clearSignalCanvasHandler");
}

  
function setEventHandlers() {

   const startTransferButton = document.getElementById("start-update-button");
   startTransferButton.addEventListener("click", startTransferButtonClickHandler );

   const stopTransferButton = document.getElementById("stop-update-button");
   stopTransferButton.addEventListener("click", stopTransferButtonClickHandler );

   const connectSwitch = document.getElementById("switch");
   connectSwitch.addEventListener("change", connectionSwitchHandler );
   connectSwitch.value = true;


   const testButton = document.getElementById("test1-button");
   testButton.addEventListener("click", testingDrawing );

   const clearFrameButton = document.getElementById("clear-frame-canvas-button");
   clearFrameButton.addEventListener("click", clearFrameCanvasHandler );

   const testButton2 = document.getElementById("test2-button");
   testButton2.addEventListener("click", testingDrawing2 );

   const clearSignalButton = document.getElementById("clear-signal-canvas-button");
   clearSignalButton.addEventListener("click", clearSignalCanvasHandler );

     
    document.getElementById("payload-size-input").value = 100;
    document.getElementById("frame-count-input").value = 1;

 }
  
 setEventHandlers();
 redrawCanvas(ctx);