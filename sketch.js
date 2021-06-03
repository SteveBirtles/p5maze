"use strict";

const TEXTURE_COUNT = 40;
const DEFAULT_FLAT = 25;
const DEFAULT_WALL = 8;

const CEILING_BIT = 0b10000;
const HIGH_BIT = 0b01000;
const LOW_BIT = 0b00100;
const WALL_BIT = 0b00010;
const GROUND_BIT = 0b00001;

const SKY = 0b00001;
const SKY_SINGLE_BLOCK = 0b00011;
const SKY_FLOATING_BLOCK = 0b00101;
const SKY_DOUBLE_BLOCK = 0b00111;
const LOW_ROOM = 0b11001;
const LOW_ROOM_SINGLE_BLOCK = 0b11011;
const CORRIDOR = 0b11101;
const WALL = 0b11111;
const HIGH_ROOM = 0b10001;
const HIGH_ROOM_SINGLE_BLOCK = 0b10011;
const HIGH_ROOM_FLOATING_BLOCK = 0b10101;
const HIGH_ROOM_DOUBLE_BLOCK = 0b10111;

const GENERATOR_WALL = WALL;
const GENERATOR_PATH = CORRIDOR;
const GENERATOR_ROOM = HIGH_ROOM;

const textures = [];

const unit = 100;
const MAX_WIDTH = 100;
const MAX_HEIGHT = 100;
const mazeWidth = 40;
const mazeHeight = 40;
let drawDistance = 1000;

let mapName = null, allMapNames = [], selectedMap = -1;

let playerX = 3 * unit / 2, playerY = 3 * unit / 2, playerZ = unit / 2, playerAngle = 0, playerPitch = 0;
let lastPlayerX, lastPlayerY;

let cursorX = null, cursorY = null, cursorLevel, cursorLock = 0;
let lastCursorX = null, lastCursorY = null, lastCursorLevel = null;
let cursorTexture, cursorFloor, cursorWall, cursorType, selectedTexture = 0;
let markerX = null, markerY = null, stepStack = [];

let levelNo = 1;

let quads = [];
let sortedQuads = [];
let map = [];
let kruskalMaze = [];
let rays;

let cam3d;
let cam2d;

let illuminate = false;

p5.disableFriendlyErrors = true;

let inconsolata;
let framerateStack = [];

let undoBuffer = [], dirtyMap = false;

const MAX_UNDOS = 10000;

let toggles = {

  handlePlayInputs: true,
  handlePlayerInteractions: true,
  updateQuads: true,
  evaluateCursor: true,
  background: true,
  renderQuads: true,
  renderOverlay: true,
  mouseMoved: true,
  mouseReleased: true,
  dotProductTest: true

}


function preload() {


  inconsolata = loadFont('fonts/inconsolata.otf');

  for (let i = 0; i < TEXTURE_COUNT; i++) {
    textures.push(loadImage("textures/" + (i+1) + ".jpg"));
  }

  for (let j = 0; j <= MAX_HEIGHT * 2 + 1; j++) {
    let row = [];
    for (let i = 0; i <= MAX_WIDTH * 2 + 1; i++) {
      row.push(new mapCell());
    }
    map.push(row);
  }

  for (let j = 0; j < MAX_HEIGHT; j++) {
    let row = [];
    for (let i = 0; i < MAX_WIDTH; i++) {
      row.push(new kruskalCell(0, false, false));
    }
    kruskalMaze.push(row);
  }

  allMapNames = [];
  for (let i = 0; i < localStorage.length; i++) {
    if (localStorage.key(i).startsWith("#")) continue;
    allMapNames.push(localStorage.key(i));
  }

}

function windowResized() {
  resizeCanvas(windowWidth, windowHeight);
}

function setup() {

  angleMode(RADIANS);

  createCanvas(windowWidth, windowHeight, WEBGL);

  clearMap();
  regenerateQuads();

  cam2d = createCamera();
  cam3d = createCamera();
  setCamera(cam3d);

}

function mouseReleased() {

  if (!toggles.mouseReleased) return;

  requestPointerLock();
}

function mouseMoved() {

  if (!toggles.mouseMoved) return;

  if (cursorLock > 0) cursorLock--;

  if (!keyIsDown(69)) {
    playerAngle -= movedX / 500;
    if (playerAngle > PI) playerAngle -= 2 * PI;
    if (playerAngle < -PI) playerAngle += 2 * PI;
    playerPitch += movedY / 500;
    if (playerPitch > HALF_PI - 0.001) playerPitch = HALF_PI - 0.001;
    if (playerPitch < -HALF_PI + 0.001) playerPitch = -HALF_PI + 0.001;
  }
}

function mouseDragged() {
  mouseMoved();
}

function undo() {

  if (undoBuffer.length === 0) return;

  let f = undoBuffer[undoBuffer.length - 1].frame;

  while (undoBuffer.length > 0 && undoBuffer[undoBuffer.length - 1].frame === f) {
    let q = undoBuffer.pop();
    console.log("BEFORE: " + JSON.stringify(map[q.x][q.y]));
    map[q.x][q.y].type = q.type;
    map[q.x][q.y].flat = [...q.flat];
    map[q.x][q.y].wall = [[...q.wall[0]], [...q.wall[1]], [...q.wall[2]]];
    console.log("AFTER: " + JSON.stringify(map[q.x][q.y]));
  }

  regenerateQuads();

}

function setQuadType(x, y, type) {

  if (map[x][y].type !== type) {

    undoBuffer.push({
      frame: frameCount,
      x: x,
      y: y,
      type: map[x][y].type,
      flat: [...map[x][y].flat],
      wall: [[...map[x][y].wall[0]], [...map[x][y].wall[1]], [...map[x][y].wall[2]]]
    });

    while (undoBuffer.length > MAX_UNDOS) {
      undoBuffer.shift();
    }

    map[x][y].type = type;

    dirtyMap = true;

  }


}

