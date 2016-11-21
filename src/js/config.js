module.exports = [{
    "type": "heading",
    "defaultValue": "Placidial Configuration"
}, {
    "type": "submit",
    "defaultValue": "Save Settings"
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Bluetooth and Battery Status"
    }, {
        "type": "toggle",
        "messageKey": "statusshow",
        "label": "Show Icon on Disconnect",
        "defaultValue": true
    }, {
        "type": "radiogroup",
        "messageKey": "statusvibe",
        "label": "Vibrate on Disconnect",
        "defaultValue": "1",
        "options": [{
            "label": "Off",
            "value": 0
        }, {
            "label": "Short",
            "value": 1
        }, {
            "label": "Long",
            "value": 2
        }, {
            "label": "Double",
            "value": 3
        }]
    }, {
        "type": "slider",
        "messageKey": "batwarn",
        "defaultValue": 10,
        "label": "Battery Warn Level",
        "min": 0,
        "max": 100,
        "step": 5
    }]
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Day"
    }, {
        "type": "toggle",
        "messageKey": "dayshow",
        "label": "Show",
        "defaultValue": true
    }, {
        "type": "radiogroup",
        "messageKey": "dayfont",
        "label": "Font",
        "defaultValue": "1",
        "options": [{
            "label": "Blocky",
            "value": 0
        }, {
            "label": "Smooth",
            "value": 1
        }]
    }]
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Dial Numbers"
    }, {
        "type": "toggle",
        "messageKey": "hournumshow",
        "label": "Show Hour",
        "defaultValue": true
    }, {
        "type": "toggle",
        "messageKey": "minnumshow",
        "label": "Show Minute",
        "defaultValue": false
    }, {
        "type": "radiogroup",
        "messageKey": "dialfont",
        "label": "Font",
        "defaultValue": "1",
        "options": [{
            "label": "Blocky",
            "value": 0
        }, {
            "label": "Smooth",
            "value": 1
        }]
    }]
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Colors"
    }, {
        "type": "toggle",
        "messageKey": "outline",
        "label": "Outline",
        "defaultValue": true
    }, {
        "type": "radiogroup",
        "messageKey": "colorflip",
        "label": "Invert Brightness",
        "defaultValue": "0",
        "options": [{
            "label": "No Inversion",
            "value": 0
        }, {
            "label": "Invert at Day",
            "value": 1
        }, {
            "label": "Invert at Night",
            "value": 2
        }]
    }, {
        "type": "color",
        "messageKey": "bgcol",
        "defaultValue": "0x000000",
        "label": "Background Color",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "statuscol",
        "defaultValue": "0xFFFFFF",
        "label": "Status Icons",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "hourcol",
        "defaultValue": "0xFFFFFF",
        "label": "Hour Hand",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "hourtickcol",
        "defaultValue": "0xFFFFFF",
        "label": "Hour Tick",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "mincol",
        "defaultValue": "0xFFFFFF",
        "label": "Minute Hand",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "mintickcol",
        "defaultValue": "0x0FF0FF",
        "label": "Minute Tick",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "centercol",
        "defaultValue": "0xFFFFFF",
        "label": "Center",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "seccol",
        "defaultValue": "0xFF0000",
        "label": "Second Hand",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "seccentercol",
        "defaultValue": "0xFF0000",
        "label": "Second Center",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "dialnumcol",
        "defaultValue": "0xFFFFFF",
        "label": "Dial Numbers",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "daycol",
        "defaultValue": "0xFFFFFF",
        "label": "Day of Month",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "weekcol",
        "defaultValue": "0xFFFFFF",
        "label": "Weekday",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "sundaycol",
        "defaultValue": "0x00AAFF",
        "label": "Sunday",
        "sunlight": false
    }, {
        "type": "color",
        "messageKey": "todaycol",
        "defaultValue": "0xFF0000",
        "label": "Today",
        "sunlight": false
    }]
}, {
    "type": "submit",
    "defaultValue": "Save Settings"
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Hour Hand"
    }, {
        "type": "toggle",
        "messageKey": "hourbelowmin",
        "label": "Below Minute Hand",
        "defaultValue": false
    }, {
        "type": "slider",
        "messageKey": "hourlen",
        "defaultValue": 50,
        "label": "Length (%)",
        "min": 0,
        "max": 90,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "hourext",
        "defaultValue": "0",
        "label": "Extension (%)",
        "min": 0,
        "max": 30,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "hourwidth",
        "defaultValue": 8,
        "label": "Width",
        "min": 1,
        "max": 15,
        "step": 1
    }]
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Minute Hand"
    }, {
        "type": "slider",
        "messageKey": "minlen",
        "defaultValue": 83,
        "label": "Length (%)",
        "min": 0,
        "max": 90,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "minext",
        "defaultValue": "0",
        "label": "Extension (%)",
        "min": 0,
        "max": 30,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "minwidth",
        "defaultValue": 8,
        "label": "Width",
        "min": 1,
        "max": 15,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "centerwidth",
        "defaultValue": 14,
        "label": "Center Width",
        "min": 0,
        "max": 32,
        "step": 1
    }]
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Second Hand"
    }, {
        "type": "radiogroup",
        "messageKey": "showsec",
        "label": "Show",
        "defaultValue": "1",
        "options": [{
            "label": "Never",
            "value": 0
        }, {
            "label": "Always",
            "value": 1
        }, {
            "label": "On Shake",
            "value": 2
        }]
    }, {
        "type": "slider",
        "messageKey": "sectimeout",
        "defaultValue": 10,
        "label": "Timeout (s)",
        "description": "Timeout in seconds for displaying second hand when 'On Shake' is set.",
        "min": 5,
        "max": 120,
        "step": 5
    }, {
        "type": "slider",
        "messageKey": "seclen",
        "defaultValue": 82,
        "label": "Length (%)",
        "min": 0,
        "max": 90,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "secext",
        "defaultValue": 16,
        "label": "Extension (%)",
        "min": 0,
        "max": 30,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "secwidth",
        "defaultValue": 3,
        "label": "Width",
        "min": 1,
        "max": 15,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "seccenterwidth",
        "defaultValue": 8,
        "label": "Center Width",
        "min": 0,
        "max": 32,
        "step": 1
    }]
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Hour Tick"
    }, {
        "type": "radiogroup",
        "messageKey": "hourtickshow",
        // "label": "Hour Tick",
        "defaultValue": "1",
        "options": [{
            "label": "Off",
            "value": 0
        }, {
            "label": "Last Hour",
            "value": 1
        }, {
            "label": "Nearest Hour",
            "value": 2
        }]
    }, {
        "type": "slider",
        "messageKey": "hourticklen",
        "defaultValue": 6,
        "label": "Length",
        "min": 1,
        "max": 16,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "hourtickwidth",
        "defaultValue": 4,
        "label": "Width",
        "min": 1,
        "max": 16,
        "step": 1
    }]
}, {
    "type": "section",
    "items": [{
        "type": "heading",
        "defaultValue": "Minute Ticks"
    }, {
        "type": "radiogroup",
        "messageKey": "mintickshow",
        // "label": "Minute Ticks",
        "defaultValue": "1",
        "options": [{
            "label": "Off",
            "value": 0
        }, {
            "label": "Last 5-Minute",
            "value": 1
        }, {
            "label": "Nearest 5-Minute",
            "value": 2
        }]
    }, {
        "type": "slider",
        "messageKey": "minticklen",
        "defaultValue": 5,
        "label": "Length",
        "min": 1,
        "max": 16,
        "step": 1
    }, {
        "type": "slider",
        "messageKey": "mintickwidth",
        "defaultValue": 3,
        "label": "Width",
        "min": 1,
        "max": 16,
        "step": 1
    }]
}, {
    "type": "submit",
    "defaultValue": "Save Settings"
}];
