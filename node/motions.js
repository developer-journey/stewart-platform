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

function boxXY(transform, duration) {
  return set(transform).then(function(transform) {
    return ease(transform, [0, 0, 0,  -0.5, 0.5, 0], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,  0.5, 0.5, 0], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,  0.5, -0.5, 0], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,  -0.5, -0.5, 0], duration / 4);
  });
}

function boxYZ(transform, duration) {
  return set(transform).then(function(transform) {
    return ease(transform, [ 0, 0, 0,   0, -0.5, 0.5], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,   0,  0.5, 0.5], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,   0,  0.5, -0.5], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,   0, -0.5, -0.5], duration / 4);
  });
}

function boxXZ(transform, duration) {
  return set(transform).then(function(transform) {
    return ease(transform, [ 0, 0, 0,  -0.5, 0,  0.5], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,   0.5, 0,  0.5], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,   0.5, 0, -0.5], duration / 4);
  }).then(function(transform) {
    return ease(transform, [ 0, 0, 0,  -0.5, 0, -0.5], duration / 4);
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

function spin(transform, duration, loops) {
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
       Math.cos(alphaPi), Math.sin(alphaPi), 0, 10
      ];
      outputTransform(transform);
      setTimeout(tick, 1000 / 50);
    }

    tick();
  });
}

let transform = [ 0, 0, 0, 0, 0, 0 ];

function cycle(transform) {
  console.error("Initializing to " + transform);
  set(transform).then(function(transform) {
    console.error("Doing BOX X-Y");
    return boxXY(transform, duration);
  }).then(function(transform) {
    return boxXY(transform, duration);
  }).then(function(transform) {
    console.error("Doing BOX X-Z");
    return boxXZ(transform, duration);
  }).then(function(transform) {
    return boxXZ(transform, duration);
  }).then(function(transform) {
    console.error("Doing BOX Y-Z");
    return boxYZ(transform, duration);
  }).then(function(transform) {
    return boxYZ(transform, duration);
  }).then(function(transform) {
    console.error("Resetting for WAVE");
    /* a transform isn't passed to wave; instead we set to the xyz of 0, 0, 0.5 position */
    return ease(transform, [ 0, 0, 0,  0,  0, 0.5 ], duration / 4).then(function() {
      console.error("Doing WAVE");
      return wave(undefined, duration, 3);
    }).then(function(transform) {
      console.error("Doing SPIN");
      return spin(transform, duration, 3);
    }).then(function(transform) {
      /* wave and spin return an axis-angle transform -- reset it to 0, then return a
       * euclidian transform used by the other functions */
      console.error("Resetting for BOX moves");
      return ease(transform, [ 0, 1, 0,  0,  0, 0, 0 ], duration / 4).then(function() {
        return [ 0, 0, 0,  0, 0, 0 ];
      });
    });
  }).then(function(transform) {
    setTimeout(cycle.bind(undefined, transform));
  });
}

cycle(transform);