function setQuadTexture(x, y, level, id, wall) {

  let change = false;

  if (x < 0 || y < 0 || x > 2 * mazeWidth || y > 2 * mazeHeight) return;

  let before = {
    frame: frameCount,
    x: x,
    y: y,
    type: map[x][y].type,
    flat: [...map[x][y].flat],
    wall: [[...map[x][y].wall[0]], [...map[x][y].wall[1]], [...map[x][y].wall[2]]]
  };

  if (wall === null) {
    if (level >= 0 && level <= 3) {
      if (map[x][y].flat[level] !== id) {
        map[x][y].flat[level] = id;
        change = true;
      }
    }
  } else {
    if (level >= 0 && level <= 2) {
      if (map[x][y].wall[level][wall] !== id) {
        map[x][y].wall[level][wall] = id;
        change = true;
      }
    }
  }

  if (change) {

    undoBuffer.push(before);

    for (let i = 0; i < quads.length; i++) {
      if (quads[i].mapX == x && quads[i].mapY == y) {
        if (wall !== null) {
          if (quads[i].wall && quads[i].level === level && quads[i].direction === wall) {
            quads[i].texture = id;
          }
        } else {
          if (!quads[i].wall && quads[i].level === level) {
            quads[i].texture = id;
          }
        }
      }
    }

    while (undoBuffer.length > MAX_UNDOS) {
      undoBuffer.shift();
    }

    dirtyMap = true;

  }
}

function quadStruct(p1, p2, p3, p4,
  wall = false, mapX = 0, mapY = 0,
  texture = -1, level = 0, direction = -1) {

  if (p1 === undefined) p1 = [0, 0, 0];
  if (p2 === undefined) p2 = [0, 0, 0];
  if (p3 === undefined) p3 = [0, 0, 0];
  if (p4 === undefined) p4 = [0, 0, 0];

  this.texture = texture;
  this.vertices = [
    { x: p1[0], y: p1[1], z: p1[2] },
    { x: p2[0], y: p2[1], z: p2[2] },
    { x: p3[0], y: p3[1], z: p3[2] },
    { x: p4[0], y: p4[1], z: p4[2] }
  ];

  this.visible = false;

  this.mapX = mapX;
  this.mapY = mapY;

  this.dSquared = 0;

  this.wall = wall;
  this.level = level;
  this.direction = direction;

  this.r = 0;
  this.g = 0;
  this.b = 0;
  this.a = 0;

  this.centre = {
    x: (this.vertices[0].x + this.vertices[1].x + this.vertices[2].x + this.vertices[3].x) / 4,
    y: (this.vertices[0].y + this.vertices[1].y + this.vertices[2].y + this.vertices[3].y) / 4,
    z: (this.vertices[0].z + this.vertices[1].z + this.vertices[2].z + this.vertices[3].z) / 4
  };


}

function clearMap() {
  for (let i = -mazeWidth; i <= mazeWidth; i++) {
    for (let j = -mazeHeight; j <= mazeHeight; j++) {
      if (i == -mazeWidth || j == -mazeWidth || i == mazeWidth ||
        j == mazeWidth) {
        map[i + mazeWidth][j + mazeHeight].type = WALL;
        for (let f = 0; f < 3; f++) {
          for (let d = 0; d < 4; d++) {
            map[i + mazeWidth][j + mazeHeight].wall[f][d] = DEFAULT_WALL;
          }
        }
        for (let f = 0; f < 4; f++) {
          map[i + mazeWidth][j + mazeHeight].flat[f] = DEFAULT_FLAT;
        }
      } else {
        map[i + mazeWidth][j + mazeHeight].type = SKY;
        for (let f = 0; f < 3; f++) {
          for (let d = 0; d < 4; d++) {
            map[i + mazeWidth][j + mazeHeight].wall[f][d] = DEFAULT_WALL;
          }
        }
        for (let f = 0; f < 4; f++) {
          map[i + mazeWidth][j + mazeHeight].flat[f] = DEFAULT_FLAT;
        }
      }
    }
  }
}

function regenerateQuads() {

  quads = [];

  for (let i = -mazeWidth; i <= mazeWidth; i++) {
    for (let j = -mazeHeight; j <= mazeHeight; j++) {
      let x1 = unit * i;
      let y1 = unit * j;
      let x2 = x1 + unit;
      let y2 = y1;
      let x4 = x1;
      let y4 = y1 + unit;
      let x3 = x1 + (x2 - x1) + (x4 - x1);
      let y3 = y1 + (y2 - y1) + (y4 - y1);

      if (map[i + mazeWidth][j + mazeHeight].type & LOW_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type & WALL_BIT)) {  // CORRIDOR CEILING (1, down)

        quads.push(new quadStruct([x4, y4, 0, -1], [x3, y3, 0, -1],
          [x2, y2, 0, -1], [x1, y1, 0, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[1], 1));

      }

      if (map[i + mazeWidth][j + mazeHeight].type & HIGH_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type & LOW_BIT)) {  // LOW ROOM CEILING (2, down)

        quads.push(new quadStruct([x4, y4, -unit, -1], [x3, y3, -unit, -1],
          [x2, y2, -unit, -1], [x1, y1, -unit, -1], false,
          i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[2], 2));

      }

      if (map[i + mazeWidth][j + mazeHeight].type & CEILING_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type & HIGH_BIT)) {  // HIGH ROOM CEILING (3, down)

        quads.push(new quadStruct([x4, y4, -unit * 2, -1], [x3, y3, -unit * 2, -1],
          [x2, y2, -unit * 2, -1], [x1, y1, -unit * 2, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[3], 3));

      }

      if (!(map[i + mazeWidth][j + mazeHeight].type & WALL_BIT)) {  // FLOOR (0, up)

        quads.push(new quadStruct([x1, y1, unit, -1], [x2, y2, unit, -1],
          [x3, y3, unit, -1], [x4, y4, unit, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[0], 0));

      }

      for (let level = 0; level < 3; level++) {
        let LEVEL_BIT;
        let zTop;
        let zBottom;
        switch (level) {
          case 0:
            LEVEL_BIT = WALL_BIT;
            zTop = 0;
            zBottom = unit;
            break;
          case 1:
            LEVEL_BIT = LOW_BIT;
            zTop = -unit;
            zBottom = 0;
            break;
          case 2:
            LEVEL_BIT = HIGH_BIT;
            zTop = -2 * unit;
            zBottom = -unit;
            break;
        }

        if (!(map[i + mazeWidth][j + mazeHeight].type & LEVEL_BIT)) continue;

        if (j > -mazeHeight &&
          !(map[i + mazeWidth][j + mazeHeight - 1].type & LEVEL_BIT)) {
          quads.push(new quadStruct(
            [x2, y2, zBottom, 3], [x2, y2, zTop, 2],
            [x1, y1, zTop, 1], [x1, y1, zBottom, 0], true,
            i + mazeWidth, j + mazeHeight,
            map[i + mazeWidth][j + mazeHeight].wall[level][0], level, 0));
        }

        if (i < mazeWidth - 1 &&
          !(map[i + mazeWidth + 1][j + mazeHeight].type & LEVEL_BIT)) {
          quads.push(new quadStruct(
            [x3, y3, zBottom, 3], [x3, y3, zTop, 2],
            [x2, y2, zTop, 1], [x2, y2, zBottom, 0], true,
            i + mazeWidth, j + mazeHeight,
            map[i + mazeWidth][j + mazeHeight].wall[level][1], level, 1));
        }

        if (j < mazeHeight - 1 &&
          !(map[i + mazeWidth][j + mazeHeight + 1].type & LEVEL_BIT)) {
          quads.push(new quadStruct(
            [x4, y4, zBottom, 3], [x4, y4, zTop, 2],
            [x3, y3, zTop, 1], [x3, y3, zBottom, 0], true,
            i + mazeWidth, j + mazeHeight,
            map[i + mazeWidth][j + mazeHeight].wall[level][2], level, 2));
        }

        if (i > -mazeHeight &&
          !(map[i + mazeWidth - 1][j + mazeHeight].type & LEVEL_BIT)) {
          quads.push(new quadStruct(
            [x1, y1, zBottom, 3], [x1, y1, zTop, 2],
            [x4, y4, zTop, 1], [x4, y4, zBottom, 0], true,
            i + mazeWidth, j + mazeHeight,
            map[i + mazeWidth][j + mazeHeight].wall[level][3], level, 3));
        }
      }
    }
  }
}

