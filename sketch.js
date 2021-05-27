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


void playProcessing(frameLength) {
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
    createCanvas(windowWidth, windowHeight);
}

function draw() {
    background(0, 0, 0);
}



