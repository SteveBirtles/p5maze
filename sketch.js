"use strict";

const TILE_SIZE = 64;
const SPRITE_SIZE = 64;
const OMEGA = 0.1;

const DEFAULT_FLAT = 97;
const DEFAULT_WALL = 154;

const CEILING_BIT = 0b10000;
const HIGH_BIT = 0b01000;
const LOW_BIT = 0b00100;
const WALL_BIT = 0b00010;

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

const GENERATOR_WALL = SKY_SINGLE_BLOCK;
const GENERATOR_PATH = SKY;
const GENERATOR_ROOM = SKY;

const textures = [];

const unit = 100;
const MAX_WIDTH = 100;
const MAX_HEIGHT = 100;
let mazeWidth = 40;
let mazeHeight = 40;
let drawDistance = 1000;
const MAX_CLIPBOARD_SIZE = 100;

let cameraAngle = 0;
let cameraPitch = 0;
let cameraX = unit / 2;
let cameraY = unit / 2;
let cameraZ = unit / 2;

let playerX = 0, playerY = 0, playerZ = 0, playerAngle;
let lastPlayerX, lastPlayerY;

let levelNo = 1;

let quads = [];
let sortedQuads = [];

function updateMatrix() {
  const m1 = [[1, 0, 0],
  [0, cos(cameraPitch), sin(cameraPitch)],
  [0, -sin(cameraPitch), cos(cameraPitch)]];

  const m2 = [[cos(cameraAngle), -sin(cameraAngle), 0],
  [sin(cameraAngle), cos(cameraAngle), 0],
  [0, 0, 1]];

  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      m[i][j] =
        m1[i][0] * m2[0][j] + m1[i][1] * m2[1][j] + m1[i][2] * m2[2][j];
    }
  }
}


function playProcessing() {
  cameraX = playerX;
  cameraY = playerY;
  cameraZ = playerZ;
  cameraAngle = playerAngle;
}

function preload() {

  for (let i = 0; i < 176; i++) {
    textures.push(loadImage("tiles/tile" + i + ".png"));
  }

}

function windowResized() {
  resizeCanvas(windowWidth, windowHeight);
}

let cam3d;
let cam2d;

p5.disableFriendlyErrors = true;