function rand() {
  return floor(random(0, 65535));
}

function kruskalCell(set, right, down) {
  this.set = set;
  this.right = right;
  this.down = down;
}

function mapCell() {
  this.type = 0;
  this.flat = [0, 0, 0, 0];
  this.wall = [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]];
}

function kruskalStep() {
  let oneSet = true;
  for (let i = 0; i < mazeWidth; i++) {
    for (let j = 0; j < mazeHeight; j++) {
      if (kruskalMaze[i][j].set != kruskalMaze[0][0].set) {
        oneSet = false;
      }
    }
  }
  if (oneSet) return true;

  let x, y, a, b, horizontal = 0, vertical = 0;

  while (true) {
    x = rand() % mazeWidth;
    y = rand() % mazeHeight;

    if (rand() % 2 == 0) {
      horizontal = 1;
      vertical = 0;
    } else {
      horizontal = 0;
      vertical = 1;
    }

    if (horizontal > 0 && (kruskalMaze[x][y].right || x == mazeWidth - 1))
      continue;
    if (vertical > 0 && (kruskalMaze[x][y].down || y == mazeHeight - 1))
      continue;

    a = kruskalMaze[x][y].set;
    b = kruskalMaze[x + horizontal][y + vertical].set;

    if (a == b) continue;

    if (vertical > 0) {
      kruskalMaze[x][y].down = true;
    } else {
      kruskalMaze[x][y].right = true;
    }
    for (let i = 0; i < mazeWidth; i++) {
      for (let j = 0; j < mazeHeight; j++) {
        if (kruskalMaze[i][j].set == b) {
          kruskalMaze[i][j].set = a;
        }
      }
    }

    return false;
  }
}

function makeMaze() {
  let n = 0;
  for (let i = 0; i < mazeWidth; i++) {
    for (let j = 0; j < mazeHeight; j++) {
      n++;
      kruskalMaze[i][j].set = n;
      kruskalMaze[i][j].right = false;
      kruskalMaze[i][j].down = false;
    }
  }

  let mazeDone = false;
  while (!mazeDone) {
    mazeDone = kruskalStep();
  }

  for (let i = -mazeWidth; i <= mazeWidth; i++) {
    for (let j = -mazeHeight; j <= mazeHeight; j++) {
      map[i + mazeWidth][j + mazeHeight].type =
        (i + mazeWidth) % 2 == 1 && (j + mazeHeight) % 2 == 1
          ? GENERATOR_PATH
          : GENERATOR_WALL;
      for (let f = 0; f < 4; f++) {
        map[i + mazeWidth][j + mazeHeight].flat[f] = DEFAULT_FLAT;
      }
      for (let f = 0; f < 3; f++) {
        for (let d = 0; d < 4; d++) {
          map[i + mazeWidth][j + mazeHeight].wall[f][d] = DEFAULT_WALL;
        }
      }
    }
  }

  for (let i = 0; i < mazeWidth; i++) {
    for (let j = 0; j < mazeHeight; j++) {
      if (kruskalMaze[i][j].right) {
        map[i * 2 + 2][j * 2 + 1].type = GENERATOR_PATH;
        for (let f = 0; f < 4; f++) {
          map[i * 2 + 2][j * 2 + 1].flat[f] = DEFAULT_FLAT;
        }
        for (let f = 0; f < 3; f++) {
          for (let d = 0; d < 4; d++) {
            map[i * 2 + 2][j * 2 + 1].wall[f][d] = DEFAULT_WALL;
          }
        }
      }
      if (kruskalMaze[i][j].down) {
        map[i * 2 + 1][j * 2 + 2].type = GENERATOR_PATH;
        for (let f = 0; f < 4; f++) {
          map[i * 2 + 1][j * 2 + 2].flat[f] = DEFAULT_FLAT;
        }
        for (let f = 0; f < 3; f++) {
          for (let d = 0; d < 4; d++) {
            map[i * 2 + 1][j * 2 + 2].wall[f][d] = DEFAULT_WALL;
          }
        }
      }
    }
  }

  for (let r = 0; r < 10; r++) {
    let rw = rand() % 10 + 5;
    let rh = rand() % 10 + 5;
    let x = rand() % (mazeWidth * 2 - rw);
    let y = rand() % (mazeHeight * 2 - rh);
    for (let i = x; i <= x + rw; i++) {
      for (let j = y; j <= y + rh; j++) {
        map[i][j].type = GENERATOR_ROOM;
        for (let f = 0; f < 4; f++) {
          map[i][j].flat[f] = DEFAULT_FLAT;
        }
        for (let f = 0; f < 3; f++) {
          for (let d = 0; d < 4; d++) {
            map[i][j].wall[f][d] = DEFAULT_WALL;
          }
        }
      }
    }
  }
}

