"option strict";

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

const texture = [];

const unit = 100;
const MAX_WIDTH = 100;
const MAX_HEIGHT = 100;
let mazeWidth = 40;
let mazeHeight = 40;
let drawDistance = 30 * unit;
const MAX_CLIPBOARD_SIZE = 100;

let cameraAngle = 0;
let cameraPitch = 0;
let cameraX = unit / 2;
let cameraY = unit / 2;
let cameraZ = unit / 2;

let playerX, playerY, playerZ, playerAngle;
let lastPlayerX, lastPlayerY;

let mousePos = { x: 0, y: 0 };
let lastMousePos = { x: 0, y: 0 };
let cursorX, cursorY;
let cursorRotation = 0;

let m = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];

let zoom;
let day = true;
let showHelp = true;
let noClip = false;
let autoTexture = false;
let levelNo = 1;
let exitSignal = false;
let editMode = true;

let editCameraAngle, editCameraPitch, editCameraX, editCameraY, editCameraZ;

let selectionStartX;
let selectionStartY;
let selectionEndX;
let selectionEndY;
let selectionLive = false;

let quads = [];

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
  cameraX = playerX - 6 * unit * sin(playerAngle);
  cameraY = playerY - 6 * unit * cos(playerAngle);
  cameraZ = playerZ - 3 * unit;
  cameraAngle = playerAngle;
}

function preload() {

  for (let i = 0; i < 176; i++) {
    texture.push(loadImage("tiles/tile" + i + ".png"));
  }

}

function windowResized() {
  resizeCanvas(windowWidth, windowHeight);
}

function setup() {

  angleMode(RADIANS);

  createCanvas(windowWidth, windowHeight);
  zoom = windowWidth / 2;

  makeMaze();
  //undoBuffer.clear();
  regenerateQuads();

  playerX = cameraX;
  playerY = cameraY;
  playerZ = cameraZ;
  playerAngle = cameraAngle;

  cameraPitch = 0.6;

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
  constructor() {

  }
}

