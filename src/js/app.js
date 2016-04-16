var sendWatch = function() {
    var queue = [];
    var status = 0;

    var acked, nacked, send, enqueue;
    var current;

    acked = function(data) {
        console.log("message '" + JSON.stringify(current) +
                    "' send: " + JSON.stringify(data));
        status = 0;
        if (queue.length > 0)
            send(queue.shift());
    };

    nacked = function(data, err) {
        console.log("failed to send message '" + JSON.stringify(current) +
            "': " + err + " " + JSON.stringify(data));
        console.log("retrying");
        send(current);
    }

    send = function(msg) {
        status = 1;
        current = msg;
        Pebble.sendAppMessage(msg, acked, nacked);
    }

    enqueue = function(msg) {
        switch (status) {
        case 0: send(msg); break;
        default: queue.push(msg); break;
        }
    };

    return enqueue;
}();


function readConfig() {
    var keys, data;
    var configKeys = localStorage.getItem("configKeys");
    console.log("reading keys: " + configKeys);
    if (typeof configKeys == "string") {
        data = {};
        keys = configKeys.split(",");
        for (var i in keys) {
            var k = keys[i];
            data[k] = localStorage.getItem(k);
            console.log("read: " + k + " = " + data[k]);
        }
    }
    return data;
}

function saveConfig(data) {
    var keys = [];
    if (data) {
        for (var k in data) {
            keys.push(k);
            console.log("save: " + k + " = " + data[k]);
            localStorage.setItem(k, data[k]);
        }
        console.log("saving keys: " + keys.join());
        localStorage.setItem("configKeys", keys.join());
    }
}

Pebble.addEventListener('ready', function() {
    console.log('PebbleKit JS ready!');
});

Pebble.addEventListener('showConfiguration', function(e) {
    // Show config page
    var query = "";
    var configData = readConfig();
    if (configData) {
        for (var k in configData){
            query += (query.length == 0) ? "?" : "&";
            query += k + "=" + encodeURIComponent(configData[k]);
        }
        console.log("query: " + query);
    }
    Pebble.openURL("https://jackscan.github.io/placidial/config.html" + query);
});

Pebble.addEventListener('webviewclosed', function(e) {
    var configData = JSON.parse(decodeURIComponent(e.response));
    console.log('Configuration page returned: ' + JSON.stringify(configData));
    if (configData != "CANCELLED") {
        saveConfig(configData);
        sendWatch(configData);
    }
    else console.log("cancelled");
});