function setup() {

  angleMode(RADIANS);

  createCanvas(windowWidth, windowHeight, WEBGL);

  makeMaze();
  regenerateQuads();

  playerX = cameraX;
  playerY = cameraY;
  playerZ = cameraZ;
  playerAngle = cameraAngle;

  cameraPitch = 0;

  cam2d = createCamera();
  cam3d = createCamera();
  setCamera(cam3d);

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



function clearQuads(x, y) {
  let clearCount = 0;
  for (let i = 0; i < quads.size();) {
    if (quads[i].mapX == x && quads[i].mapY == y) {
      quads.erase(quads.begin() + i);
      clearCount++;
    } else {
      i++;
    }
  }
  return clearCount;
}

function setQuadTexture(x, y, id, wall, floor = false) {
  if (x < 0 || y < 0 || x > 2 * mazeWidth || y > 2 * mazeWidth) return 0;
  let setCount = 0;
  for (let i = 0; i < quads.size(); i++) {
    if (quads[i].mapX == x && quads[i].mapY == y) {
      if (wall == quads[i].wall) {
        if (!wall &&
          (floor && quads[i].level != 0 || !floor && quads[i].level == 0))
          continue;
        quads[i].renderable = texture[id];
        setCount++;
      }
    }
  }

  if (wall) {
    for (let f = 0; f < 3; f++) {
      for (let d = 0; d < 4; d++) {
        map[x][y].wall[f][d] = id;
      }
    }
  } else {
    for (let f = 0; f < 4; f++) {
      if (floor && f > 0) break;
      if (!floor && f == 0) continue;
      map[x][y].flat[f] = id;
    }
  }

  return setCount;
}

class quadStruct {
  constructor(p1, p2, p3, p4,
    wall = false, mapX = 0, mapY = 0,
    texture = -1, level = 0, direction = -1) {

    if (p1 === undefined) p1 = [0, 0, 0];
    if (p2 === undefined) p2 = [0, 0, 0];
    if (p3 === undefined) p3 = [0, 0, 0];
    if (p4 === undefined) p4 = [0, 0, 0];

    this.texture = texture;;
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

    this.centre = {
      x: (this.vertices[0].x + this.vertices[1].x + this.vertices[2].x + this.vertices[3].x) / 4,
      y: (this.vertices[0].y + this.vertices[1].y + this.vertices[2].y + this.vertices[3].y) / 4,
      z: (this.vertices[0].z + this.vertices[1].z + this.vertices[2].z + this.vertices[3].z) / 4
    };


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

      if (map[i + mazeWidth][j + mazeHeight].type &
        CEILING_BIT) {  // ROOF (3, up)
        quads.push(new quadStruct([x1, y1, -unit * 2, -1], [x2, y2, -unit * 2, -1],
          [x3, y3, -unit * 2, -1], [x4, y4, -unit * 2, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[3], 3));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & LOW_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type &
          HIGH_BIT)) {  // DOUBLE BLOCK TOP (2, up)
        quads.push(new quadStruct([x1, y1, -unit, -1], [x2, y2, -unit, -1],
          [x3, y3, -unit, -1], [x4, y4, -unit, -1], false,
          i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[2], 2));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & WALL_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type &
          LOW_BIT)) {  // SINGLE BLOCK TOP (1, up)
        quads.push(new quadStruct([x1, y1, 0, -1], [x2, y2, 0, -1],
          [x3, y3, 0, -1], [x4, y4, 0, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[1], 1));
      }

      if (!(map[i + mazeWidth][j + mazeHeight].type &
        WALL_BIT)) {  // FLOOR (0, up)
        quads.push(new quadStruct([x1, y1, unit, -1], [x2, y2, unit, -1],
          [x3, y3, unit, -1], [x4, y4, unit, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[0], 0));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & LOW_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type &
          WALL_BIT)) {  // CORRIDOR CEILING (1, down)
        quads.push(new quadStruct([x4, y4, 0, -1], [x3, y3, 0, -1],
          [x2, y2, 0, -1], [x1, y1, 0, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[1], 1));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & HIGH_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type &
          LOW_BIT)) {  // LOW ROOM CEILING (2, down)
        quads.push(new quadStruct([x4, y4, -unit, -1], [x3, y3, -unit, -1],
          [x2, y2, -unit, -1], [x1, y1, -unit, -1], false,
          i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[2], 2));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & CEILING_BIT &&
        !(map[i + mazeWidth][j + mazeHeight].type &
          HIGH_BIT)) {  // HIGH ROOM CEILING (3, down)
        quads.push(new quadStruct([x4, y4, -unit * 2, -1], [x3, y3, -unit * 2, -1],
          [x2, y2, -unit * 2, -1], [x1, y1, -unit * 2, -1],
          false, i + mazeWidth, j + mazeHeight,
          map[i + mazeWidth][j + mazeHeight].flat[3], 3));
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
  return Math.floor(random(0, 65535));
}

class kruskalCell {
  constructor(set, right, down) {
    this.set = set;
    this.right = right;
    this.down = down;
  }
};

class mapCell {

  constructor(type, flat, wall) {
    if (type === undefined) {
      this.type = 0;
    } else {
      this.type = type;
    }
    if (flat === undefined) {
      this.flat = [0, 0, 0, 0];
    } else {
      this.flat = flat;
    }
    if (wall === undefined) {
      this.wall = [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]];
    } else {
      this.wall = wall;
    }

  }
}

let map = [];
for (let j = 0; j <= MAX_HEIGHT * 2 + 1; j++) {
  let row = [];
  for (let i = 0; i <= MAX_WIDTH * 2 + 1; i++) {
    row.push(new mapCell());
  }
  map.push(row);
}


let kruskalMaze = [];
for (let j = 0; j < MAX_HEIGHT; j++) {
  let row = [];
  for (let i = 0; i < MAX_WIDTH; i++) {
    row.push(new kruskalCell(0, false, false));
  }
  kruskalMaze.push(row);
}

let clipboardWidth = -1, clipboardHeight = -1;
let clipboardPreview = false;

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
    let y = rand() % (mazeWidth * 2 - rh);
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

  let distanceTravelled = sqrt(pow(playerX - lastPlayerX, 2) + pow(playerY - lastPlayerY, 2));
  if (distanceTravelled > unit) {
    let dx = (playerX - lastPlayerX) / distanceTravelled;
    let dy = (playerY - lastPlayerY) / distanceTravelled;
    playerX = lastPlayerX + dx * unit;
    playerY = lastPlayerY + dy * unit;
    console.log("Stead on, son.");
  }

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
    let y = rand() % (mazeWidth * 2 - rh);
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

let rays;

function handlePlayerInteractions() {

  rays = [];

  let mapXstart = Math.floor(lastPlayerX / unit) + mazeWidth;
  let mapYstart = Math.floor(lastPlayerY / unit) + mazeHeight;
  let mapXtarget = Math.floor(playerX / unit) + mazeWidth;
  let mapYtarget = Math.floor(playerY / unit) + mazeHeight;

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

  //console.log(lowestX, highestX, ",", lowestY, highestY, "=", playerX, playerY);

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

          ray.length = sqrt(pow(ray.x, 2) - pow(ray.y, 2));
          if (isNaN(ray.length) || ray.length == 0) continue;

          ray.overlap = unit * 0.5 - ray.length;
          if (isNaN(ray.overlap)) continue;           

          rays.push(ray);

          if (ray.overlap > 0) {

            //console.log("overlap", overlap, "rayLength", rayLength);

            //playerX -= ray.overlap * ray.x / ray.length;
            //playerY -= ray.overlap * ray.y / ray.length;

          }

        }
      }


    }
  }


}


