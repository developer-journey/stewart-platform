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
      let x = transform.x, y = transform.y, z = transform.z, M;

      M = Math.sqrt(x * x + y * y + z * z);

      if (M > 0) {
        x = x * maxAngle / M;
        y = y * maxAngle / M;
        z = z * maxAngle / M;
      } else {
        x = 0;
        y = 0;
        z = 0;
      }

      let tX = transform.tX, tY = transform.tY, tZ = transform.tZ, tM;

      tM = Math.sqrt(tX * tX + tY * tY + tZ * tZ);
      if (tM > 0) {
        tX = tX * maxDistance / tM;
        tY = tY * maxDistance / tM;
        tZ = tZ * maxDistance / tM;
      } else {
        tX = 0;
        tY = 0;
        tZ = 0;
      }

      timeout = 0;

      let line = x + " " + y + " " + z + " " +
        " " + tX + " " + tY + " " + tZ;

      console.log(line);
      console.error(line);
    });
  };
})();

ds3.on("l1:analog", function(data) {
  transform.z = (data / 2);
  debounce();
});
ds3.on("l2:analog", function(data) {
  transform.z = -(data / 2);
  debounce();
});

ds3.on("r1:analog", function(data) {
  transform.tZ = (data / 2);
  debounce();
});

ds3.on("r2:analog", function(data) {
  transform.tZ = -(data / 2);
  debounce();
});

ds3.on("left:move", function(data) {
  transform.x = (data.y - 128) / 128;
  transform.y = (data.x - 128) / 128;
  debounce();
});

ds3.on("right:move", function(data) {
  transform.tX = (data.x - 128) / 128;
  transform.tY = (data.y - 128) / 128;
  debounce();
});


ds3.on("connected", function() { console.log("connected"); });