function handlePlayInputs(frameLength) {

  lastPlayerX = playerX;
  lastPlayerY = playerY;

  if (keyIsDown(LEFT_ARROW)) {
    playerAngle += 4 * frameLength;
    if (playerAngle > PI) playerAngle -= 2 * PI;
    //console.log(playerX, playerY, playerZ, playerAngle);

  }
  if (keyIsDown(RIGHT_ARROW)) {
    playerAngle -= 4 * frameLength;
    if (playerAngle < -PI) playerAngle += 2 * PI;
    //console.log(playerX, playerY, playerZ, playerAngle);
  }

  if (keyIsDown(DOWN_ARROW)) {
    playerPitch += 4 * frameLength;
    if (playerPitch > HALF_PI - 0.001) playerPitch = HALF_PI - 0.001;
    //console.log(playerX, playerY, playerZ, playerAngle);

  }
  if (keyIsDown(UP_ARROW)) {
    playerPitch -= 4 * frameLength;
    if (playerPitch < -HALF_PI + 0.001) playerPitch = -HALF_PI + 0.001;
    //console.log(playerX, playerY, playerZ, playerAngle);
  }

  if (keyIsDown(87)) { //W
    playerX += sin(playerAngle) * unit * 4 * frameLength;
    playerY += cos(playerAngle) * unit * 4 * frameLength;
    //console.log(playerX, playerY, playerZ);
  }
  if (keyIsDown(83)) { //S
    playerX -= sin(playerAngle) * unit * 4 * frameLength;
    playerY -= cos(playerAngle) * unit * 4 * frameLength;
    //console.log(playerX, playerY, playerZ);
  }
  if (keyIsDown(65)) { //A
    playerX += cos(playerAngle) * unit * 4 * frameLength;
    playerY += -sin(playerAngle) * unit * 4 * frameLength;
    //console.log(playerX, playerY, playerZ);
  }
  if (keyIsDown(68)) { //D
    playerX -= cos(playerAngle) * unit * 4 * frameLength;
    playerY -= -sin(playerAngle) * unit * 4 * frameLength;
    //console.log(playerX, playerY, playerZ);
  }

  if (keyIsDown(82)) { //R
    playerZ -= unit * 4 * frameLength;
    //console.log(playerX, playerY, playerZ);
  }
  if (keyIsDown(70)) { //F
    playerZ = unit / 2;
    //console.log(playerX, playerY, playerZ);
  }

  let distanceTravelled = dist(playerX, playerY, lastPlayerX, lastPlayerY);
  if (distanceTravelled > unit / 2) {
    let dx = (playerX - lastPlayerX) / distanceTravelled;
    let dy = (playerY - lastPlayerY) / distanceTravelled;
    playerX = lastPlayerX + dx * unit / 2;
    playerY = lastPlayerY + dy * unit / 2;
  }

  if (keyIsDown(32)) { // space

    if (cursorX === lastCursorX && cursorY === lastCursorY && cursorLevel === lastCursorLevel) return;

    lastCursorX = cursorX;
    lastCursorY = cursorY;
    lastCursorLevel = cursorLevel;

    if (markerY !== null && markerY !== null) {
      let x0 = min(cursorX, markerX);
      let y0 = min(cursorY, markerY);
      let x1 = max(cursorX, markerX) + 1;
      let y1 = max(cursorY, markerY) + 1;
      for (let x = x0; x < x1; x++) {
        for (let y = y0; y < y1; y++) {
          if (keyIsDown(CONTROL)) {
            for (let l = 0; l < 4; l++) {
              setQuadTexture(x, y, l, selectedTexture, null);
              if (l === 3) break;
              for (let d = 0; d < 4; d++) {
                setQuadTexture(x, y, l, selectedTexture, d);
              }
            }
          } else {
            setQuadTexture(x, y, cursorLevel, selectedTexture, cursorWall);
          }
        }
      }

    } else {
      if (keyIsDown(CONTROL)) {
        for (let l = 0; l < 4; l++) {
          setQuadTexture(cursorX, cursorY, l, selectedTexture, null);
          if (l === 3) break;
          for (let d = 0; d < 4; d++) {
            setQuadTexture(cursorX, cursorY, l, selectedTexture, d);
          }
        }
      } else {
        setQuadTexture(cursorX, cursorY, cursorLevel, selectedTexture, cursorWall);
      }
    }

  }


  if (keyIsDown(81)) { // q
    if (cursorTexture !== null) selectedTexture = cursorTexture;
  }

  if (mouseIsPressed) {

    if (keyIsDown(69)) { // e

      if (selectedTexture < 0) selectedTexture = 0;
      if (selectedTexture > TEXTURE_COUNT - 1) selectedTexture = TEXTURE_COUNT - 1;

    }

  }

}

function mouseWheel(event) {

  if (cursorX === null || cursorY === null) return;

  let x0, y0, x1, y1;

  if (markerY !== null && markerY !== null) {
    x0 = min(cursorX, markerX);
    y0 = min(cursorY, markerY);
    x1 = max(cursorX, markerX) + 1;
    y1 = max(cursorY, markerY) + 1;

  } else {

    x0 = cursorX;
    y0 = cursorY;
    x1 = cursorX + 1;
    y1 = cursorY + 1;

  }

  if (event.delta > 0) {

    for (let x = x0; x < x1; x++) {
      for (let y = y0; y < y1; y++) {

        switch (map[x][y].type) {
          case WALL:
            break;
          case HIGH_ROOM:
            setQuadType(x, y, LOW_ROOM);
            break;
          case CORRIDOR:
            setQuadType(x, y, WALL);
            break;
          case LOW_ROOM:
            setQuadType(x, y, CORRIDOR);
            break;
          default:
            setQuadType(x, y, HIGH_ROOM);
        }

      }
    }

  } else if (event.delta < 0) {

    for (let x = x0; x < x1; x++) {
      for (let y = y0; y < y1; y++) {

        switch (map[x][y].type) {
          case HIGH_ROOM:
            break;
          case WALL:
            setQuadType(x, y, CORRIDOR);
            break;
          case CORRIDOR:
            setQuadType(x, y, LOW_ROOM);
            break;
          case LOW_ROOM:
            setQuadType(x, y, HIGH_ROOM);
            break;
          default:
            setQuadType(x, y, SKY);
        }

      }

    }

  }

  regenerateQuads();

  cursorLock = 3;

}