function handlePlayerInteractions_old() {
  let mapX = Math.floor(playerX / unit) + mazeWidth;
  let mapY = Math.floor(playerY / unit) + mazeHeight;

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

  let dx = playerX - lastPlayerX;
  let dy = playerY - lastPlayerY;

  if (mapX > 0 && mapY > 0 && mapX < mazeWidth * 2 + 1 &&
    mapY < mazeWidth * 2 + 1) {
    let north = false, south = false, east = false, west = false;
    let northEast = false, southEast = false, northWest = false,
      southWest = false;

    let eastWest = fract(playerX / unit + mazeWidth - mapX);
    let northSouth = fract(playerY / unit + mazeHeight - mapY);

    east = map[mapX - 1][mapY].type & INTERACTION_BIT;
    west = map[mapX + 1][mapY].type & INTERACTION_BIT;
    north = map[mapX][mapY - 1].type & INTERACTION_BIT;
    south = map[mapX][mapY + 1].type & INTERACTION_BIT;

    northEast = map[mapX - 1][mapY - 1].type & INTERACTION_BIT;
    southEast = map[mapX - 1][mapY + 1].type & INTERACTION_BIT;
    northWest = map[mapX + 1][mapY - 1].type & INTERACTION_BIT;
    southWest = map[mapX + 1][mapY + 1].type & INTERACTION_BIT;

    if (!north && !east && northSouth < 0.25 && eastWest < 0.25 &&
      northEast) {
      if (dx < 0 && dy < 0) {
        if (-dx > -dy)
          playerX = (mapX + 0.25 - mazeWidth) * unit;
        else
          playerY = (mapY + 0.25 - mazeHeight) * unit;
      } else {
        if (dx < 0) playerX = (mapX + 0.25 - mazeWidth) * unit;
        if (dy < 0) playerY = (mapY + 0.25 - mazeHeight) * unit;
      }
    }

    if (!north && !west && northSouth < 0.25 && eastWest > 0.75 &&
      northWest) {
      if (dx > 0 && dy < 0) {
        if (dx > -dy)
          playerX = (mapX + 0.75 - mazeWidth) * unit;
        else
          playerY = (mapY + 0.25 - mazeHeight) * unit;
      } else {
        if (dx > 0) playerX = (mapX + 0.75 - mazeWidth) * unit;
        if (dy < 0) playerY = (mapY + 0.25 - mazeHeight) * unit;
      }
    }

    if (!south && !east && northSouth > 0.75 && eastWest < 0.25 &&
      southEast) {
      if (dx < 0 && dy > 0) {
        if (-dx < dy)
          playerX = (mapX + 0.25 - mazeWidth) * unit;
        else
          playerY = (mapY + 0.75 - mazeHeight) * unit;
      } else {
        if (dx < 0) playerX = (mapX + 0.25 - mazeWidth) * unit;
        if (dy > 0) playerY = (mapY + 0.75 - mazeHeight) * unit;
      }
    }

    if (!south && !west && northSouth > 0.75 && eastWest > 0.75 &&
      southWest) {
      if (dx > 0 && dy > 0) {
        if (dx > dy)
          playerX = (mapX + 0.75 - mazeWidth) * unit;
        else
          playerY = (mapY + 0.75 - mazeHeight) * unit;
      } else {
        if (dx > 0) playerX = (mapX + 0.75 - mazeWidth) * unit;
        if (dy > 0) playerY = (mapY + 0.75 - mazeHeight) * unit;
      }
    }

    eastWest = fract(playerX / unit + mazeWidth - mapX);
    northSouth = fract(playerY / unit + mazeHeight - mapY);

    if (northSouth < 0.25 && north && dy < 0)
      playerY = (mapY + 0.25 - mazeHeight) * unit;
    if (northSouth > 0.75 && south && dy > 0)
      playerY = (mapY + 0.75 - mazeHeight) * unit;
    if (eastWest < 0.25 && east && dx < 0)
      playerX = (mapX + 0.25 - mazeWidth) * unit;
    if (eastWest > 0.75 && west && dx > 0)
      playerX = (mapX + 0.75 - mazeWidth) * unit;
  }
}

