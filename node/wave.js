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
        setTimeout(tick, 1000 / 50);
      } else {
        return resolve(stop);
      }
    }

    tick();
  });
}

function wave(transform, duration, loops) {
  loops = loops || 1;
  duration = duration || 1000;
  transform = transform || [ 0, 1, 0, 0, 0, 0, 0.5 ];

  return new Promise(function(resolve, reject) {
    let start = Date.now();
    function tick() {
      let now = Date.now(), alpha = ((now - start) % duration) / duration;
      if (now - start >= duration * loops) {
        return resolve(transform);
      }

      let alphaPi = 2 * Math.PI * alpha;
      transform = [
        /* axis-angle */
        0, 1, 0, Math.sin(alphaPi) * 10,
        /* translation */
        -Math.sin(alphaPi) * 0.25, 0, 0.25 + Math.cos(alphaPi) * 0.5
      ];
      outputTransform(transform);
      setTimeout(tick, 1000 / 50);
    }

    tick();
  });
}

let transform = [ 0, 0, 0, 0, 0, 0 ];


function cycle(transform) {
  console.error("Doing WAVE");
  wave(undefined, duration, 3).then(function(transform) {
    setTimeout(cycle.bind(undefined, transform));
  });
}

set(transform).then(function() {
  cycle(transform);
});
