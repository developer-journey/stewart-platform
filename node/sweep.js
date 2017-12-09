

"use strict";

const duration = 1000;

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
        setTimeout(tick, 1000 / 100);
      } else {
        return resolve(stop);
      }
    }

    tick();
  });
}

let transform = [ 0, 0, 0,  0, 0, 0 ];

function cycle() {
  set(transform).then(function(transform) {
    return ease(transform, [0, 0, 20,  -0.5, 0, 0.5], duration);
  }).then(function(transform) {
    return ease(transform, [ -10, 0, 0,  0.5, 0, 0.5], duration);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, -20,  0.5, 0, -0.5], duration);
  }).then(function(transform) {
    return ease(transform, [ 0, -10, 0,  -0.5, 0, -0.5], duration);
  }).then(function(update) {
    transform = update;
    setTimeout(cycle);
  });
}

cycle();