function handlePlayerInteractions() {

  rays = [];

  let mapXstart = floor(lastPlayerX / unit) + mazeWidth;
  let mapYstart = floor(lastPlayerY / unit) + mazeHeight;

  let mapXtarget = floor(playerX / unit) + mazeWidth;
  let mapYtarget = floor(playerY / unit) + mazeHeight;

  let lowestX = min(mapXstart, mapXtarget) - 1;
  let highestX = max(mapXstart, mapXtarget) + 1;
  let lowestY = min(mapYstart, mapYtarget) - 1;
  let highestY = max(mapYstart, mapYtarget) + 1;

  let mapZ = 0;
  if (playerZ < -5 * unit / 2 || playerZ > unit / 2)
    return;
  else if (playerZ < -3 * unit / 2)
    mapZ = 2;
  else if (playerZ < -unit / 2)
    mapZ = 1;

  let INTERACTION_BIT;
  switch (mapZ) {
    case 0:
      INTERACTION_BIT = WALL_BIT;
      break;
    case 1:
      INTERACTION_BIT = LOW_BIT;
      break;
    case 2:
      INTERACTION_BIT = HIGH_BIT;
      break;
  }

  if (map[mapXstart] === undefined ||
    map[mapXstart][mapYstart] === undefined ||
    map[mapXstart][mapYstart].type & INTERACTION_BIT) return;

  for (let i = lowestX; i <= highestX; i++) {
    for (let j = lowestY; j <= highestY; j++) {

      if (i >= 0 && j >= 0 &&
        i < mazeWidth * 2 + 1 && j < mazeWidth * 2 + 1) {

        if (map[i][j].type & INTERACTION_BIT) {

          let nearestPoint = {
            x: max((i - mazeWidth) * unit, min(playerX, ((i - mazeWidth) + 1) * unit)),
            y: max((j - mazeHeight) * unit, min(playerY, ((j - mazeHeight) + 1) * unit))
          };

          let ray = {
            x: nearestPoint.x - playerX,
            y: nearestPoint.y - playerY,
            i: i,
            j: j,
            length: 0,
            overlap: 0
          };

          ray.length = mag(ray.x, ray.y);
          if (isNaN(ray.length) || ray.length == 0) continue;

          ray.overlap = unit * 0.5 - ray.length;
          if (isNaN(ray.overlap)) continue;

          rays.push(ray);

          if (ray.overlap > 0) {

            playerX -= ray.overlap * ray.x / ray.length;
            playerY -= ray.overlap * ray.y / ray.length;

          }

        }
      }


    }
  }


}

function updateQuads() {

  const dx = sin(playerAngle) * cos(playerPitch);
  const dy = cos(playerAngle) * cos(playerPitch);
  const dz = sin(playerPitch);

  for (let quadCounter = 0; quadCounter < quads.length; quadCounter++) {

    let q = quads[quadCounter];

    let nx = q.centre.x - playerX;
    let ny = q.centre.y - playerY;
    let nz = q.centre.z - playerZ;
    let nd;

    if (abs(nx) > drawDistance || abs(ny) > drawDistance) {
      q.visible = false;
      continue;
    }

    q.dSquared = pow(nx, 2) + pow(ny, 2) + pow(nz, 2);

    nd = sqrt(q.dSquared);

    if (toggles.dotProductTest && !(nd < 2 * unit)) {

      nx /= nd;
      ny /= nd;
      nz /= nd;

      let dotProduct = dx * nx + dy * ny + dz * nz;

      if (dotProduct < 0) {
        q.visible = false;
        continue;
      }

    }

    q.visible = true;

    q.r = 0;
    q.g = 0;
    q.b = 0;
    q.a = 255;

    if (cursorX === q.mapX && cursorY === q.mapY) {
      q.r = 255;
      q.g = 255;
      q.b = 255;
      continue;
    }

    let d = 255 * (1 - nd / drawDistance);

    if (illuminate) d *= 10;

    if (d > 255) d = 255;
    if (d <= 0) continue;

    if (illuminate) {
      q.r = 255;
      q.g = 255;
      q.b = 255;
      q.a = d;
    } else {
      q.r = d;
      q.g = d;
      q.b = d;
      q.a = 255;
    }

  }


}

