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

      const buffer = new ArrayBuffer(8);
      for (let i=0;i<8;i++) buffer[i] = i;

	  if (messageCount <= 8) {

        
        

        var message = "d" + Date.now() + ":" + Math.floor(Math.random() * 100);
//         ws.send(`${message}`);   // For single client, use this one..
         ws.send(buffer);   // For single client, use this one..

         

         const delay = Math.floor(Math.random() * 70) + 10;
         console.log(`Delay will be ${delay}`);

	  	  setTimeout(transferHandler, delay);
    } else {
        transferActive = false;
        messageCount = 0;
    }

	 
    
       /*   wss.clients.forEach((client) => {       // For multiple clients, use this code instead...
        client.send(`${messageCount}`);
      }); */
    }



    ws.on('message', function incoming(message) {

//        if (message == 'start') {

        const msgString = message.toString();

        if (msgString.startsWith('start')) {

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
