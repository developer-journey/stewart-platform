

"use strict";

const transform = [ 0, 0, 0, 0, 0, 0 ],
  duration = 200;

function outputTransform(transform) {
  let rounded = transform.map(function(value) {
    if (typeof value == "string") {
      return value;
    }
    return Math.round(value * 100) / 100;
  });
  console.log(rounded.join(" "));
}

function set(transform) {
  return new Promise(function(resolve, reject) {
    outputTransform(transform);
    return resolve(transform);
  });
}

function ease(start, stop, period) {
  return new Promise(function(resolve, reject) {
    let startTime = Date.now();
    function tick() {
      let alpha = (Date.now() - startTime) / period,
        interpolated = [];
      if (alpha >= 1.0) {
        interpolated = stop;
      } else {
        start.forEach(function(value, index) {
          interpolated[index] = (stop[index] - value) * alpha + value;
        });
      }

      outputTransform(interpolated);

      if (alpha < 1.0) {
        setTimeout(tick, 1000 / 50);
      } else {
        return resolve(stop);
      }
    }

    tick();
  });
}

let start = Date.now();
function cycle() {
  var alpha = ((Date.now() - start) % 5000) / 5000,
        x = 0.5 * Math.cos(alpha * 2 * 3.1415926535),
	y = 0.5 * Math.sin(alpha * 2 * 3.1415926535);
  outputTransform([-y * 10, -x * 10, 0, x, y, 0]);
  setTimeout(cycle, 1000 / 100);
}

cycle();