function renderOverlay() {

  const w = windowWidth;
  const h = windowHeight;
  const size = 8;

  const mapX = playerX / unit + mazeWidth;
  const mapY = playerY / unit + mazeHeight;

  setCamera(cam2d);
  ortho();

  textFont(inconsolata);

  if (framerateStack.length > 30) framerateStack.shift();
  framerateStack.push(frameRate());
  let averageFPS = 0;
  for (let f of framerateStack) averageFPS += f;
  averageFPS /= framerateStack.length;

  textAlign(RIGHT, TOP);
  textSize(18);
  fill(255, 255, 255);
  text(floor(averageFPS) + " FPS", w / 2 - 5, -h / 4 + 5);
  if (keyIsDown(84)) {
    let ty = 1;
    for (let t of Object.keys(toggles)) {
      if (toggles[t]) {
        fill(0, 128, 0);
        text("[" + ty + "] " + t + " on", w / 2 - 5, -h / 4 + ty * 15 + 10);
      } else {
        fill(255, 0, 0);
        text("[" + ty + "] " + t + " off", w / 2 - 5, -h / 4 + ty * 15 + 10);
      }
      ty++;
    }
  } else if (undoBuffer.length > 0) {
    fill(0, 0, 255);
    text("Undo buffer: " + undoBuffer.length + " / " + MAX_UNDOS, w / 2 - 5, -h / 4 + 25);
  }

  textAlign(LEFT, TOP);
  textSize(18);

  if (dirtyMap) {
    fill(255, 0, 0);
  } else {
    fill(0, 255, 0);
  }

  if (mapName !== null) {
    text(mapName, -w / 2, -h / 4 + 5);
  } else {

    text("{untitled}", -w / 2, -h / 4 + 5);
  }


  let m = -1;
  textSize(14);
  for (let n of allMapNames) {
    m++;
    if (selectedMap === m) {
      fill(255, 255, 0);
    } else {
      fill(128, 128, 128);
    }
    text(n, -w / 2, -h / 4 + m * 15 + 25);
  }

  stroke(255, 255, 255);
  line(-20, 0, 20, 0);
  line(0, -20, 0, 20);

  push();

  textureMode(NORMAL);

  if (keyIsDown(69)) { // e

    let size = floor(h / 16);

    for (let i = 0; i < 16; i++) {
      for (let j = 0; j < 16; j++) {
        if (i + j * 16 > TEXTURE_COUNT - 1) continue;
        texture(textures[i + j * 16]);
        rect((i - 7) * size + 5, (j - 7) * size + 5, size - 10, size - 10);
        if (mouseX > w / 2 + (i - 7) * size + 5 &&
          mouseY > h / 2 + (j - 7) * size + 5 &&
          mouseX < w / 2 + (i - 7) * size + (size - 5) &&
          mouseY < h / 2 + (j - 7) * size + (size - 5)) {
          selectedTexture = i + j * 16;
        }
      }
    }

  }

  texture(textures[selectedTexture]);
  rect(w / 2 - 100, -h / 2 + 20, 80, 80);


  pop();

  noStroke();

  push();

  translate(size * (drawDistance / unit) - w / 2 - mapX * size, size * (drawDistance / unit) - h / 2 - mapY * size);

  for (let i = 0; i < mazeWidth * 2 + 1; i++) {
    for (let j = 0; j < mazeHeight * 2 + 1; j++) {

      if (abs(mapX - i) > drawDistance / unit || abs(mapY - j) > drawDistance / unit) continue;

      let alpha = 128;
      let alpha1 = 128;
      let alpha2 = 128;

      if (abs(mapX - i) > 0.8 * drawDistance / unit) alpha1 = 128 * (1 - (abs(mapX - i) - 0.8 * drawDistance / unit) / (0.2 * drawDistance / unit));
      if (abs(mapY - j) > 0.8 * drawDistance / unit) alpha2 = 128 * (1 - (abs(mapY - j) - 0.8 * drawDistance / unit) / (0.2 * drawDistance / unit));

      if (alpha1 < alpha) alpha = alpha1;
      if (alpha2 < alpha) alpha = alpha2;

      for (let ray of rays) {
        if (ray.i == i && ray.j == j) {
          alpha = 192;
          break;
        }
      }

      if (map[i][j].type & WALL_BIT) {
        if (!(map[i][j].type & HIGH_BIT)) {
          if (!(map[i][j].type & LOW_BIT)) {
            fill(255, 255, 0, alpha);
            rect(i * size, j * size, size, size);
          } else {
            fill(255, 0, 255, alpha);
            rect(i * size, j * size, size, size);
          }
        } else {
          fill(255, 255, 255, alpha);
          rect(i * size, j * size, size, size);
        }
      } else {
        if (!(map[i][j].type & HIGH_BIT)) {
          fill(0, 0, 64, alpha);
          rect(i * size, j * size, size, size);
        } else if (!(map[i][j].type & LOW_BIT)) {
          fill(0, 0, 128, alpha)
          rect(i * size, j * size, size, size);
        } else {
          fill(0, 0, 255, alpha);
          rect(i * size, j * size, size, size);
        }
      }
    }
  }

  fill(255, 0, 0, 255);
  stroke(255, 0, 0, 255);

  rect(mapX * size - size / 3, mapY * size - size / 3, size * (2 / 3), size * (2 / 3));

  line(mapX * size, mapY * size,
    mapX * size +
    size * 2 * sin(playerAngle),
    mapY * size +
    size * 2 * cos(playerAngle)
  );

  pop();

}

function renderQuads() {

  push();

  setCamera(cam3d);
  perspective(PI / 3, windowWidth / windowHeight, 0.01, drawDistance * 2);
  cam3d.setPosition(playerX, playerZ, playerY);
  cam3d.lookAt(playerX + sin(playerAngle) * cos(playerPitch), playerZ + sin(playerPitch), playerY + cos(playerAngle) * cos(playerPitch));
  noStroke();
  textureMode(NORMAL);

  let lastTexture = null;

  for (let q of quads) {

    if (!q.visible) continue;

    if (q.texture != lastTexture) {
      lastTexture = q.texture;
      texture(textures[q.texture]);
    }

    tint(q.r, q.g, q.b, q.a);

    beginShape();

    vertex(q.vertices[0].x, q.vertices[0].z, q.vertices[0].y, 0, 1);
    vertex(q.vertices[1].x, q.vertices[1].z, q.vertices[1].y, 0, 0);
    vertex(q.vertices[2].x, q.vertices[2].z, q.vertices[2].y, 1, 0);
    vertex(q.vertices[3].x, q.vertices[3].z, q.vertices[3].y, 1, 1);

    endShape();

  }

  if (markerX !== null && markerY !== null) {
    for (let z = 1; z >= -2; z -= 0.25) {

      let x0 = min(cursorX, markerX);
      let y0 = min(cursorY, markerY);
      let x1 = max(cursorX, markerX) + 1;
      let y1 = max(cursorY, markerY) + 1;

      push();
      translate((x0 - mazeWidth) * unit, z * unit, (y0 - mazeHeight) * unit);
      fill(0, 255, 0);
      sphere(2);
      pop();

      push();
      translate((x1 - mazeWidth) * unit, z * unit, (y0 - mazeHeight) * unit);
      fill(0, 255, 0);
      sphere(2);
      pop();

      push();
      translate((x1 - mazeWidth) * unit, z * unit, (y1 - mazeHeight) * unit);
      fill(0, 255, 0);
      sphere(2);
      pop();

      push();
      translate((x0 - mazeWidth) * unit, z * unit, (y1 - mazeHeight) * unit);
      fill(0, 255, 0);
      sphere(2);
      pop();

    }
  }

  for (let step of stepStack) {
    push();
    translate((step.x - mazeWidth) * unit, step.z * unit, (step.y - mazeHeight) * unit);
    if (step.cursor) {
      fill(255, 0, 0);
    } else {
      fill(255, 255, 0);
    }
    sphere(3);
    pop();
  }

  pop();

}

