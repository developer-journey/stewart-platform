var dualShock = require("dualshock-controller");

let maxAngle = 3, maxDistance = 0.5;

if (process.argv.length >= 3) {
  maxAngle = parseInt(process.argv[2]);
}

if (process.argv.length >= 4) {
  maxDistance = parseFloat(process.argv[3]);
}

console.error("Max angle: " + maxAngle);
console.error("Max distance: " + maxDistance);

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

let transform = {
  x: 0,
  y: 0,
  z: 0,
  magnitude: 0,
  tX: 0,
  tY: 0,
  tZ: 0,
};

let debounce = (function() {
  let timeout = 0;
  return function() {
    if (timeout) {
      return;
    }
    timeout = setTimeout(function () {
      let x = transform.x, y = transform.y, z = transform.z;

      x = x * maxAngle;
      y = y * maxAngle;
      z = z * maxAngle;

      let tX = transform.tX, tY = transform.tY, tZ = transform.tZ;

      tX = tX * maxDistance;
      tY = tY * maxDistance;
      tZ = tZ * maxDistance;

      timeout = 0;

      let line = x + " " + y + " " + z + " " +
        " " + tX + " " + tY + " " + tZ;

      console.log(line);
      console.error(line);
    });
  };
})();

ds3.on("l1:analog", function(data) {
console.error(JSON.stringify(data, null, 2));
  transform.z = alphaExpIn(data / 256);
  debounce();
});
ds3.on("l2:analog", function(data) {
console.error(JSON.stringify(data, null, 2));
  transform.z = alphaExpIn(-data / 256);
  debounce();
});

ds3.on("r1:analog", function(data) {
console.error(JSON.stringify(data, null, 2));
  transform.tZ = alphaExpIn(data / 256);
  debounce();
});

ds3.on("r2:analog", function(data) {
console.error(JSON.stringify(data, null, 2));
  transform.tZ = alphaExpIn(-data / 256);
  debounce();
});

function alphaExpIn(value) {
  var sign;
  if (value > 0) {
    sign = 1;
  } else {
    sign = -1;
    value = -value;
  }
  return sign * Math.pow(2, 10 *  value - 10);
}

function alphaExpOut(value) {
  var sign;
  if (value > 0) {
    sign = 1;
  } else {
    sign = -1;
    value = -value;
  }
  return sign * (1 - alphaExpIn(1 - value));
}

ds3.on("leftAnalogBump:press", function(data) {
  transform.tZ = maxDistance * 0.5;
  debounce();
});

ds3.on("lefttAnalogBump:release", function(data) {
  transform.tZ = 0;
  debounce();
});

ds3.on("rightAnalogBump:press", function(data) {
  transform.tZ = maxDistance * 0.25;
  debounce();
});

ds3.on("rightAnalogBump:release", function(data) {
  transform.tZ = 0;
  debounce();
});

ds3.on("forwardBackward:motion", function(data) {
  transform.x = alphaExpOut(-data.value / 128);
  debounce();
});

ds3.on("rightLeft:motion", function(data) {
  transform.y = alphaExpOut(data.value / 128);
  debounce();
});

ds3.on("left:move", function(data) {
/*  transform.x = (data.y - 128) / 128;
  transform.y = (data.x - 128) / 128;
  debounce();*/
});

ds3.on("right:move", function(data) {
/*  transform.tX = (data.x - 128) / 128;
  transform.tY = (data.y - 128) / 128;
  debounce();*/
});


ds3.on("connected", function() { console.log("connected"); });
