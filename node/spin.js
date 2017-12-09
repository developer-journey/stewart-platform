"use strict";

let tilt = process.argv.length == 3 ? process.argv[2] : 10,
  begin = Date.now(),
  then = Date.now(), target = 360, interval = 5, duration = 1000;

let totalTime = duration * interval;

function tick() {
  let now = Date.now(),
    alpha = Math.min((now - then) / duration, 1),
    angle = alpha * target;
  let x = Math.sin(angle * Math.PI / 180),
    y = Math.cos(angle * Math.PI / 180);
  console.log(x + " 0 " + y + " " + tilt * Math.pow((now - begin) / totalTime, 2));
  if (alpha != 1.0) {
    setTimeout(tick, interval);
  } else {
    if (interval == 0) {
      console.log("0 0 1 0");
      process.exit(0);
    } else {
      interval--;
      then = now;
      setTimeout(tick, interval);
    }
  }
}

tick();
