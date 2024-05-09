var express = require('express');
var app = express();

app.use(express.static('public'));

const PORT = 3000;

app.get('/', function (req, res) {
   res.send('Hello World from Windows host!');
})

app.get('/update', function (req, res) {

   const randValue  = Math.floor(Math.random() * 10);
   console.log('Updated value to be sent is ' + randValue);
   res.send(randValue.toString());
})


var server = app.listen(PORT, function () {
   console.log("Express App running at http://127.0.0.1:3000/");
})