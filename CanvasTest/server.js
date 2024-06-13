const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 8080 });

var messageCount = 0;
var transferActive = false;

console.log('Server , open on port 8080...');




const connectionHandler = (ws) => {

    console.log('Client connected');

    function transferHandler() {

      if (!transferActive) {
        messageCount = 0;
        return;
      } 

  
	  messageCount++;
	  console.log("Tick " + messageCount);

	  if (messageCount <= 25) {

        var message = "d" + Date.now() + ":" + Math.floor(Math.random() * 100);
         ws.send(`${message}`);   // For single client, use this one..

	  	  setTimeout(transferHandler, 100);
    } else {
        transferActive = false;
        messageCount = 0;
    }

	 
    
       /*   wss.clients.forEach((client) => {       // For multiple clients, use this code instead...
        client.send(`${messageCount}`);
      }); */
    }



    ws.on('message', function incoming(message) {

        if (message == 'start') {
            console.log('Start Transfer Request received...');
            transferActive = true;
            const myTimer = setTimeout(transferHandler, 100);
        } else   if (message == 'stop') {
            console.log('Stop Transfer Request received...');
            transferActive = false;
        } else 
            console.log('Received: %s', message);

        ws.send(`${message}`);
    });


    ws.on('close', function () {
        console.log('Client disconnected');
    });
};


wss.on('connection', connectionHandler);