function evaluateCursor() {

  if (playerZ !== unit / 2) {
    cursorX = null;
    cursorY = null;
    return;
  }

  if (cursorLock > 0) return;

  cursorX = null;
  cursorY = null;
  cursorLevel = null;
  cursorWall = null;

  let storeStack = keyIsDown(80);

  if (storeStack) {
    stepStack = [];
  }

  const dx = sin(playerAngle) * cos(playerPitch);
  const dy = cos(playerAngle) * cos(playerPitch);
  const dz = sin(playerPitch);

  let stepX = playerX / unit + mazeWidth;
  let stepY = playerY / unit + mazeHeight;
  let stepZ = playerZ / unit;

  if (storeStack) {
    stepStack.push({ x: stepX, y: stepY, z: stepZ });
  }

  for (let steps = 0; steps < 100; steps++) {

    let toXedge = Infinity, toYedge = Infinity, toZedge = Infinity;

    if (fract(stepX) === 0) {
      toXedge = 1 / abs(dx);
    } else {
      if (dx > 0) {
        toXedge = (1 - fract(stepX)) / dx;
      } else if (dx < 0) {
        toXedge = -fract(stepX) / dx;
      }
    }

    if (fract(stepY) === 0) {
      toYedge = 1 / abs(dy);
    } else {
      if (dy > 0) {
        toYedge = (1 - fract(stepY)) / dy;
      } else if (dy < 0) {
        toYedge = -fract(stepY) / dy;
      }
    }

    if (fract(stepZ) === 0) {
      toZedge = 1 / abs(dz);
    } else {
      if (dz > 0) {
        toZedge = (1 - fract(stepZ)) / dz;
      } else if (dz < 0) {
        toZedge = -fract(stepZ) / dz;
      }
    }

    if (toXedge === Infinity && toYedge === Infinity && toZedge === Infinity) break;

    if (toXedge <= toYedge && toXedge <= toZedge) {

      stepX += dx * toXedge;
      stepY += dy * toXedge;
      stepZ += dz * toXedge;

    } else if (toYedge <= toXedge && toYedge <= toZedge) {

      stepX += dx * toYedge;
      stepY += dy * toYedge;
      stepZ += dz * toYedge;

    } else if (toZedge <= toXedge && toZedge <= toYedge) {

      stepX += dx * toZedge;
      stepY += dy * toZedge;
      stepZ += dz * toZedge;

    }

    if (stepX < 0 || stepX > MAX_WIDTH * 2 + 1 ||
      stepY < 0 || stepY > MAX_HEIGHT * 2 + 1) break;

    let mapZ = floor(stepZ + dz * 0.001);

    let INTERACTION_BIT;
    let level = null;

    if (mapZ > 0) {
      INTERACTION_BIT = GROUND_BIT;
      level = 0;
    } else if (mapZ == 0) {
      INTERACTION_BIT = WALL_BIT;
      level = 0;
    } else if (mapZ == -1) {
      INTERACTION_BIT = LOW_BIT;
      level = 1;
    } else if (mapZ == -2) {
      INTERACTION_BIT = HIGH_BIT;
      level = 2;
    } else {
      INTERACTION_BIT = CEILING_BIT;
      level = 3;
    }

    let mapX = floor(stepX + dx * 0.001);
    let mapY = floor(stepY + dy * 0.001);

    if (map[mapX] === undefined) break;
    if (map[mapX][mapY] === undefined) break;

    let isCursor = false;
    if (map[mapX][mapY].type & INTERACTION_BIT || INTERACTION_BIT === CEILING_BIT) {
      if (cursorX === null && cursorY === null) {
        cursorX = mapX;
        cursorY = mapY;
        cursorLevel = level;
        cursorType = map[mapX][mapY].type;

        if (fract(stepZ) !== 0) {
          if (fract(stepX) === 0) {
            if (dx > 0) {
              cursorWall = 3;
            } else if (dx < 0) {
              cursorWall = 1;
            }
          } else if (fract(stepY) === 0) {
            if (dy > 0) {
              cursorWall = 0;
            } else if (dy < 0) {
              cursorWall = 2;
            }
          }
        }

        if (level !== null) {
          if (cursorWall !== null && level >= 0 && level <= 2) {
            cursorTexture = map[mapX][mapY].wall[level][cursorWall];
          } else if (level >= 0 && level <= 3) {
            cursorTexture = map[mapX][mapY].flat[level];
          }
        }

        isCursor = true;
      }
    }

    if (storeStack) {
      stepStack.push({ x: stepX, y: stepY, z: stepZ, cursor: isCursor });
    }

  }

}

function keyReleased() {
  if (keyCode === 69) { // e
    requestPointerLock();
    return
  }
}