function regenerateQuads() {
  
  console.log("Regenerating quads");
  
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

/*

function makeEntities(let count) {
  entities.clear();

  for (let i = 0; i < count; i++) {
  tryAgain:

    let x = rand() % (mazeWidth * 2) - mazeWidth;
    let y = rand() % (mazeHeight * 2) - mazeHeight;
    let s = rand() % 50;

    for (auto e : entities) {
      if (e.mapX == x && e.mapY == y) goto tryAgain;
    }

    entities.push(
        entity([static_cast<float>(x + 0.5) * unit,
                      static_cast<float>(y + 0.5) * unit, 7 * unit / 8, -1),
               x, y, s));
  }
}*/

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
for (let j = 0; j < MAX_HEIGHT * 2 + 1; j++) {
  let row = [];
  for (let i = 0; i < MAX_WIDTH * 2 + 1; i++) {
    row.push(new mapCell());
  }
  map.push(row);
}

//mapCell map[MAX_WIDTH * 2 + 1][MAX_HEIGHT * 2 + 1];
//mapCell clipboard[MAX_CLIPBOARD_SIZE][MAX_CLIPBOARD_SIZE];

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



function keyPressed() {

  if (key == 'p' && keyIsDown(CONTROL)) {
    cameraAngle = editCameraAngle;
    cameraPitch = editCameraPitch;
    cameraX = editCameraX;
    cameraY = editCameraY;
    cameraZ = editCameraZ;
    editMode = true;
    return;
  }

  if (key == 'q' && keyIsDown(CONTROL)) {
    exitSignal = true;
  }

  if (key == 'n' && keyIsDown(CONTROL)) {
    noClip = !noClip;
  }

}

function handlePlayInputs(frameLength) {

  lastPlayerX = playerX;
  lastPlayerY = playerY;

  if (keyIsDown(LEFT_ARROW)) playerAngle += 2 * frameLength;
  if (keyIsDown(RIGHT_ARROW)) playerAngle -= 2 * frameLength;

  if (keyIsDown(87)) { //W
    playerX += sin(playerAngle) * unit * 2 * frameLength;
    playerY += cos(playerAngle) * unit * 2 * frameLength;
    console.log(playerX, playerY);
  }
  if (keyIsDown(83)) { //S
    playerX -= sin(playerAngle) * unit * 2 * frameLength;
    playerY -= cos(playerAngle) * unit * 2 * frameLength;
    console.log(playerX, playerY);
  }
  if (keyIsDown(65)) { //A
    playerX += cos(playerAngle) * unit * 2 * frameLength;
    playerY += -sin(playerAngle) * unit * 2 * frameLength;
    console.log(playerX, playerY);
  }
  if (keyIsDown(68)) { //D
    playerX -= cos(playerAngle) * unit * 2 * frameLength;
    playerY -= -sin(playerAngle) * unit * 2 * frameLength;
    console.log(playerX, playerY);
  }
}

/*

function handleEditInputs(let frameLength) {
  mousePos = GetMousePos();

  if (keyIsDown('KeyP).bPressed && keyIsDown('ControlLeft)) {
    editCameraAngle = cameraAngle;
    editCameraPitch = cameraPitch;
    editCameraX = cameraX;
    editCameraY = cameraY;
    editCameraZ = cameraZ;

    playerX = cameraX;
    playerY = cameraY;
    playerZ = cameraZ;
    playerAngle = cameraAngle;

    cameraPitch = 0.6;

    editMode = false;
  }

  if (GetMouse(2) || keyIsDown('KeyOEM_5)) {
    let iMax = static_cast<int>(w / (TILE_SIZE + 10));
    let jMax = static_cast<int>(176 / iMax) + 1;

    selectedTexture =
        static_cast<int>(mousePos.y / (TILE_SIZE + 10.0f)) * iMax +
        static_cast<int>(mousePos.x < (iMax - 1) * (TILE_SIZE + 10.0f)
                             ? mousePos.x / (TILE_SIZE + 10.0f)
                             : (iMax - 1));

    if (selectedTexture < 0) selectedTexture = 0;
    if (selectedTexture > 175) selectedTexture = 175;

  } else {
    let mouseWheel = GetMouseWheel();

    if (mouseWheel < 0) selectedBlock = (selectedBlock + 1) % 12;
    if (mouseWheel > 0) selectedBlock = (selectedBlock + 11) % 12;

    if (keyIsDown('ArrowLeft)) cameraAngle += 2 * frameLength;
    if (keyIsDown('ArrowRight)) cameraAngle -= 2 * frameLength;

    if (cursorX >= 0 && cursorY >= 0 && cursorX <= mazeWidth * 2 &&
        cursorY <= mazeHeight * 2) {
      if (GetMouse(0) && quad::cursorQuad != nullptr) {
        let x = quad::cursorQuad->mapX;
        let y = quad::cursorQuad->mapY;
        let f = quad::cursorQuad->level;
        let d = quad::cursorQuad->direction;

        uint8_t candidateTexture;
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



/*
function makeEntities(let count) {
  entities.clear();

  for (let i = 0; i < count; i++) {
    tryAgain:

    let x = rand() % (mazeWidth * 2) - mazeWidth;
    let y = rand() % (mazeHeight * 2) - mazeHeight;
    let s = rand() % 50;

    for (auto e : entities) {
      if (e.mapX == x && e.mapY == y) goto tryAgain;
    }

    entities.push(
      entity([static_cast<float>(x + 0.5) * unit,
      static_cast<float>(y + 0.5) * unit, 7 * unit / 8, -1),
      x, y, s));
  }
}*/

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
/*tedTexture) {
  undoBuffer.push(action(map[x][y], x, y, frame));

  if (quad:: cursorQuad -> wall) {
    map[x][y].wall[f][d] = selectedTexture;
  } else {
    map[x][y].flat[f] = selectedTexture;
  }

  quad:: cursorQuad -> renderable = & texture[selectedTexture];
}

      } else if (GetMouse(1) && quad:: cursorQuad != nullptr) {
  cameraAngle -= static_cast<float>(mousePos.x - lastMousePos.x) /
    (w / 4) * ((w / 2) / zoom);

} else if (keyIsDown('KeyQ).bPressed) {
        let x = quad:: cursorQuad -> mapX;
let y = quad:: cursorQuad-> mapY;
let f = quad:: cursorQuad-> level;
let d = quad:: cursorQuad-> direction;

if (quad:: cursorQuad -> wall) {
  selectedTexture = map[x][y].wall[f][d];
} else if (f == 0) {
  selectedTexture = map[x][y].flat[f];
}

      } else if ((keyIsDown('KeyT).bPressed ||
                  keyIsDown('KeyF).bPressed ||
                  keyIsDown('KeyC).bPressed) &&
                 !keyIsDown('ControlLeft)) {
        if (selectionLive) {
    let x1 = selectionStartX < selectionEndX ? selectionStartX
      : selectionEndX;
    let y1 = selectionStartY < selectionEndY ? selectionStartY
      : selectionEndY;
    let x2 = selectionStartX > selectionEndX ? selectionStartX
      : selectionEndX;
    let y2 = selectionStartY > selectionEndY ? selectionStartY
      : selectionEndY;

    if (x2 - x1 >= MAX_CLIPBOARD_SIZE) x2 = x1 + MAX_CLIPBOARD_SIZE - 1;
    if (y2 - y1 >= MAX_CLIPBOARD_SIZE) y2 = y1 + MAX_CLIPBOARD_SIZE - 1;

    for (let i = x1; i <= x2; i++) {
      for (let j = y1; j <= y2; j++) {
        undoBuffer.push(action(map[i][j], i, j, frame));
        if (keyIsDown('KeyT).bPressed)
                setQuadTexture(i, j, selectedTexture, true);
        if (keyIsDown('KeyF).bPressed)
                setQuadTexture(i, j, selectedTexture, false, true);
        if (keyIsDown('KeyC).bPressed)
                setQuadTexture(i, j, selectedTexture, false);
      }
    }

    selectionLive = false;

  } else {
    let x = quad:: cursorQuad-> mapX;
    let y = quad:: cursorQuad-> mapY;
    undoBuffer.push(action(map[x][y], x, y, frame));

    if (keyIsDown('KeyT).bPressed)
            setQuadTexture(x, y, selectedTexture, true);
    if (keyIsDown('KeyF).bPressed)
            setQuadTexture(x, y, selectedTexture, false, true);
    if (keyIsDown('KeyC).bPressed)
            setQuadTexture(x, y, selectedTexture, false);
  }
      }
    }

if (keyIsDown('KeyR).bPressed) {
      if (keyIsDown('ControlLeft)) {
        cursorRotation = (cursorRotation + 7) % 8;
      } else {
  cursorRotation = (cursorRotation + 1) % 8;
}
    }

if (keyIsDown('KeyESCAPE).bPressed) {
      selectionLive = false;
    }

if (keyIsDown('KeySHIFT).bPressed && quad::cursorQuad != nullptr) {
      selectionStartX = quad:: cursorQuad -> mapX;
selectionStartY = quad:: cursorQuad -> mapY;
selectionEndX = quad:: cursorQuad -> mapX;
selectionEndY = quad:: cursorQuad -> mapY;
selectionLive = true;
    }

if (keyIsDown('KeySHIFT) && quad::cursorQuad != nullptr) {
      selectionEndX = quad:: cursorQuad -> mapX;
selectionEndY = quad:: cursorQuad -> mapY;
    }

if (keyIsDown('ControlLeft)) {
      if (keyIsDown('KeyK1).bPressed) {
        levelNo = 1;
      } else if (keyIsDown('KeyK2).bPressed) {
        levelNo = 2;
      } else if (keyIsDown('KeyK3).bPressed) {
        levelNo = 3;
      } else if (keyIsDown('KeyK4).bPressed) {
        levelNo = 4;
      } else if (keyIsDown('KeyK5).bPressed) {
        levelNo = 5;
      } else if (keyIsDown('KeyK6).bPressed) {
        levelNo = 6;
      } else if (keyIsDown('KeyK7).bPressed) {
        levelNo = 7;
      } else if (keyIsDown('KeyK8).bPressed) {
        levelNo = 8;
      } else if (keyIsDown('KeyK9).bPressed) {
        levelNo = 9;
      } else if (keyIsDown('KeyK0).bPressed) {
        levelNo = 10;
      }

if (keyIsDown('KeyZ).bPressed && undoBuffer.size() > 0) {
        action u = undoBuffer.back();
let f = u.frame;
while (u.frame == f) {
  map[u.x][u.y] = u.cell;
  undoBuffer.pop_back();
  if (undoBuffer.size() == 0) break;
  u = undoBuffer.back();
}
regenerateQuads();
      }

if (keyIsDown('KeyE).bPressed) {
        makeEntities(1000);
      }

if (keyIsDown('KeyT).bPressed) {
        autoTexture = !autoTexture;
      }

if (keyIsDown('KeyQ).bPressed) {
        exitSignal = true;
      }

if (keyIsDown('KeyS).bPressed) {
        saveLevel();
      }

if (keyIsDown('KeyL).bPressed) {
        undoBuffer.clear();
loadLevel();
      }

if (keyIsDown('KeyM).bPressed) {
        day = true;
Clear(olc:: Pixel(96, 128, 255));
clearMap();
undoBuffer.clear();
regenerateQuads();
      }

if (keyIsDown('KeyG).bPressed) {
        day = false;
Clear(olc:: BLACK);
makeMaze();
undoBuffer.clear();
regenerateQuads();
      }

if (keyIsDown('KeyHOME).bPressed) {
        zoom = w / 2;
drawDistance = 30 * unit;
      }

    } else {
  if (keyIsDown('KeyK1).bPressed) {
        selectedBlock = 0;
} else if (keyIsDown('KeyK2).bPressed) {
        selectedBlock = 1;
      } else if (keyIsDown('KeyK3).bPressed) {
        selectedBlock = 2;
      } else if (keyIsDown('KeyK4).bPressed) {
        selectedBlock = 3;
      } else if (keyIsDown('KeyK5).bPressed) {
        selectedBlock = 4;
      } else if (keyIsDown('KeyK6).bPressed) {
        selectedBlock = 5;
      } else if (keyIsDown('KeyK7).bPressed) {
        selectedBlock = 6;
      } else if (keyIsDown('KeyK8).bPressed) {
        selectedBlock = 7;
      } else if (keyIsDown('KeyK9).bPressed) {
        selectedBlock = 8;
      } else if (keyIsDown('KeyK0).bPressed) {
        selectedBlock = 9;
      } else if (keyIsDown('KeyMINUS).bPressed) {
        selectedBlock = 10;
      } else if (keyIsDown('KeyEQUALS).bPressed) {
        selectedBlock = 11;
      }
    }

if (keyIsDown('KeyOEM_4)) drawDistance /= 1.01;
    if (keyIsDown('KeyOEM_6)) drawDistance *= 1.01;

    if (drawDistance < unit) drawDistance = unit;
if (drawDistance > unit * mazeWidth * 2)
  drawDistance = unit * mazeWidth * 2;

if (keyIsDown('ArrowDown)) cameraPitch += 2 * frameLength;
    if (keyIsDown('ArrowUp)) cameraPitch -= 2 * frameLength;
    if (keyIsDown('KeyHOME)) cameraPitch = 0;

    if (keyIsDown('KeyPGUP)) cameraZ -= unit * 2 * frameLength;
    if (keyIsDown('KeyPGDN)) cameraZ += unit * 2 * frameLength;
    if (keyIsDown('KeyEND)) cameraZ = unit / 2;

    if (cameraZ > unit / 2) cameraZ = unit / 2;

if (keyIsDown('KeyC).bPressed && keyIsDown('ControlLeft)) {
  if (selectionLive) {
    let x1 =
      selectionStartX < selectionEndX ? selectionStartX : selectionEndX;
    let y1 =
      selectionStartY < selectionEndY ? selectionStartY : selectionEndY;
    let x2 =
      selectionStartX > selectionEndX ? selectionStartX : selectionEndX;
    let y2 =
      selectionStartY > selectionEndY ? selectionStartY : selectionEndY;

    if (x2 - x1 >= MAX_CLIPBOARD_SIZE) x2 = x1 + MAX_CLIPBOARD_SIZE - 1;
    if (y2 - y1 >= MAX_CLIPBOARD_SIZE) y2 = y1 + MAX_CLIPBOARD_SIZE - 1;

    clipboardWidth = x2 - x1 + 1;
    clipboardHeight = y2 - y1 + 1;
    cursorRotation = 0;

    for (let i = x1; i <= x2; i++) {
      for (let j = y1; j <= y2; j++) {
        clipboard[i - x1][j - y1] = map[i][j];
      }
    }
  }
}

if (quad:: cursorQuad != nullptr) {
  let x = quad:: cursorQuad-> mapX;
  let y = quad:: cursorQuad-> mapY;

  if (keyIsDown('KeySPACE).bPressed) {
        if (selectionLive) {
      let x1 = selectionStartX < selectionEndX ? selectionStartX
        : selectionEndX;
      let y1 = selectionStartY < selectionEndY ? selectionStartY
        : selectionEndY;
      let x2 = selectionStartX > selectionEndX ? selectionStartX
        : selectionEndX;
      let y2 = selectionStartY > selectionEndY ? selectionStartY
        : selectionEndY;

      if (x2 - x1 >= MAX_CLIPBOARD_SIZE) x2 = x1 + MAX_CLIPBOARD_SIZE - 1;
      if (y2 - y1 >= MAX_CLIPBOARD_SIZE) y2 = y1 + MAX_CLIPBOARD_SIZE - 1;

      for (let i = x1; i <= x2; i++) {
        for (let j = y1; j <= y2; j++) {
          undoBuffer.push(action(map[i][j], i, j, frame));
          map[i][j].type = selectedBlockTypes[selectedBlock];
          if (autoTexture) {
            setQuadTexture(i, i, selectedTexture, true);
            setQuadTexture(j, j, selectedTexture, false);
            setQuadTexture(j, j, selectedTexture, false, true);
          }
        }
      }

      regenerateQuads();

      selectionLive = false;

    } else {
      undoBuffer.push(action(map[x][y], x, y, frame));
      map[x][y].type = selectedBlockTypes[selectedBlock];
      regenerateQuads();
      if (autoTexture) {
        setQuadTexture(x, y, selectedTexture, true);
        setQuadTexture(x, y, selectedTexture, false);
        setQuadTexture(x, y, selectedTexture, false, true);
      }
    }
  quad:: cursorQuad = nullptr;
}

clipboardPreview = keyIsDown('KeyV);

      if (keyIsDown('KeyV).bPressed && keyIsDown('ControlLeft)) {
  for (let i = 0; i < clipboardWidth; i++) {
    for (let j = 0; j < clipboardHeight; j++) {
      switch (cursorRotation) {
        case 0:
          if (x + i < 0 || y + j < 0 || x + i > 2 * mazeWidth ||
            y + j > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x + i][y + j], x + i, y + j, frame));
          map[x + i][y + j] = clipboard[i][j];
          break;

        case 1:
          if (x + j < 0 || y + i < 0 || x + j > 2 * mazeWidth ||
            y + i > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x + j][y + i], x + j, y + i, frame));
          map[x + j][y + i] = clipboard[i][j];
          break;

        case 2:
          if (x - j < 0 || y + i < 0 || x - j > 2 * mazeWidth ||
            y + i > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x - j][y + i], x - j, y + i, frame));
          map[x - j][y + i] = clipboard[i][j];
          break;

        case 3:
          if (x - i < 0 || y + j < 0 || x - i > 2 * mazeWidth ||
            y + j > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x - i][y + j], x - i, y + j, frame));
          map[x - i][y + j] = clipboard[i][j];
          break;

        case 4:
          if (x - i < 0 || y - j < 0 || x - i > 2 * mazeWidth ||
            y - j > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x - i][y - j], x - i, y - j, frame));
          map[x - i][y - j] = clipboard[i][j];
          break;

        case 5:
          if (x - j < 0 || y - i < 0 || x - j > 2 * mazeWidth ||
            y - i > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x - j][y - i], x - j, y - i, frame));
          map[x - j][y - i] = clipboard[i][j];
          break;

        case 6:
          if (x + i < 0 || y - i < 0 || x + j > 2 * mazeWidth ||
            y - i > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x + j][y - i], x + j, y - i, frame));
          map[x + j][y - i] = clipboard[i][j];
          break;

        case 7:
          if (x + i < 0 || y - j < 0 || x + i > 2 * mazeWidth ||
            y - j > 2 * mazeWidth)
            continue;
          undoBuffer.push(
            action(map[x + i][y - j], x + i, y - j, frame));
          map[x + i][y - j] = clipboard[i][j];
          break;
      }
    }
  }
  regenerateQuads();
}

if (keyIsDown('KeyE).bPressed && !keyIsDown('ControlLeft)) {
  for (let i = 0; i < 12; i++) {
    if (map[x][y].type == selectedBlockTypes[i]) {
      selectedBlock = i;
    }
  }
}
    }

if (keyIsDown('KeyPERIOD)) zoom *= 1.05;
    if (keyIsDown('KeyCOMMA)) zoom /= 1.05;
    if (zoom < w / 4) zoom = w / 4;
if (zoom > w * 5) zoom = w * 5;

if (keyIsDown('KeyN).bPressed && !keyIsDown('ControlLeft)) {
  day = !day;
  if (day) {
    Clear(olc:: Pixel(96, 128, 255));
  } else {
    Clear(olc:: BLACK);
  }
}

if (keyIsDown('KeyH).bPressed) {
      showHelp = !showHelp;
    }

if (cameraPitch < -1.571) cameraPitch = -1.571;
if (cameraPitch > 1.571) cameraPitch = 1.571;

if (!keyIsDown('ControlLeft)) {
      if (keyIsDown('KeyW)) {
        cameraX += sin(cameraAngle) * unit * 2 * frameLength;
cameraY += cos(cameraAngle) * unit * 2 * frameLength;
      }
if (keyIsDown('KeyS)) {
        cameraX -= sin(cameraAngle) * unit * 2 * frameLength;
cameraY -= cos(cameraAngle) * unit * 2 * frameLength;
      }
if (keyIsDown('KeyA)) {
        cameraX += cos(cameraAngle) * unit * 2 * frameLength;
cameraY += -sin(cameraAngle) * unit * 2 * frameLength;
      }
if (keyIsDown('KeyD)) {
        cameraX -= cos(cameraAngle) * unit * 2 * frameLength;
cameraY -= -sin(cameraAngle) * unit * 2 * frameLength;
      }
    }
  }
}

* /

function handlePlayerInteractions() {
  let mapX = playerX / unit + mazeWidth;
  let mapY = playerY / unit + mazeHeight;

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

/*

function updateEntities() {
  for (auto& e : entities) {
    e.outOfRange = std::abs(e.position.x - cameraX) > drawDistance ||
                   std::abs(e.position.y - cameraY) > drawDistance;

    if (e.outOfRange) continue;

    e.transformed.x = (e.position.x - cameraX) * m[0][0] +
                      (e.position.y - cameraY) * m[0][1] +
                      (e.position.z - cameraZ) * m[0][2];
    e.transformed.y = (e.position.x - cameraX) * m[1][0] +
                      (e.position.y - cameraY) * m[1][1] +
                      (e.position.z - cameraZ) * m[1][2];
    e.transformed.z = (e.position.x - cameraX) * m[2][0] +
                      (e.position.y - cameraY) * m[2][1] +
                      (e.position.z - cameraZ) * m[2][2];
    e.visible = true;

    if (e.transformed.x > drawDistance || e.transformed.y > drawDistance ||
        e.transformed.z > drawDistance || e.transformed.y < OMEGA) {
      e.visible = false;
      continue;
    }

    e.scale = {5 * SPRITE_SIZE / (e.transformed.y + OMEGA),
               5 * SPRITE_SIZE / (e.transformed.y + OMEGA)};

    e.projected = {
        w / 2 - (e.transformed.x * zoom) / (e.transformed.y + OMEGA) -
            (SPRITE_SIZE / 2) * e.scale.x,
        h / 2 + (e.transformed.z * zoom) / (e.transformed.y + OMEGA) -
            (SPRITE_SIZE / 2) * e.scale.y};

    e.dSquared = std::pow(e.transformed.x, 2) + std::pow(e.transformed.y, 2) +
                 std::pow(e.transformed.z, 2);
  }
}

function updateQuads() {
  let levelQuads = quads.size();

  for (let quadCounter = 0; quadCounter < quads.size(); quadCounter++) {
    quad* q = &quads[quadCounter];

    q->outOfRange = std::abs(q->vertices[0].x - cameraX) > drawDistance ||
                    std::abs(q->vertices[0].y - cameraY) > drawDistance;

    if (q->outOfRange) continue;

    let maxX = 0;
    let maxY = 0;
    let maxZ = 0;

    for (let i = 0; i < 4; i++) {
      q->transformed[i].x = (q->vertices[i].x - cameraX) * m[0][0] +
                            (q->vertices[i].y - cameraY) * m[0][1] +
                            (q->vertices[i].z - cameraZ) * m[0][2];
      if (std::abs(q->transformed[i].x) > maxX)
        maxX = std::abs(q->transformed[i].x);
      q->transformed[i].y = (q->vertices[i].x - cameraX) * m[1][0] +
                            (q->vertices[i].y - cameraY) * m[1][1] +
                            (q->vertices[i].z - cameraZ) * m[1][2];
      if (std::abs(q->transformed[i].y) > maxY)
        maxY = std::abs(q->transformed[i].y);
      q->transformed[i].z = (q->vertices[i].x - cameraX) * m[2][0] +
                            (q->vertices[i].y - cameraY) * m[2][1] +
                            (q->vertices[i].z - cameraZ) * m[2][2];
      if (std::abs(q->transformed[i].z) > maxZ)
        maxZ = std::abs(q->transformed[i].z);
      q->visible = true;
      q->cropped = false;

      q->partials.clear();
    }

    if (maxX > drawDistance || maxY > drawDistance || maxZ > drawDistance) {
      q->visible = false;
      continue;
    }

    if (!q->wall) {
      let lostCornerCount = 0;

      for (let j = 0; j < 4; j++) {
        if (q->transformed[j].y < OMEGA) lostCornerCount++;
      }

      if (lostCornerCount > 0) {
        q->visible = false;

        if (lostCornerCount < 4) {
          let n = 0;
          let nY = 0;
          for (let j = 0; j < 4; j++) {
            if (q->transformed[j].y > nY) {
              nY = q->transformed[j].y;
              n = j;
            }
          }

          let x0 = q->transformed[n].x;
          let y0 = q->transformed[n].y;
          let z0 = q->transformed[n].z;

          let x3 = q->transformed[(n + 2) % 4].x;
          let y3 = q->transformed[(n + 2) % 4].y;
          let z3 = q->transformed[(n + 2) % 4].z;

          let d0 = std::sqrt(std::pow(x3 - x0, 2) + std::pow(y3 - y0, 2));
          let dx0 = (x3 - x0) / d0;
          let dy0 = (y3 - y0) / d0;

          let lambdaY = OMEGA;
          let lambda = (lambdaY - y0) / dy0;
          let lambdaX = x0 + lambda * dx0;

          let x1 = q->transformed[(n + 1) % 4].x;
          let y1 = q->transformed[(n + 1) % 4].y;
          let z1 = q->transformed[(n + 1) % 4].z;
          let d1 = std::sqrt(std::pow(x1 - x0, 2) + std::pow(y1 - y0, 2) +
                               std::pow(z1 - z0, 2));
          let dx1 = (x1 - x0) / d1;
          let dy1 = (y1 - y0) / d1;
          let dz1 = (z1 - z0) / d1;

          let x2 = q->transformed[(n + 3) % 4].x;
          let y2 = q->transformed[(n + 3) % 4].y;
          let z2 = q->transformed[(n + 3) % 4].z;
          let d2 = std::sqrt(std::pow(x2 - x0, 2) + std::pow(y2 - y0, 2) +
                               std::pow(z2 - z0, 2));
          let dx2 = (x2 - x0) / d2;
          let dy2 = (y2 - y0) / d2;
          let dz2 = (z2 - z0) / d2;

          let alpha = (lambdaX - x0) * dx1 + (lambdaY - y0) * dy1;

          let xAlpha = x0 + dx1 * alpha;
          let yAlpha = y0 + dy1 * alpha;
          let zAlpha = z0 + dz1 * alpha;

          let xBeta = x0 + dx2 * alpha;
          let yBeta = y0 + dy2 * alpha;
          let zBeta = z0 + dz2 * alpha;

          let xGamma = x0 + dx1 * alpha + dx2 * alpha;
          let yGamma = y0 + dy1 * alpha + dy2 * alpha;
          let zGamma = z0 + dz1 * alpha + dz2 * alpha;

          vertex vs[4] = {[x0, y0, z0, -1),
                          [xAlpha, yAlpha, zAlpha, -1),
                          [xGamma, yGamma, zGamma, -1),
                          [xBeta, yBeta, zBeta, -1)};

          let u, v, du, dv;
          std::vector<int> order;

          if (n == 0) {
            u = 0;
            v = 0;
            du = TILE_SIZE * alpha / d1;
            dv = TILE_SIZE * alpha / d2;
            order = {0, 1, 2, 3};
          }

          if (n == 1) {
            u = 0;
            v = TILE_SIZE * (1 - alpha / d2);
            du = TILE_SIZE * alpha / d1;
            dv = TILE_SIZE * alpha / d2;
            order = {3, 0, 1, 2};
          }

          if (n == 2) {
            u = TILE_SIZE * (1 - alpha / d1);
            v = TILE_SIZE * (1 - alpha / d2);
            du = TILE_SIZE * alpha / d1;
            dv = TILE_SIZE * alpha / d2;
            order = {2, 3, 0, 1};
          }

          if (n == 3) {
            u = TILE_SIZE * (1 - alpha / d1);
            v = 0;
            du = TILE_SIZE * alpha / d1;
            dv = TILE_SIZE * alpha / d2;
            order = {1, 2, 3, 0};
          }

          q->partials.push(partial(
              {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
              {static_cast<int>(u), static_cast<int>(v), static_cast<int>(du),
               static_cast<int>(dv)},
              q->colour));

          if (y1 >= OMEGA) {
            let yTheta = OMEGA;
            let theta = (yTheta - y1) / (y3 - y1);
            let xTheta = x1 + theta * (x3 - x1);
            let zTheta = z1 + theta * (z3 - z1);

            vertex vs[4]{
                [xAlpha, yAlpha, zAlpha, -1), [x1, y1, z1, -1),
                [xTheta, yTheta, zTheta, -1),
                [xTheta - (x1 - xAlpha), yTheta - (y1 - yAlpha),
                       zTheta - (z1 - zAlpha), -1)};

            let u, v, du, dv;
            std::vector<int> order;

            if (n == 0) {
              u = 0;
              v = TILE_SIZE * alpha / d1;
              du = TILE_SIZE * theta;
              dv = TILE_SIZE * (1 - alpha / d1);
              order = {0, 1, 2, 3};
            }

            if (n == 1) {
              u = TILE_SIZE * alpha / d1;
              v = TILE_SIZE * (1 - theta);
              du = TILE_SIZE * (1 - alpha / d1);
              dv = TILE_SIZE * theta;
              order = {3, 0, 1, 2};
            }

            if (n == 2) {
              u = TILE_SIZE * (1 - theta);
              v = 0;
              du = TILE_SIZE * theta;
              dv = TILE_SIZE * (1 - alpha / d1);
              order = {2, 3, 0, 1};
            }

            if (n == 3) {
              u = 0;
              v = 0;
              du = TILE_SIZE * (1 - alpha / d1);
              dv = TILE_SIZE * theta;
              order = {1, 2, 3, 0};
            }

            q->partials.push(partial(
                {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                {static_cast<int>(u), static_cast<int>(v),
                 static_cast<int>(du), static_cast<int>(dv)},
                q->colour));
          }

          if (y2 >= OMEGA) {
            let yTheta = OMEGA;
            let theta = (yTheta - y2) / (y3 - y2);
            let xTheta = x2 + theta * (x3 - x2);
            let zTheta = z2 + theta * (z3 - z2);

            vertex vs[4]{
                [xBeta, yBeta, zBeta, -1),
                [xTheta - (x2 - xBeta), yTheta - (y2 - yBeta),
                       zTheta - (z2 - zBeta), -1),
                [xTheta, yTheta, zTheta, -1),
                [x2, y2, z2, -1),
            };

            let u, v, du, dv;
            std::vector<int> order;

            if (n == 0) {
              u = TILE_SIZE * alpha / d1;
              v = 0;
              du = TILE_SIZE * (1 - alpha / d1);
              dv = TILE_SIZE * theta;
              order = {0, 1, 2, 3};
            }

            if (n == 1) {
              u = 0;
              v = 0;
              du = TILE_SIZE * theta;
              dv = TILE_SIZE * (1 - alpha / d1);
              order = {3, 0, 1, 2};
            }

            if (n == 2) {
              u = 0;
              v = TILE_SIZE * (1 - theta);
              du = TILE_SIZE * (1 - alpha / d1);
              dv = TILE_SIZE * theta;
              order = {2, 3, 0, 1};
            }

            if (n == 3) {
              u = TILE_SIZE * (1 - theta);
              v = TILE_SIZE * alpha / d1;
              du = TILE_SIZE * theta;
              dv = TILE_SIZE * (1 - alpha / d1);
              order = {1, 2, 3, 0};
            }

            q->partials.push(partial(
                {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                {static_cast<int>(u), static_cast<int>(v),
                 static_cast<int>(du), static_cast<int>(dv)},
                q->colour));
          }
        }
      }
    } else {
      q->sourcePos.x = 0;
      q->sourceSize.x = TILE_SIZE;

      let cropped[4]{false, false, false, false};

      let cropCount = 0;

      for (let i = 0; i < 4; i++) {
        if (q->transformed[i].y < OMEGA) {
          cropped[i] = true;
          cropCount++;
        }
      }

      if (cropCount > 2) {
        q->visible = false;
        continue;
      }

      if (cropCount > 0) {
        let key = -1;
        let highestY = OMEGA;
        for (let i = 0; i < 4; i++) {
          if (q->transformed[i].y < highestY) {
            highestY = q->transformed[i].y;
            key = i;
          }
        }

        if (key == -1) {
          q->visible = false;
          continue;
        }

        q->cropped = true;

        let keyY = q->transformed[key].y;
        let pairY = q->transformed[q->vertices[key].pair].y;

        let delta = 1 - (OMEGA - pairY) / (keyY - pairY);

        for (let i = 0; i < 4; i++) {
          if (!cropped[i]) continue;
          let pair = q->vertices[i].pair;

          q->transformed[i].x =
              q->transformed[i].x +
              delta * (q->transformed[pair].x - q->transformed[i].x);

          q->transformed[i].y =
              q->transformed[i].y +
              delta * (q->transformed[pair].y - q->transformed[i].y);

          q->transformed[i].z =
              q->transformed[i].z +
              delta * (q->transformed[pair].z - q->transformed[i].z);

          if (i > pair) {
            q->sourcePos.x = 0;
            q->sourceSize.x = TILE_SIZE * (1 - delta);
          } else {
            q->sourcePos.x = delta * TILE_SIZE;
            q->sourceSize.x = (1 - delta) * TILE_SIZE;
          }
        }
      }
    }

    let ax = q->transformed[1].x - q->transformed[0].x;
    let ay = q->transformed[1].y - q->transformed[0].y;
    let az = q->transformed[1].z - q->transformed[0].z;

    let bx = q->transformed[3].x - q->transformed[0].x;
    let by = q->transformed[3].y - q->transformed[0].y;
    let bz = q->transformed[3].z - q->transformed[0].z;

    q->centre.x = 0;
    q->centre.y = 0;
    q->centre.z = 0;
    for (let i = 0; i < 4; i++) {
      q->centre.x += q->transformed[i].x;
      q->centre.y += q->transformed[i].y;
      q->centre.z += q->transformed[i].z;
    }
    q->centre.x /= 4;
    q->centre.y /= 4;
    q->centre.z /= 4;

    q->normal =
        [ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx);

    let dotProduct = q->centre.x * q->normal.x + q->centre.y * q->normal.y +
                       q->centre.z * q->normal.z;

    q->dSquared = std::pow(q->centre.x, 2) + std::pow(q->centre.y, 2) +
                  std::pow(q->centre.z, 2);

    if (q->partials.size() == 0) {
      q->minProjectedX = w;
      q->minProjectedY = h;
      q->maxProjectedX = 0;
      q->maxProjectedY = 0;
      for (let i = 0; i < 4; i++) {
        q->projected[i] = {w / 2 - (q->transformed[i].x * zoom) /
                                       (q->transformed[i].y + OMEGA),
                           h / 2 + (q->transformed[i].z * zoom) /
                                       (q->transformed[i].y + OMEGA)};
        if (q->projected[i].x < q->minProjectedX)
          q->minProjectedX = q->projected[i].x;
        if (q->projected[i].y < q->minProjectedY)
          q->minProjectedY = q->projected[i].y;
        if (q->projected[i].x > q->maxProjectedX)
          q->maxProjectedX = q->projected[i].x;
        if (q->projected[i].y > q->maxProjectedY)
          q->maxProjectedY = q->projected[i].y;
      }
    } else {
      for (auto& p : q->partials) {
        for (let i = 0; i < 4; i++) {
          p.projected[i].x = w / 2 - (p.transformed[i].x * zoom) /
                                         (p.transformed[i].y + OMEGA);
          p.projected[i].y = h / 2 + (p.transformed[i].z * zoom) /
                                         (p.transformed[i].y + OMEGA);
        }
      }
    }

    if (dotProduct < 0) {
      q->visible = false;
    }
  }
}

function updateCursor() {
  if (GetMouse(2) || keyIsDown('KeyOEM_5)) return;

  let bestDsquared = drawDistance * drawDistance;

  for (auto& q : quads) {
    if (!q.visible || q.partials.size() > 0) continue;
    if (mousePos.x > q.minProjectedX && mousePos.x < q.maxProjectedX &&
        mousePos.y > q.minProjectedY && mousePos.y < q.maxProjectedY) {
      let dx[4];
      let dy[4];

      for (let i = 0; i < 4; i++) {
        dx[i] = q.projected[i].x - mousePos.x;
        dy[i] = q.projected[i].y - mousePos.y;
        let d = std::sqrt(std::pow(dx[i], 2) + std::pow(dy[i], 2));
        dx[i] /= d;
        dy[i] /= d;
      }

      let totalAngle = 0;
      for (let i = 0; i < 4; i++) {
        let dotProduct = dx[i] * dx[(i + 1) % 4] + dy[i] * dy[(i + 1) % 4];
        totalAngle += std::acos(dotProduct);
      }

      if (std::abs(totalAngle - 6.28318) < 0.0001 &&
          q.dSquared < bestDsquared) {
        bestDsquared = q.dSquared;
        cursorX = q.mapX;
        cursorY = q.mapY;

        quad::cursorQuad = &q;
      }
    }
  }

  lastMousePos = mousePos;
}

function sortEntities() {
  sortedEntities.clear();

  for (auto& entity : entities) {
    if (entity.outOfRange) continue;
    if (entity.dSquared > drawDistance * drawDistance) continue;

    let fade = 1 - std::sqrt(entity.dSquared) / drawDistance;
    if (fade < 0) continue;

    if (day) {
      let alpha = 255;
      if (fade < 0.25) {
        alpha = static_cast<int>(255.0f * 4 * fade);
      }
      let rgb = static_cast<int>(128 + 128 * std::pow(fade, 2));
      entity.colour = olc::Pixel(rgb, rgb, rgb, alpha);
    } else {
      let rgb = static_cast<int>(255.0f * std::pow(fade, 2));
      entity.colour = olc::Pixel(rgb, rgb, rgb);
    }

    sortedEntities.push(&entity);
  }

  std::sort(sortedEntities.begin(), sortedEntities.end(),
            [](entity* a, entity* b) { return a->dSquared > b->dSquared; });
}

function sortQuads() {
  sortedQuads.clear();

  for (let quadCounter = 0; quadCounter < quads.size(); quadCounter++) {
    quad* quadPointer = &quads[quadCounter];

    if (quadPointer->outOfRange) continue;
    if (quadPointer->dSquared > drawDistance * drawDistance) continue;
    if (quadPointer->cropped && cameraPitch != 0) continue;
    let fade = 1 - std::sqrt(quadPointer->dSquared) / drawDistance;
    if (fade < 0) continue;

    let inSelection = false;
    let inPreview = false;
    let isPlayer = false;

    if (editMode) {
      let x1 =
          selectionStartX < selectionEndX ? selectionStartX : selectionEndX;
      let y1 =
          selectionStartY < selectionEndY ? selectionStartY : selectionEndY;
      let x2 =
          selectionStartX > selectionEndX ? selectionStartX : selectionEndX;
      let y2 =
          selectionStartY > selectionEndY ? selectionStartY : selectionEndY;

      inSelection = selectionLive && quadPointer->mapX >= x1 &&
                    quadPointer->mapY >= y1 && quadPointer->mapX <= x2 &&
                    quadPointer->mapY <= y2;

      inPreview = cursorRotation == 0 && quadPointer->mapX >= cursorX &&
                      quadPointer->mapX < cursorX + clipboardWidth &&
                      quadPointer->mapY >= cursorY &&
                      quadPointer->mapY < cursorY + clipboardHeight ||
                  cursorRotation == 1 && quadPointer->mapX >= cursorX &&
                      quadPointer->mapX < cursorX + clipboardHeight &&
                      quadPointer->mapY >= cursorY &&
                      quadPointer->mapY < cursorY + clipboardWidth ||
                  cursorRotation == 2 && quadPointer->mapX <= cursorX &&
                      quadPointer->mapY >= cursorY &&
                      quadPointer->mapX > cursorX - clipboardHeight &&
                      quadPointer->mapY < cursorY + clipboardWidth ||
                  cursorRotation == 3 && quadPointer->mapX <= cursorX &&
                      quadPointer->mapX > cursorX - clipboardWidth &&
                      quadPointer->mapY >= cursorY &&
                      quadPointer->mapY < cursorY + clipboardHeight ||
                  cursorRotation == 4 && quadPointer->mapX <= cursorX &&
                      quadPointer->mapY <= cursorY &&
                      quadPointer->mapX > cursorX - clipboardWidth &&
                      quadPointer->mapY > cursorY - clipboardHeight ||
                  cursorRotation == 5 && quadPointer->mapX <= cursorX &&
                      quadPointer->mapY <= cursorY &&
                      quadPointer->mapX > cursorX - clipboardHeight &&
                      quadPointer->mapY > cursorY - clipboardWidth ||
                  cursorRotation == 6 && quadPointer->mapX >= cursorX &&
                      quadPointer->mapX < cursorX + clipboardHeight &&
                      quadPointer->mapY <= cursorY &&
                      quadPointer->mapY > cursorY - clipboardWidth ||
                  cursorRotation == 7 && quadPointer->mapX >= cursorX &&
                      quadPointer->mapY <= cursorY &&
                      quadPointer->mapX < cursorX + clipboardWidth &&
                      quadPointer->mapY > cursorY - clipboardHeight;
    } else {
      let mapX = playerX / unit + mazeWidth;
      let mapY = playerY / unit + mazeHeight;

      isPlayer = quadPointer->mapX == mapX && quadPointer->mapY == mapY;
    }

    if (isPlayer) {
      quadPointer->colour = olc::Pixel(64, 255, 64);
    } else if (clipboardPreview && inPreview) {
      quadPointer->colour = olc::Pixel(255, 64, 255);
    } else if (inSelection) {
      quadPointer->colour = olc::Pixel(64, 255, 255);
    } else if (editMode && cursorX == quadPointer->mapX &&
               cursorY == quadPointer->mapY) {
      if (day) {
        quadPointer->colour = olc::Pixel(128, 128, 255);
      } else {
        quadPointer->colour = olc::Pixel(255, 255, 128);
      }
    } else {
      if (day) {
        let alpha = 255;
        if (fade < 0.25) {
          alpha = static_cast<int>(255.0f * 4 * fade);
        }
        let rgb = static_cast<int>(128 + 128 * std::pow(fade, 2));
        quadPointer->colour = olc::Pixel(rgb, rgb, rgb, alpha);
      } else {
        let rgb = static_cast<int>(255.0f * std::pow(fade, 2));
        quadPointer->colour = olc::Pixel(rgb, rgb, rgb);
      }
    }

    sortedQuads.push(new quadPointer);
  }

  std::sort(sortedQuads.begin(), sortedQuads.end(),
            [](quad* a, quad* b) { return a->dSquared > b->dSquared; });
}

*/

function renderMazeMap() {
  const size = 8;
  for (let i = 0; i < mazeWidth * 2 + 1; i++) {
    for (let j = 0; j < mazeHeight * 2 + 1; j++) {
      if (map[i][j].type & WALL_BIT) {
        if (!(map[i][j].type & HIGH_BIT)) {
          if (!(map[i][j].type & LOW_BIT)) {
            fill(255, 255, 0);
            rect(i * size, j * size, size, size);
          } else {
            fill(255, 0, 255);
            rect(i * size, j * size, size, size);
          }
        } else {
          fill(255, 255, 255);
          rect(i * size, j * size, size, size);
        }
      } else {
        if (!(map[i][j].type & HIGH_BIT)) {
          fill(0, 0, 64);
          rect(i * size, j * size, size, size);
        } else if (!(map[i][j].type & LOW_BIT)) {
          fill(0, 0, 128)
          rect(i * size, j * size, size, size);
        } else {
          fill(0, 0, 255);
          rect(i * size, j * size, size, size);
        }
      }
    }
  }

  let mapX = cameraX / unit + mazeWidth;
  let mapY = cameraY / unit + mazeHeight;

  fill(255, 0, 0);
  rect(mapX * size, mapY * size, size, size);
  stroke(255, 0, 0);

  line(mapX * size + size / 2, mapY * size + size / 2,
    mapX * size + size / 2 +
    size * 2 * sin(cameraAngle),
    mapY * size + size / 2 +
    size * 2 * cos(cameraAngle)
  );
}

/*
function renderQuads(let renderEntities = false) {
  let qCount = 0;
  let eCount = 0;

  let e = 0;

  for (auto * q : sortedQuads) {
    while (e < sortedEntities.size() &&
      sortedEntities[e] -> dSquared > q -> dSquared) {
      auto E = sortedEntities[e];

      if (E -> visible) {
        DrawDecal(E -> projected, E -> renderable -> Decal(), E -> scale, E -> colour);
        eCount++;
      }

      e++;
    }

    if (q -> renderable != nullptr) {
      let dotProduct = q -> centre.x * q -> normal.x +
        q -> centre.y * q -> normal.y +
        q -> centre.z * q -> normal.z;

      auto decal = q -> renderable -> Decal();

      if (dotProduct > 0) {
        if (q -> partials.size() > 0) {
          for (auto & p : q -> partials) {
            DrawPartialWarpedDecal(decal, p.projected, p.sourcePos,
              p.sourceSize, p.colour);
            qCount++;
          }
        }
        if (q -> visible) {
          DrawPartialWarpedDecal(decal, q -> projected, q -> sourcePos,
            q -> sourceSize, q -> colour);
          qCount++;
        }
      }
    }
  }

  if (qCount > qMax) qMax = qCount;

  if (editMode) {
    std:: ostringstream stringStream;

    stringStream + "./maps/level" + levelNo + ".dat" + "\n";
    stringStream + "\n";

    stringStream + "Quads: " + qCount + " (Max: " + qMax + ")"
      + "\n"
        + "Entities: " + eCount + " / " + entities.size()
        + "\n"
          + "Zoom: " + static_cast<int>(200 * zoom / w)
          + "% | Range: " + drawDistance + " | " + GetFPS()
          + " FPS" + "\n"
            + "X: " + cameraX + " Y: " + cameraY + " Z: " + cameraZ
            + "\n";

    if (quad:: cursorQuad != nullptr) {
      stringStream + "Cursor " + cursorX + ", " + cursorY + ", "
        + "Wall: " + (quad:: cursorQuad -> wall ? "True" : "False")
                     + ", Level: " + quad:: cursorQuad -> level
        + ", Direction: " + quad:: cursorQuad -> direction
          + (autoTexture ? " [Auto]" : "") + "\n";
    }

    if (clipboardWidth > -1 && clipboardHeight > -1) {
      stringStream + "Clipboard: " + clipboardWidth + " x "
        + clipboardHeight + " [" + cursorRotation + "]"
        + "\n";
    }

    stringStream + "\n";
    stringStream + "1 2 3 4 5 6 7 8 9 0 - =" + "\n";
    stringStream + "        _ _ _ _ _ _ _ _" + "\n";
    stringStream + "    _ _ # # # #     _ _" + "\n";
    stringStream + "    # #   _ # #   _ # #" + "\n";
    stringStream + "_ # _ # _ # _ # _ # _ #" + "\n" + "\n";

    for (let i = 0; i < 12; i++) {
      if (i == selectedBlock) {
        stringStream + "^ ";
      } else {
        stringStream + "  ";
      }
    }

    DrawStringDecal({ 10, 10}, stringStream.str(), olc:: WHITE);
  }
}

function renderTileSelector() {
  let iMax = static_cast<int>(w / (TILE_SIZE + 10));
  let jMax = static_cast<int>(176 / iMax) + 1;
  for (let j = 0; j < jMax; j++) {
    for (let i = 0; i < iMax; i++) {
      let k = j * iMax + i;
      if (k >= 176) continue;
      DrawDecal({ static_cast<float>(i * (TILE_SIZE + 10) + 10),
        static_cast<float>(j * (TILE_SIZE + 10) + 10)
    },
    texture[k].Decal(), { 1, 1},
      k == selectedTexture ? olc :: WHITE: olc:: GREY);
  }
}
  }

function renderHelp() {
  std:: ostringstream stringStream;
  stringStream + "W/S/A/D - Move" + "\n"
    + "Left Click - Apply Texture (One surface)" + "\n"
      + "Right Click (Drag) or Left/Right - Turn" + "\n"
        + "Middle Click (Hold) or \\ - Choose Texture" + "\n"
          + "T - Apply Texture (walls)" + "\n"
            + "F - Apply Texture (floor)" + "\n"
              + "C - Apply Texture (ceiling)" + "\n"
                + "Q - Pick Texture" + "\n"
                  + "Ctrl + 1,2,3...-,= or Mouse Wheel - Pick Block Type"
                  + "\n"
                    + "Space - Apply block type" + "\n"
                      + "E - Pick block type" + "\n"
                        + "Shift - Select region" + "\n"
                          + "Escape - Cancel selection" + "\n"
                            + "Ctrl + C - Copy column on selection" + "\n"
                              + "V - Preview paste location" + "\n"
                                + "Ctrl + V - Paste column on selection" + "\n"
                                  + "R / Ctrl + R - Change paste transformation" + "\n"
                                    + "Ctrl + Z - Undo texture or block change" + "\n"
                                      + "Ctrl + E - Generate random entities" + "\n"
                                        + "-/+ - Zoom" + "\n"
                                          + ",/. - Render Distance" + "\n"
                                            + "PgUp/PgDn - Fly Up / Down" + "\n"
                                              + "End - Reset Height" + "\n"
                                                + "Up/Down - Look up / down (Experimental)" + "\n"
                                                  + "Home - Reset Pitch" + "\n"
                                                    + "Ctrl + Home - Reset View Zoom & Render Distance"
                                                    + "\n"
                                                      + "Tab - Show map" + "\n"
                                                        + "Ctrl + G - Generate new maze" + "\n"
                                                          + "Ctrl + M - Generate empty map" + "\n"
                                                            + "N - Switch between night and day" + "\n"
                                                              + "Ctrl + N - No-clip mode" + "\n"
                                                                + "Ctrl + S - Save level" + "\n"
                                                                  + "Ctrl + L - Load level" + "\n"
                                                                    + "Ctrl + 1,2,3...0 - Pick save slot, 1-10" + "\n"
                                                                      + "H - Toggle Help (this text!)" + "\n";

  DrawStringDecal({ 2 * w / 3, 10}, stringStream.str(), olc:: WHITE);
}

*/



function draw() {

  background(0, 0, 0);

  /*if (editMode) {
    handleEditInputs(fElapsedTime);
  } else {*/
  handlePlayInputs(deltaTime / 1000);
  //}

  //if (!editMode) 
  playProcessing();

  updateMatrix();

  if (!editMode && !noClip) handlePlayerInteractions();

  //updateQuads();
  //updateEntities();
  //if (editMode) updateCursor();

  //if (keyIsDown('olc:: Key:: TAB)) {
  renderMazeMap();
  /*} else {
    sortQuads();
    sortEntities();
    renderQuads(true);
    if (editMode) {
      if (GetMouse(2) || keyIsDown('KeyOEM_5)) {
        renderTileSelector();
      } else {
        DrawDecal({w / 2 - TILE_SIZE / 2, 10},
                  texture[selectedTexture].Decal(), {1, 1});
      }
      if (showHelp) renderHelp();
    }
  }*/

}

