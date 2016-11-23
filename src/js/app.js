// Import the Clay package
var Clay = require('pebble-clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay
var clay = new Clay(clayConfig);

var locationOptions = {
  enableHighAccuracy: false,
  maximumAge: 21600000, // 6h
  timeout: 60000, // 1min
};

function locationSuccess(pos) {
  console.log('lat= ' + pos.coords.latitude + ' lon= ' + pos.coords.longitude);

  const TRIG_MAX_ANGLE = 0x10000;

  var loc = {
      'longitude': Math.round(pos.coords.longitude * TRIG_MAX_ANGLE / 360),
      'latitude': Math.round(pos.coords.latitude * TRIG_MAX_ANGLE / 360),
  }

  Pebble.sendAppMessage(loc,
    function(e) {
      console.log('Send successful.');
    },
    function(e) {
      console.log('Send failed!');
    }
  );
}

function locationError(err) {
  console.log('location error (' + err.code + '): ' + err.message);
}

Pebble.addEventListener('ready',
  function(e) {
    Pebble.sendAppMessage({'ready': 1},
      function(e) {
        console.log('Send ready.');
      },
      function(e) {
        console.log('Send failed!');
      }
    );
  }
);

Pebble.addEventListener('appmessage',
    function(e) {
      console.log("appmsg: " + JSON.stringify(e.payload));
      var r = e.payload['request'];
      if (r != null) {
        switch (r) {
        case 0:
          // Request current position
          navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
          break;
        }
      }
    }
);