function keyPressed() {

  if (keyCode === 69) { // e
    exitPointerLock();
    return
  }

  if (keyCode === 90 && keyIsDown(CONTROL)) { // z    
    undo();
    return;
  }

  if (cursorX !== null && cursorY !== null) {

    let changeType = null;

    switch (keyCode) {

      case 33: // page up

        if (allMapNames.length > 0) {
          selectedMap = selectedMap === -1 ? allMapNames.length - 1 : selectedMap - 1;
        }

        break;

      case 34: // page down

        if (allMapNames.length > 0) {
          selectedMap = selectedMap === -1 ? 0 : selectedMap + 1;
          if (selectedMap === allMapNames.length) selectedMap = -1;
        }

        break;

      case 189: // -
        drawDistance -= unit;
        if (drawDistance < unit * 5) drawDistance = unit * 5;
        break;
      case 187: // =
        drawDistance += unit;
        if (drawDistance > (mazeWidth / 2) * unit) drawDistance = (mazeWidth / 2) * unit;
        break;

      case 73:
        if (keyIsDown(ALT)) {
          illuminate = !illuminate; ``
        }
        break;

      case 78: // n
        if (keyIsDown(ALT)) {

          if (dirtyMap) {
            let answer = confirm("Discard changes to?");
            if (!answer) return;
          }

          selectedMap = -1;
          removeItem("#mapname");
          mapName = null;
          clearMap();
          regenerateQuads();
          dirtyMap = false;
          undoBuffer = [];
        }
        break;

      case 77: // m
        if (keyIsDown(ALT)) {

          if (dirtyMap) {
            let answer = confirm("Discard changes?");
            if (!answer) return;
          }

          selectedMap = -1;
          removeItem("#mapname");
          mapName = null;
          makeMaze();
          regenerateQuads();
          dirtyMap = false;
          undoBuffer = [];
        }
        break;

      case 83: // s
        if (keyIsDown(ALT)) {
          let candidateMapName;
          if (selectedMap > -1) {
            candidateMapName = allMapNames[selectedMap];
            if (mapName !== null && mapName !== candidateMapName) {
              let answer = confirm("Overwrite " + candidateMapName + "?");
              if (!answer) return;
            }
          } else if (mapName !== null) {
            candidateMapName = mapName;
          } else {
            candidateMapName = prompt("Enter map name", mapName === null ? "" : mapName);
          }

          if (candidateMapName !== null) {
            mapName = candidateMapName;
            selectedMap = -1;
            localStorage.setItem("#mapname", mapName);
            let data = LZString.compress(JSON.stringify(map));
            localStorage.setItem(mapName, data);
            allMapNames = [];
            for (let i = 0; i < localStorage.length; i++) {
              if (localStorage.key(i).startsWith("#")) continue;
              allMapNames.push(localStorage.key(i));
            }
            dirtyMap = false;
          }

        }
        break;

      case 76: // l
        if (keyIsDown(ALT)) {

          let candidateMapName;
          if (selectedMap > -1) {
            candidateMapName = allMapNames[selectedMap];
          } else if (mapName !== null) {
            candidateMapName = mapName;
          } else {
            candidateMapName = prompt("Enter map name", mapName === null ? "" : mapName);
          }

          if (dirtyMap) {
            let answer = confirm("Discard changes to " + candidateMapName + "?");
            if (!answer) return;
          }

          if (candidateMapName !== null) {
            let candidateMap = localStorage.getItem(candidateMapName);
            if (candidateMap !== null) {
              mapName = candidateMapName;
              selectedMap = -1;
              localStorage.setItem("#mapname", mapName);
              let data = localStorage.getItem(mapName);
              map = JSON.parse(LZString.decompress(data));
              regenerateQuads();
              dirtyMap = false;
              undoBuffer = [];
            } else {
              alert("No map with that name");
            }
          }

        }
        break;

      case 68: // d

        if (keyIsDown(ALT)) {

          let candidateMapName;
          if (selectedMap > -1) {
            candidateMapName = allMapNames[selectedMap];
          } else if (mapName !== null) {
            candidateMapName = mapName;
          }

          if (candidateMapName !== null) {
            let answer = confirm("Are you sure you want to delete " + candidateMapName + "?");
            if (!answer) return;

            localStorage.removeItem(candidateMapName);

            allMapNames = [];
            for (let i = 0; i < localStorage.length; i++) {
              if (localStorage.key(i).startsWith("#")) continue;
              allMapNames.push(localStorage.key(i));
            }

            if (mapName == candidateMapName) mapName = null;

            dirtyMap = true;
            selectedMap = -1;

          }

        }

        break;

      case SHIFT:
        if (markerY !== null || markerY !== null) {
          markerX = null;
          markerY = null;
        } else {
          markerX = cursorX;
          markerY = cursorY;
        }
        break;
      case 49: // 1      
        if (keyIsDown(84)) {
          toggles.handlePlayInputs = !toggles.handlePlayInputs;
        } else {
          changeType = SKY;
        }
        break;
      case 50: // 2        
        if (keyIsDown(84)) {
          toggles.handlePlayerInteractions = !toggles.handlePlayerInteractions;
        } else {
          changeType = SKY_SINGLE_BLOCK;
        }
        break;
      case 51: // 3
        if (keyIsDown(84)) {
          toggles.updateQuads = !toggles.updateQuads;
        } else {
          changeType = SKY_DOUBLE_BLOCK;
        }
        break;
      case 52: // 4
        if (keyIsDown(84)) {
          toggles.evaluateCursor = !toggles.evaluateCursor;
        } else {
          changeType = WALL;
        }
        break;
      case 53: // 5
        if (keyIsDown(84)) {
          toggles.background = !toggles.background;
        } else {
          changeType = CORRIDOR;
        }
        break;
      case 54: // 6
        if (keyIsDown(84)) {
          toggles.renderQuads = !toggles.renderQuads;
        } else {
          changeType = LOW_ROOM;
        }
        break;
      case 55: // 7
        if (keyIsDown(84)) {
          toggles.renderOverlay = !toggles.renderOverlay;
        } else {
          changeType = HIGH_ROOM;
        }
        break;
      case 56: // 8
        if (keyIsDown(84)) {
          toggles.mouseMoved = !toggles.mouseMoved;
        }
        break;
      case 57: // 9        
        if (keyIsDown(84)) {
          toggles.mouseReleased = !toggles.mouseReleased;
        }
        break;
      case 48: // 0        
        if (keyIsDown(84)) {
          toggles.dotProductTest = !toggles.dotProductTest;
        }
        break;

    }

    if (changeType !== null) {
      if (markerY !== null && markerY !== null) {
        let x0 = min(cursorX, markerX);
        let y0 = min(cursorY, markerY);
        let x1 = max(cursorX, markerX) + 1;
        let y1 = max(cursorY, markerY) + 1;
        for (let x = x0; x < x1; x++) {
          for (let y = y0; y < y1; y++) {
            setQuadType(x, y, changeType);
          }
        }
      } else {
        setQuadType(cursorX, cursorY, changeType);
      }
      regenerateQuads();
    }

  }

}

function draw() {

  if (toggles.handlePlayInputs) handlePlayInputs(deltaTime / 1000);
  if (toggles.handlePlayerInteractions) handlePlayerInteractions();

  if (toggles.updateQuads) updateQuads();
  if (toggles.evaluateCursor) evaluateCursor();

  if (toggles.background) background(0, 0, 0);

  if (toggles.renderQuads) renderQuads();

  if (toggles.renderOverlay) renderOverlay();

}