function updateQuads() {

  for (let quadCounter = 0; quadCounter < quads.length; quadCounter++) {
    let q = quads[quadCounter];

    q.visible = true;

    let outOfRange = abs(q.vertices[0].x - cameraX) > drawDistance ||
      abs(q.vertices[0].y - cameraY) > drawDistance;

    if (outOfRange) q.visible = false;


    q.dSquared = pow(q.centre.x - cameraX, 2) + pow(q.centre.y - cameraY, 2) + pow(q.centre.z - cameraZ, 2);

    let d = 255 * (1 - q.dSquared / pow(drawDistance, 2));
    if (d > 255) d = 255;
    if (d <= 0) continue;

    q.r = d;
    q.g = d;
    q.b = d;

  }

}

function renderMazeMap() {

  const w = windowWidth;
  const h = windowHeight;
  const size = 16;

  const mapX = cameraX / unit + mazeWidth;
  const mapY = cameraY / unit + mazeHeight;

  /*let mapXstart = Math.floor(lastPlayerX / unit) + mazeWidth;
  let mapYstart = Math.floor(lastPlayerY / unit) + mazeHeight;
  let mapXtarget = Math.floor(playerX / unit) + mazeWidth;
  let mapYtarget = Math.floor(playerY / unit) + mazeHeight;    

  let lowestX = min(mapXstart, mapXtarget) - 1;
  let highestX = max(mapXstart, mapXtarget) + 1;
  let lowestY = min(mapYstart, mapYtarget) - 1;
  let highestY = max(mapYstart, mapYtarget) + 1;*/

  setCamera(cam2d);
  ortho();
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

      //if (i >= lowestX && i <= highestX && j >= lowestY && j <= highestY) alpha = 255;

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
    size * 2 * sin(cameraAngle),
    mapY * size +
    size * 2 * cos(cameraAngle)
  );

  
  for (let ray of rays) {
    if (ray.overlap > 0) {
      strokeWeight(4);
      stroke(255, 255, 255, 255);
    } else {
      strokeWeight(1);
      stroke(0, 255, 0, 255);
    }
    line(mapX * size, mapY * size, mapX * size + ray.x/unit*size,  mapY * size + ray.y/unit*size);
  }

  pop();

  /*push();
  stroke(255, 255, 0, 255);
  rotate(playerAngle);
  noFill();
  circle(0, 0, unit*0.5);
  for (let ray of rays) {
    line(0, 0, -ray.x, -ray.y);
  }
  pop(); */


}


function renderQuads() {

  push();

  setCamera(cam3d);
  perspective(PI / 3, windowWidth / windowHeight, 0.01, drawDistance);
  cam3d.setPosition(playerX, playerZ, playerY);
  cam3d.lookAt(playerX + sin(playerAngle), playerZ, playerY + cos(playerAngle));
  noStroke();
  textureMode(NORMAL);

  let lastTexture = null;

  for (let q of quads) {

    if (!q.visible) continue;

    if (q.texture != lastTexture) {
      lastTexture = q.texture;
      texture(textures[q.texture]);
    }

    tint(q.r, q.g, q.b);

    beginShape();

    vertex(q.vertices[0].x, q.vertices[0].z, q.vertices[0].y, 0, 0);
    vertex(q.vertices[1].x, q.vertices[1].z, q.vertices[1].y, 0, 1);
    vertex(q.vertices[2].x, q.vertices[2].z, q.vertices[2].y, 1, 1);
    vertex(q.vertices[3].x, q.vertices[3].z, q.vertices[3].y, 1, 0);

    endShape();

  }

  pop();

}

function draw() {

  handlePlayInputs(deltaTime / 1000);
  handlePlayerInteractions();

  playProcessing();
  updateQuads();

  background(0, 0, 0);

  renderQuads();

  renderMazeMap();


}