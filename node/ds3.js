var dualShock = require("dualshock-controller");

var ds3 = dualShock({
  /* config: "dualshock4-generic-driver", */
  config : "dualShock3",
  accelerometerSmoothing : true,
  analogStickSmoothing : false
});

ds3.on("error", function(error) { console.error(err); });

/**
 * DualShock 3 control rumble and light settings for the controller
 */
ds3.setExtras({
  rumbleLeft:  0,
  rumbleRight: 0,
  led: 1 /* Turn off the blinking LEDs */
});

[ "l1", "l2", "r1", "r2" ].forEach(function(id) {
  [ "analog" ].forEach(function(action) {
    ds3.on(id + ":" + action, function(data) {
      console.log(id + " " + action + ": " + JSON.stringify(data));
      if (id == "l2") {
        ds3.setExtras({ rumbleLeft: data });
      } else if (id == "r2") {
        ds3.setExtras({ rumbleRight: data });
      }
    });
  });
});

[ "left", "right" ].forEach(function(id) {
  [ "move" ].forEach(function(action) {
    ds3.on(id + ":" + action, function(data) { console.log(id + " " + action + ": " + JSON.stringify(data)); });
  });
});

[ "rightLeft", "forwardBackward", "upDown" ].forEach(function(id) {
  [ "motion" ].forEach(function(action) {
    ds3.on(id + ":" + action, function(data) { console.log(id + " " + action + ": " + JSON.stringify(data)); });
  });
});

[ "battery", "connection", "charging" ].forEach(function(id) {
  [ "change" ].forEach(function(action) {
    ds3.on(id + ":" + action, function(data) { console.log(id + " " + action + ": " + JSON.stringify(data)); });
  });
});

[ "l1", "l2", "r1", "r2", "leftAnalogBump", "rightAnalogBump",
  "select", "start", "psxButton",
  "square", "triangle", "circle", "x",
  "dpadLeft", "dpadRight", "dpadUp", "dpadDown" ].forEach(function(id) {
  [ "press", "release" ].forEach(function(action) {
    ds3.on(id + ":" + action, function(data) { console.log(id + " " + action + ": " + JSON.stringify(data)); });
  });
});

ds3.on("connected", function() { console.log("connected"); });
