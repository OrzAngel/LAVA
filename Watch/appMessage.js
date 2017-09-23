Pebble.addEventListener("appmessage", 
  function(e) {    
    if (e.payload) {
      var str = "";
      
      if (e.payload.request) {
        str = e.payload.request;
      }
      
      if (e.payload.request_data) {
        str = e.payload.request_data;        //0
      }  
      
      if (e.payload.request_mode_change) {
        str = e.payload.request_mode_change; //3 or 4
      }
      
      if (e.payload.request_unit_change) {
        str = e.payload.request_unit_change; //1
      }
      
      if (e.payload.request_warning) {
        str = e.payload.request_warning;// 2/threshold unit
      }
      console.log(str);
      sendToServer(str);      
    }
  }
);

function sendToServer(str) {
  var req = new XMLHttpRequest();
  var ipAddress = "130.91.143.71"; // Hard coded IP address
  var port = "3001"; // Same port specified as argument to server 
  var url = "http://" + ipAddress + ":" + port + "/595/" + str;
  console.log(url);
  var method = "GET";
  var async = true;
  req.onload = function(e) {
    console.log("response");
    var response = JSON.parse(req.responseText); 
    if (response) {
      if (response.isWarning){
        console.log(response.isWarning);
        Pebble.sendAppMessage({
          'warning' : response.isWarning,
          'response': "1"});
      }else{
        var dict = {
          'low'      : response.low,
          'high'     : response.high,
          'avg'      : response.avg,
          'recent'   : response.recent,
          'isConnect': response.isConnect,
          'response' : "1"
        };
        Pebble.sendAppMessage(dict);
      }
    } else {
      Pebble.sendAppMessage({'response' : "0"});
    } 
  };
  req.onerror = function(e) {
    console.log("fail");
    Pebble.sendAppMessage({'response' : "0"});
  };
  req.open(method, url, async);   
  req.send(null);
}
