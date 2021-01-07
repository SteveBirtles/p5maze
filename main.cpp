#define OLC_PGE_APPLICATION
#include <filesystem>

#include "olcPixelGameEngine.h"

const int w = 1280;
const int h = 1024;

const int TILE_SIZE = 64;
const int SPRITE_SIZE = 64;
const float OMEGA = 0.1;

const uint8_t DEFAULT_FLAT = 97;
const uint8_t DEFAULT_WALL = 154;

const uint8_t CEILING_BIT = 0b10000;
const uint8_t HIGH_BIT = 0b01000;
const uint8_t LOW_BIT = 0b00100;
const uint8_t WALL_BIT = 0b00010;

const uint8_t SKY = 0b00001;
const uint8_t SKY_SINGLE_BLOCK = 0b00011;
const uint8_t SKY_FLOATING_BLOCK = 0b00101;
const uint8_t SKY_DOUBLE_BLOCK = 0b00111;
const uint8_t LOW_ROOM = 0b11001;
const uint8_t LOW_ROOM_SINGLE_BLOCK = 0b11011;
const uint8_t CORRIDOR = 0b11101;
const uint8_t WALL = 0b11111;
const uint8_t HIGH_ROOM = 0b10001;
const uint8_t HIGH_ROOM_SINGLE_BLOCK = 0b10011;
const uint8_t HIGH_ROOM_FLOATING_BLOCK = 0b10101;
const uint8_t HIGH_ROOM_DOUBLE_BLOCK = 0b10111;

const uint8_t GENERATOR_WALL = SKY_SINGLE_BLOCK;
const uint8_t GENERATOR_PATH = SKY;
const uint8_t GENERATOR_ROOM = SKY;

olc::Renderable texture[176];
olc::Renderable sprite[50];

struct vertex {
  float x;
  float y;
  float z;
  int pair;
  vertex() {}
  vertex(float _x, float _y, float _z, int _pair = 0) {
    x = _x;
    y = _y;
    z = _z;
    pair = _pair;
  }
};

struct partial {
  olc::vf2d sourcePos;
  olc::vf2d sourceSize;
  std::array<olc::vf2d, 4> projected;
  std::array<vertex, 4> transformed;
  olc::Pixel colour;
  partial(std::array<vertex, 4> _transformed, std::array<int, 4> _texture,
          olc::Pixel _colour = olc::WHITE) {
    sourcePos.x = _texture[0];
    sourcePos.y = _texture[1];
    sourceSize.x = _texture[2];
    sourceSize.y = _texture[3];
    transformed = _transformed;
    colour = _colour;
  }
};

struct quad {
  static quad* cursorQuad;
  olc::Renderable* renderable;
  std::array<vertex, 4> vertices;
  std::array<vertex, 4> transformed;
  olc::vf2d sourcePos;
  olc::vf2d sourceSize;
  bool visible;
  bool wall;
  bool cropped;
  double dSquared;
  bool outOfRange;
  olc::vf2d projected[4];
  float minProjectedX;
  float minProjectedY;
  float maxProjectedX;
  float maxProjectedY;
  std::vector<partial> partials;
  vertex normal;
  vertex centre;
  olc::Pixel colour;
  int mapX;
  int mapY;
  int level;
  int direction;
  quad() {
    mapX = -1;
    mapY = -1;
  };
  quad(vertex _p0, vertex _p1, vertex _p2, vertex _p3, bool _wall = false,
       int _mapX = 0, int _mapY = 0, int textureNo = -1, int _level = 0,
       int _direction = -1) {
    vertices[0] = _p0;
    vertices[1] = _p1;
    vertices[2] = _p2;
    vertices[3] = _p3;
    sourcePos.x = 0;
    sourcePos.y = 0;
    sourceSize.x = TILE_SIZE;
    sourceSize.y = TILE_SIZE;
    mapX = _mapX;
    mapY = _mapY;
    wall = _wall;
    level = _level;
    direction = _direction;
    if (textureNo != -1) {
      renderable = &texture[textureNo];
    } else {
      renderable = nullptr;
    }
  }
  ~quad() {
    if (quad::cursorQuad == this) {
      quad::cursorQuad = nullptr;
    }
  }
};

quad* quad::cursorQuad = nullptr;

std::vector<quad> quads;
std::vector<quad*> sortedQuads;

struct entity {
  olc::Renderable* renderable;
  vertex position;
  vertex transformed;
  olc::vf2d projected;
  olc::vf2d scale;
  bool visible;
  double dSquared;
  bool outOfRange;
  olc::Pixel colour;
  int mapX;
  int mapY;
  int level;
  entity() {
    mapX = -1;
    mapY = -1;
  };
  entity(vertex _p, int _mapX, int _mapY, int spriteNo, int _level = 0) {
    position = _p;
    mapX = _mapX;
    mapY = _mapY;
    renderable = &sprite[spriteNo];
    level = _level;
  }
};

std::vector<entity> entities;
std::vector<entity*> sortedEntities;

struct mapCell {
  uint8_t type;
  uint8_t flat[4];
  uint8_t wall[3][4];
  mapCell() { type = SKY; }
  mapCell(uint8_t _type, uint8_t _flat[4], uint8_t _wall[3][4]) {
    type = _type;
    for (int f = 0; f < 4; f++) {
      flat[f] = _flat[f];
    }
    for (int f = 0; f < 3; f++) {
      for (int d = 0; d < 4; d++) {
        wall[f][d] = _wall[f][d];
      }
    }
  }
};

struct kruskalCell {
  int set;
  bool right;
  bool down;
};

const float unit = 100;
const int MAX_WIDTH = 100;
const int MAX_HEIGHT = 100;
int mazeWidth = 40;
int mazeHeight = 40;
float drawDistance = 30 * unit;
const int MAX_CLIPBOARD_SIZE = 100;

mapCell map[MAX_WIDTH * 2 + 1][MAX_HEIGHT * 2 + 1];
mapCell clipboard[MAX_CLIPBOARD_SIZE][MAX_CLIPBOARD_SIZE];
int clipboardWidth = -1, clipboardHeight = -1;
bool clipboardPreview = false;
kruskalCell kruskalMaze[MAX_WIDTH][MAX_HEIGHT];

struct action {
  int frame;
  int x;
  int y;
  mapCell cell;
  action(mapCell _cell, int _x, int _y, int _frame) {
    x = _x;
    y = _y;
    cell = _cell;
    frame = _frame;
  }
};

std::vector<action> undoBuffer;

void clearMap() {
  for (int i = -mazeWidth; i <= mazeWidth; i++) {
    for (int j = -mazeHeight; j <= mazeHeight; j++) {
      if (i == -mazeWidth || j == -mazeWidth || i == mazeWidth ||
          j == mazeWidth) {
        map[i + mazeWidth][j + mazeHeight].type = WALL;
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i + mazeWidth][j + mazeHeight].wall[f][d] = DEFAULT_WALL;
          }
        }
        for (int f = 0; f < 4; f++) {
          map[i + mazeWidth][j + mazeHeight].flat[f] = DEFAULT_FLAT;
        }
      } else {
        map[i + mazeWidth][j + mazeHeight].type = SKY;
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i + mazeWidth][j + mazeHeight].wall[f][d] = DEFAULT_WALL;
          }
        }
        for (int f = 0; f < 4; f++) {
          map[i + mazeWidth][j + mazeHeight].flat[f] = DEFAULT_FLAT;
        }
      }
    }
  }
}

int clearQuads(int x, int y) {
  int clearCount = 0;
  for (int i = 0; i < quads.size();) {
    if (quads[i].mapX == x && quads[i].mapY == y) {
      quads.erase(quads.begin() + i);
      clearCount++;
    } else {
      i++;
    }
  }
  return clearCount;
}

int setQuadTexture(int x, int y, uint8_t id, bool wall, bool floor = false) {
  if (x < 0 || y < 0 || x > 2 * mazeWidth || y > 2 * mazeWidth) return 0;
  int setCount = 0;
  for (int i = 0; i < quads.size(); i++) {
    if (quads[i].mapX == x && quads[i].mapY == y) {
      if (wall == quads[i].wall) {
        if (!wall &&
            (floor && quads[i].level != 0 || !floor && quads[i].level == 0))
          continue;
        quads[i].renderable = &texture[id];
        setCount++;
      }
    }
  }

  if (wall) {
    for (int f = 0; f < 3; f++) {
      for (int d = 0; d < 4; d++) {
        map[x][y].wall[f][d] = id;
      }
    }
  } else {
    for (int f = 0; f < 4; f++) {
      if (floor && f > 0) break;
      if (!floor && f == 0) continue;
      map[x][y].flat[f] = id;
    }
  }

  return setCount;
}

void regenerateQuads() {
  quads.clear();

  for (int i = -mazeWidth; i <= mazeWidth; i++) {
    for (int j = -mazeHeight; j <= mazeHeight; j++) {
      float x1 = unit * i;
      float y1 = unit * j;
      float x2 = x1 + unit;
      float y2 = y1;
      float x4 = x1;
      float y4 = y1 + unit;
      float x3 = x1 + (x2 - x1) + (x4 - x1);
      float y3 = y1 + (y2 - y1) + (y4 - y1);

      if (map[i + mazeWidth][j + mazeHeight].type &
          CEILING_BIT) {  // ROOF (3, up)
        quads.push_back(
            quad(vertex(x1, y1, -unit * 2, -1), vertex(x2, y2, -unit * 2, -1),
                 vertex(x3, y3, -unit * 2, -1), vertex(x4, y4, -unit * 2, -1),
                 false, i + mazeWidth, j + mazeHeight,
                 map[i + mazeWidth][j + mazeHeight].flat[3], 3));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & LOW_BIT &&
          !(map[i + mazeWidth][j + mazeHeight].type &
            HIGH_BIT)) {  // DOUBLE BLOCK TOP (2, up)
        quads.push_back(
            quad(vertex(x1, y1, -unit, -1), vertex(x2, y2, -unit, -1),
                 vertex(x3, y3, -unit, -1), vertex(x4, y4, -unit, -1), false,
                 i + mazeWidth, j + mazeHeight,
                 map[i + mazeWidth][j + mazeHeight].flat[2], 2));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & WALL_BIT &&
          !(map[i + mazeWidth][j + mazeHeight].type &
            LOW_BIT)) {  // SINGLE BLOCK TOP (1, up)
        quads.push_back(quad(vertex(x1, y1, 0, -1), vertex(x2, y2, 0, -1),
                             vertex(x3, y3, 0, -1), vertex(x4, y4, 0, -1),
                             false, i + mazeWidth, j + mazeHeight,
                             map[i + mazeWidth][j + mazeHeight].flat[1], 1));
      }

      if (!(map[i + mazeWidth][j + mazeHeight].type &
            WALL_BIT)) {  // FLOOR (0, up)
        quads.push_back(quad(vertex(x1, y1, unit, -1), vertex(x2, y2, unit, -1),
                             vertex(x3, y3, unit, -1), vertex(x4, y4, unit, -1),
                             false, i + mazeWidth, j + mazeHeight,
                             map[i + mazeWidth][j + mazeHeight].flat[0], 0));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & LOW_BIT &&
          !(map[i + mazeWidth][j + mazeHeight].type &
            WALL_BIT)) {  // CORRIDOR CEILING (1, down)
        quads.push_back(quad(vertex(x4, y4, 0, -1), vertex(x3, y3, 0, -1),
                             vertex(x2, y2, 0, -1), vertex(x1, y1, 0, -1),
                             false, i + mazeWidth, j + mazeHeight,
                             map[i + mazeWidth][j + mazeHeight].flat[1], 1));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & HIGH_BIT &&
          !(map[i + mazeWidth][j + mazeHeight].type &
            LOW_BIT)) {  // LOW ROOM CEILING (2, down)
        quads.push_back(
            quad(vertex(x4, y4, -unit, -1), vertex(x3, y3, -unit, -1),
                 vertex(x2, y2, -unit, -1), vertex(x1, y1, -unit, -1), false,
                 i + mazeWidth, j + mazeHeight,
                 map[i + mazeWidth][j + mazeHeight].flat[2], 2));
      }

      if (map[i + mazeWidth][j + mazeHeight].type & CEILING_BIT &&
          !(map[i + mazeWidth][j + mazeHeight].type &
            HIGH_BIT)) {  // HIGH ROOM CEILING (3, down)
        quads.push_back(
            quad(vertex(x4, y4, -unit * 2, -1), vertex(x3, y3, -unit * 2, -1),
                 vertex(x2, y2, -unit * 2, -1), vertex(x1, y1, -unit * 2, -1),
                 false, i + mazeWidth, j + mazeHeight,
                 map[i + mazeWidth][j + mazeHeight].flat[3], 3));
      }

      for (int level = 0; level < 3; level++) {
        uint8_t LEVEL_BIT;
        float zTop;
        float zBottom;
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
          quads.push_back(quad(
              vertex(x2, y2, zBottom, 3), vertex(x2, y2, zTop, 2),
              vertex(x1, y1, zTop, 1), vertex(x1, y1, zBottom, 0), true,
              i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[level][0], level, 0));
        }

        if (i < mazeWidth - 1 &&
            !(map[i + mazeWidth + 1][j + mazeHeight].type & LEVEL_BIT)) {
          quads.push_back(quad(
              vertex(x3, y3, zBottom, 3), vertex(x3, y3, zTop, 2),
              vertex(x2, y2, zTop, 1), vertex(x2, y2, zBottom, 0), true,
              i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[level][1], level, 1));
        }

        if (j < mazeHeight - 1 &&
            !(map[i + mazeWidth][j + mazeHeight + 1].type & LEVEL_BIT)) {
          quads.push_back(quad(
              vertex(x4, y4, zBottom, 3), vertex(x4, y4, zTop, 2),
              vertex(x3, y3, zTop, 1), vertex(x3, y3, zBottom, 0), true,
              i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[level][2], level, 2));
        }

        if (i > -mazeHeight &&
            !(map[i + mazeWidth - 1][j + mazeHeight].type & LEVEL_BIT)) {
          quads.push_back(quad(
              vertex(x1, y1, zBottom, 3), vertex(x1, y1, zTop, 2),
              vertex(x4, y4, zTop, 1), vertex(x4, y4, zBottom, 0), true,
              i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[level][3], level, 3));
        }
      }
    }
  }
}

void makeEntities(int count) {
  entities.clear();

  for (int i = 0; i < count; i++) {
  tryAgain:

    int x = std::rand() % (mazeWidth * 2) - mazeWidth;
    int y = std::rand() % (mazeHeight * 2) - mazeHeight;
    int s = std::rand() % 50;

    for (auto e : entities) {
      if (e.mapX == x && e.mapY == y) goto tryAgain;
    }

    entities.push_back(
        entity(vertex(static_cast<float>(x + 0.5) * unit,
                      static_cast<float>(y + 0.5) * unit, 7 * unit / 8, -1),
               x, y, s));
  }
}

bool kruskalStep() {
  bool oneSet = true;
  for (int i = 0; i < mazeWidth; i++) {
    for (int j = 0; j < mazeHeight; j++) {
      if (kruskalMaze[i][j].set != kruskalMaze[0][0].set) {
        oneSet = false;
      }
    }
  }
  if (oneSet) return true;

  int x, y, a, b, horizontal = 0, vertical = 0;

  while (true) {
    x = std::rand() % mazeWidth;
    y = std::rand() % mazeHeight;

    if (std::rand() % 2 == 0) {
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
    for (int i = 0; i < mazeWidth; i++) {
      for (int j = 0; j < mazeHeight; j++) {
        if (kruskalMaze[i][j].set == b) {
          kruskalMaze[i][j].set = a;
        }
      }
    }

    return false;
  }
}

void makeMaze() {
  int n = 0;
  for (int i = 0; i < mazeWidth; i++) {
    for (int j = 0; j < mazeHeight; j++) {
      n++;
      kruskalMaze[i][j].set = n;
      kruskalMaze[i][j].right = false;
      kruskalMaze[i][j].down = false;
    }
  }

  bool mazeDone = false;
  while (!mazeDone) {
    mazeDone = kruskalStep();
  }

  for (int i = -mazeWidth; i <= mazeWidth; i++) {
    for (int j = -mazeHeight; j <= mazeHeight; j++) {
      map[i + mazeWidth][j + mazeHeight].type =
          (i + mazeWidth) % 2 == 1 && (j + mazeHeight) % 2 == 1
              ? GENERATOR_PATH
              : GENERATOR_WALL;
      for (int f = 0; f < 4; f++) {
        map[i + mazeWidth][j + mazeHeight].flat[f] = DEFAULT_FLAT;
      }
      for (int f = 0; f < 3; f++) {
        for (int d = 0; d < 4; d++) {
          map[i + mazeWidth][j + mazeHeight].wall[f][d] = DEFAULT_WALL;
        }
      }
    }
  }

  for (int i = 0; i < mazeWidth; i++) {
    for (int j = 0; j < mazeHeight; j++) {
      if (kruskalMaze[i][j].right) {
        map[i * 2 + 2][j * 2 + 1].type = GENERATOR_PATH;
        for (int f = 0; f < 4; f++) {
          map[i * 2 + 2][j * 2 + 1].flat[f] = DEFAULT_FLAT;
        }
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i * 2 + 2][j * 2 + 1].wall[f][d] = DEFAULT_WALL;
          }
        }
      }
      if (kruskalMaze[i][j].down) {
        map[i * 2 + 1][j * 2 + 2].type = GENERATOR_PATH;
        for (int f = 0; f < 4; f++) {
          map[i * 2 + 1][j * 2 + 2].flat[f] = DEFAULT_FLAT;
        }
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i * 2 + 1][j * 2 + 2].wall[f][d] = DEFAULT_WALL;
          }
        }
      }
    }
  }

  for (int r = 0; r < 10; r++) {
    int rw = std::rand() % 10 + 5;
    int rh = std::rand() % 10 + 5;
    int x = std::rand() % (mazeWidth * 2 - rw);
    int y = std::rand() % (mazeWidth * 2 - rh);
    for (int i = x; i <= x + rw; i++) {
      for (int j = y; j <= y + rh; j++) {
        map[i][j].type = GENERATOR_ROOM;
        for (int f = 0; f < 4; f++) {
          map[i][j].flat[f] = DEFAULT_FLAT;
        }
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i][j].wall[f][d] = DEFAULT_WALL;
          }
        }
      }
    }
  }
}

class Olc3d2 : public olc::PixelGameEngine {
 public:
  Olc3d2() { sAppName = "Olc3d2"; }

  long frame = 0;

  float zoom = w / 2;
  bool day = true;
  bool showHelp = true;
  bool noClip = false;
  bool autoTexture = false;
  int levelNo = 1;
  bool exitSignal = false;
  bool editMode = true;

  float editCameraAngle, editCameraPitch, editCameraX, editCameraY, editCameraZ;

  int selectionStartX;
  int selectionStartY;
  int selectionEndX;
  int selectionEndY;
  bool selectionLive = false;

  void saveLevel() {
    std::ostringstream filename;
    filename << "./maps/level" << levelNo << ".dat";

    std::ifstream source(filename.str(), std::ios::in | std::ios::binary);
    if (source.good()) {
      std::ofstream backup(filename.str() + "_bak",
                           std::ios::out | std::ios::binary);
      backup << source.rdbuf();
    }

    std::ofstream output(filename.str(), std::ios::out | std::ios::binary);
    if (!output) {
      std::cout << "Cannot open file!" << std::endl;
      return;
    }

    int version = 1;
    output.write((char*)&version, sizeof(version));
    output.write((char*)&mazeWidth, sizeof(mazeWidth));
    output.write((char*)&mazeHeight, sizeof(mazeHeight));
    output.write((char*)&day, sizeof(day));

    for (int i = -mazeWidth; i <= mazeWidth; i++) {
      for (int j = -mazeHeight; j <= mazeHeight; j++) {
        output.write((char*)&map[i + mazeWidth][j + mazeHeight],
                     sizeof(mapCell));
      }
    }

    output.close();
    if (!output.good()) {
      std::cout << "Error occurred saving level." << std::endl;
      return;
    }

    std::cout << "Level saved: " << filename.str() << std::endl;
  }

  void loadLevel() {
    std::ostringstream filename;
    filename << "./maps/level" << levelNo << ".dat";

    std::ifstream input(filename.str(), std::ios::in | std::ios::binary);
    if (!input) {
      std::cout << "Cannot open file!" << std::endl;
      return;
    }

    int version;
    input.read((char*)&version, sizeof(version));
    input.read((char*)&mazeWidth, sizeof(mazeWidth));
    input.read((char*)&mazeHeight, sizeof(mazeHeight));
    input.read((char*)&day, sizeof(day));

    for (int i = -mazeWidth; i <= mazeWidth; i++) {
      for (int j = -mazeHeight; j <= mazeHeight; j++) {
        input.read((char*)&map[i + mazeWidth][j + mazeHeight], sizeof(mapCell));
      }
    }
    input.close();
    if (!input.good()) {
      std::cout << "Error occurred loading level." << std::endl;
      return;
    }

    regenerateQuads();

    std::cout << "Level loaded: " << filename.str() << std::endl;
  }

 public:
  bool OnUserCreate() override {
    for (int i = 0; i < 176; i++) {
      std::ostringstream filename;
      filename << "./tiles/tile" << i << ".png";
      texture[i].Load(filename.str());
    }

    for (int i = 0; i < 50; i++) {
      std::ostringstream filename;
      filename << "./items/item" << (i + 1) << ".png";
      sprite[i].Load(filename.str());
    }

    std::srand(std::time(nullptr));
    loadLevel();
    makeEntities(1000);

    return true;
  }

  float cameraAngle = 0;
  float cameraPitch = 0;
  float cameraX = unit / 2;
  float cameraY = unit / 2;
  float cameraZ = unit / 2;

  float playerX, playerY, playerZ, playerAngle;
  float lastPlayerX, lastPlayerY;

  olc::vi2d mousePos;
  olc::vi2d lastMousePos;
  int cursorX, cursorY;
  int cursorRotation = 0;

  uint8_t selectedTexture = 1;
  int selectedBlock = 0;

  uint8_t selectedBlockTypes[12]{SKY,
                                 SKY_SINGLE_BLOCK,
                                 SKY_FLOATING_BLOCK,
                                 SKY_DOUBLE_BLOCK,
                                 LOW_ROOM,
                                 LOW_ROOM_SINGLE_BLOCK,
                                 CORRIDOR,
                                 WALL,
                                 HIGH_ROOM,
                                 HIGH_ROOM_SINGLE_BLOCK,
                                 HIGH_ROOM_FLOATING_BLOCK,
                                 HIGH_ROOM_DOUBLE_BLOCK};

  int qMax = 0;

  float m[3][3];

  void updateMatrix() {
    float m1[3][3] = {{1, 0, 0},
                      {0, std::cos(cameraPitch), std::sin(cameraPitch)},
                      {0, -std::sin(cameraPitch), std::cos(cameraPitch)}};

    float m2[3][3] = {{std::cos(cameraAngle), -std::sin(cameraAngle), 0},
                      {std::sin(cameraAngle), std::cos(cameraAngle), 0},
                      {0, 0, 1}};

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        m[i][j] =
            m1[i][0] * m2[0][j] + m1[i][1] * m2[1][j] + m1[i][2] * m2[2][j];
      }
    }
  }

  void playProcessing(float frameLength) {
    cameraX = playerX - 6 * unit * std::sin(playerAngle);
    cameraY = playerY - 6 * unit * std::cos(playerAngle);
    cameraZ = playerZ - 3 * unit;
    cameraAngle = playerAngle;
  }

  void handlePlayInputs(float frameLength) {
    if (GetKey(olc::Key::P).bPressed && GetKey(olc::Key::CTRL).bHeld) {
      cameraAngle = editCameraAngle;
      cameraPitch = editCameraPitch;
      cameraX = editCameraX;
      cameraY = editCameraY;
      cameraZ = editCameraZ;
      editMode = true;
      return;
    }

    if (GetKey(olc::Key::Q).bPressed && GetKey(olc::Key::CTRL).bHeld) {
      exitSignal = true;
    }

    if (GetKey(olc::Key::N).bPressed && GetKey(olc::Key::CTRL).bHeld) {
      noClip = !noClip;
    }

    lastPlayerX = playerX;
    lastPlayerY = playerY;

    if (GetKey(olc::Key::LEFT).bHeld) playerAngle += 2 * frameLength;
    if (GetKey(olc::Key::RIGHT).bHeld) playerAngle -= 2 * frameLength;

    if (GetKey(olc::Key::W).bHeld) {
      playerX += std::sin(playerAngle) * unit * 2 * frameLength;
      playerY += std::cos(playerAngle) * unit * 2 * frameLength;
    }
    if (GetKey(olc::Key::S).bHeld) {
      playerX -= std::sin(playerAngle) * unit * 2 * frameLength;
      playerY -= std::cos(playerAngle) * unit * 2 * frameLength;
    }
    if (GetKey(olc::Key::A).bHeld) {
      playerX += std::cos(playerAngle) * unit * 2 * frameLength;
      playerY += -std::sin(playerAngle) * unit * 2 * frameLength;
    }
    if (GetKey(olc::Key::D).bHeld) {
      playerX -= std::cos(playerAngle) * unit * 2 * frameLength;
      playerY -= -std::sin(playerAngle) * unit * 2 * frameLength;
    }
  }

  void handleEditInputs(float frameLength) {
    mousePos = GetMousePos();

    if (GetKey(olc::Key::P).bPressed && GetKey(olc::Key::CTRL).bHeld) {
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

    if (GetMouse(2).bHeld || GetKey(olc::Key::OEM_5).bHeld) {
      int iMax = static_cast<int>(w / (TILE_SIZE + 10));
      int jMax = static_cast<int>(176 / iMax) + 1;

      selectedTexture =
          static_cast<int>(mousePos.y / (TILE_SIZE + 10.0f)) * iMax +
          static_cast<int>(mousePos.x < (iMax - 1) * (TILE_SIZE + 10.0f)
                               ? mousePos.x / (TILE_SIZE + 10.0f)
                               : (iMax - 1));

      if (selectedTexture < 0) selectedTexture = 0;
      if (selectedTexture > 175) selectedTexture = 175;

    } else {
      int mouseWheel = GetMouseWheel();

      if (mouseWheel < 0) selectedBlock = (selectedBlock + 1) % 12;
      if (mouseWheel > 0) selectedBlock = (selectedBlock + 11) % 12;

      if (GetKey(olc::Key::LEFT).bHeld) cameraAngle += 2 * frameLength;
      if (GetKey(olc::Key::RIGHT).bHeld) cameraAngle -= 2 * frameLength;

      if (cursorX >= 0 && cursorY >= 0 && cursorX <= mazeWidth * 2 &&
          cursorY <= mazeHeight * 2) {
        if (GetMouse(0).bHeld && quad::cursorQuad != nullptr) {
          int x = quad::cursorQuad->mapX;
          int y = quad::cursorQuad->mapY;
          int f = quad::cursorQuad->level;
          int d = quad::cursorQuad->direction;

          uint8_t candidateTexture;
          if (quad::cursorQuad->wall) {
            candidateTexture = map[x][y].wall[f][d];
          } else {
            candidateTexture = map[x][y].flat[f];
          }

          if (candidateTexture != selectedTexture) {
            undoBuffer.push_back(action(map[x][y], x, y, frame));

            if (quad::cursorQuad->wall) {
              map[x][y].wall[f][d] = selectedTexture;
            } else {
              map[x][y].flat[f] = selectedTexture;
            }

            quad::cursorQuad->renderable = &texture[selectedTexture];
          }

        } else if (GetMouse(1).bHeld && quad::cursorQuad != nullptr) {
          cameraAngle -= static_cast<float>(mousePos.x - lastMousePos.x) /
                         (w / 4) * ((w / 2) / zoom);

        } else if (GetKey(olc::Key::Q).bPressed) {
          int x = quad::cursorQuad->mapX;
          int y = quad::cursorQuad->mapY;
          int f = quad::cursorQuad->level;
          int d = quad::cursorQuad->direction;

          if (quad::cursorQuad->wall) {
            selectedTexture = map[x][y].wall[f][d];
          } else if (f == 0) {
            selectedTexture = map[x][y].flat[f];
          }

        } else if ((GetKey(olc::Key::T).bPressed ||
                    GetKey(olc::Key::F).bPressed ||
                    GetKey(olc::Key::C).bPressed) &&
                   !GetKey(olc::Key::CTRL).bHeld) {
          if (selectionLive) {
            int x1 = selectionStartX < selectionEndX ? selectionStartX
                                                     : selectionEndX;
            int y1 = selectionStartY < selectionEndY ? selectionStartY
                                                     : selectionEndY;
            int x2 = selectionStartX > selectionEndX ? selectionStartX
                                                     : selectionEndX;
            int y2 = selectionStartY > selectionEndY ? selectionStartY
                                                     : selectionEndY;

            if (x2 - x1 >= MAX_CLIPBOARD_SIZE) x2 = x1 + MAX_CLIPBOARD_SIZE - 1;
            if (y2 - y1 >= MAX_CLIPBOARD_SIZE) y2 = y1 + MAX_CLIPBOARD_SIZE - 1;

            for (int i = x1; i <= x2; i++) {
              for (int j = y1; j <= y2; j++) {
                undoBuffer.push_back(action(map[i][j], i, j, frame));
                if (GetKey(olc::Key::T).bPressed)
                  setQuadTexture(i, j, selectedTexture, true);
                if (GetKey(olc::Key::F).bPressed)
                  setQuadTexture(i, j, selectedTexture, false, true);
                if (GetKey(olc::Key::C).bPressed)
                  setQuadTexture(i, j, selectedTexture, false);
              }
            }

            selectionLive = false;

          } else {
            int x = quad::cursorQuad->mapX;
            int y = quad::cursorQuad->mapY;
            undoBuffer.push_back(action(map[x][y], x, y, frame));

            if (GetKey(olc::Key::T).bPressed)
              setQuadTexture(x, y, selectedTexture, true);
            if (GetKey(olc::Key::F).bPressed)
              setQuadTexture(x, y, selectedTexture, false, true);
            if (GetKey(olc::Key::C).bPressed)
              setQuadTexture(x, y, selectedTexture, false);
          }
        }
      }

      if (GetKey(olc::Key::R).bPressed) {
        if (GetKey(olc::Key::CTRL).bHeld) {
          cursorRotation = (cursorRotation + 7) % 8;
        } else {
          cursorRotation = (cursorRotation + 1) % 8;
        }
      }

      if (GetKey(olc::Key::ESCAPE).bPressed) {
        selectionLive = false;
      }

      if (GetKey(olc::Key::SHIFT).bPressed && quad::cursorQuad != nullptr) {
        selectionStartX = quad::cursorQuad->mapX;
        selectionStartY = quad::cursorQuad->mapY;
        selectionEndX = quad::cursorQuad->mapX;
        selectionEndY = quad::cursorQuad->mapY;
        selectionLive = true;
      }

      if (GetKey(olc::Key::SHIFT).bHeld && quad::cursorQuad != nullptr) {
        selectionEndX = quad::cursorQuad->mapX;
        selectionEndY = quad::cursorQuad->mapY;
      }

      if (GetKey(olc::Key::CTRL).bHeld) {
        if (GetKey(olc::Key::K1).bPressed) {
          levelNo = 1;
        } else if (GetKey(olc::Key::K2).bPressed) {
          levelNo = 2;
        } else if (GetKey(olc::Key::K3).bPressed) {
          levelNo = 3;
        } else if (GetKey(olc::Key::K4).bPressed) {
          levelNo = 4;
        } else if (GetKey(olc::Key::K5).bPressed) {
          levelNo = 5;
        } else if (GetKey(olc::Key::K6).bPressed) {
          levelNo = 6;
        } else if (GetKey(olc::Key::K7).bPressed) {
          levelNo = 7;
        } else if (GetKey(olc::Key::K8).bPressed) {
          levelNo = 8;
        } else if (GetKey(olc::Key::K9).bPressed) {
          levelNo = 9;
        } else if (GetKey(olc::Key::K0).bPressed) {
          levelNo = 10;
        }

        if (GetKey(olc::Key::Z).bPressed && undoBuffer.size() > 0) {
          action u = undoBuffer.back();
          int f = u.frame;
          while (u.frame == f) {
            map[u.x][u.y] = u.cell;
            undoBuffer.pop_back();
            if (undoBuffer.size() == 0) break;
            u = undoBuffer.back();
          }
          regenerateQuads();
        }

        if (GetKey(olc::Key::E).bPressed) {
          makeEntities(1000);
        }

        if (GetKey(olc::Key::T).bPressed) {
          autoTexture = !autoTexture;
        }

        if (GetKey(olc::Key::Q).bPressed) {
          exitSignal = true;
        }

        if (GetKey(olc::Key::S).bPressed) {
          saveLevel();
        }

        if (GetKey(olc::Key::L).bPressed) {
          undoBuffer.clear();
          loadLevel();
        }

        if (GetKey(olc::Key::M).bPressed) {
          day = true;
          Clear(olc::Pixel(96, 128, 255));
          clearMap();
          undoBuffer.clear();
          regenerateQuads();
        }

        if (GetKey(olc::Key::G).bPressed) {
          day = false;
          Clear(olc::BLACK);
          makeMaze();
          undoBuffer.clear();
          regenerateQuads();
        }

        if (GetKey(olc::Key::HOME).bPressed) {
          zoom = w / 2;
          drawDistance = 30 * unit;
        }

      } else {
        if (GetKey(olc::Key::K1).bPressed) {
          selectedBlock = 0;
        } else if (GetKey(olc::Key::K2).bPressed) {
          selectedBlock = 1;
        } else if (GetKey(olc::Key::K3).bPressed) {
          selectedBlock = 2;
        } else if (GetKey(olc::Key::K4).bPressed) {
          selectedBlock = 3;
        } else if (GetKey(olc::Key::K5).bPressed) {
          selectedBlock = 4;
        } else if (GetKey(olc::Key::K6).bPressed) {
          selectedBlock = 5;
        } else if (GetKey(olc::Key::K7).bPressed) {
          selectedBlock = 6;
        } else if (GetKey(olc::Key::K8).bPressed) {
          selectedBlock = 7;
        } else if (GetKey(olc::Key::K9).bPressed) {
          selectedBlock = 8;
        } else if (GetKey(olc::Key::K0).bPressed) {
          selectedBlock = 9;
        } else if (GetKey(olc::Key::MINUS).bPressed) {
          selectedBlock = 10;
        } else if (GetKey(olc::Key::EQUALS).bPressed) {
          selectedBlock = 11;
        }
      }

      if (GetKey(olc::Key::OEM_4).bHeld) drawDistance /= 1.01;
      if (GetKey(olc::Key::OEM_6).bHeld) drawDistance *= 1.01;

      if (drawDistance < unit) drawDistance = unit;
      if (drawDistance > unit * mazeWidth * 2)
        drawDistance = unit * mazeWidth * 2;

      if (GetKey(olc::Key::DOWN).bHeld) cameraPitch += 2 * frameLength;
      if (GetKey(olc::Key::UP).bHeld) cameraPitch -= 2 * frameLength;
      if (GetKey(olc::Key::HOME).bHeld) cameraPitch = 0;

      if (GetKey(olc::Key::PGUP).bHeld) cameraZ -= unit * 2 * frameLength;
      if (GetKey(olc::Key::PGDN).bHeld) cameraZ += unit * 2 * frameLength;
      if (GetKey(olc::Key::END).bHeld) cameraZ = unit / 2;

      if (cameraZ > unit / 2) cameraZ = unit / 2;

      if (GetKey(olc::Key::C).bPressed && GetKey(olc::Key::CTRL).bHeld) {
        if (selectionLive) {
          int x1 =
              selectionStartX < selectionEndX ? selectionStartX : selectionEndX;
          int y1 =
              selectionStartY < selectionEndY ? selectionStartY : selectionEndY;
          int x2 =
              selectionStartX > selectionEndX ? selectionStartX : selectionEndX;
          int y2 =
              selectionStartY > selectionEndY ? selectionStartY : selectionEndY;

          if (x2 - x1 >= MAX_CLIPBOARD_SIZE) x2 = x1 + MAX_CLIPBOARD_SIZE - 1;
          if (y2 - y1 >= MAX_CLIPBOARD_SIZE) y2 = y1 + MAX_CLIPBOARD_SIZE - 1;

          clipboardWidth = x2 - x1 + 1;
          clipboardHeight = y2 - y1 + 1;
          cursorRotation = 0;

          for (int i = x1; i <= x2; i++) {
            for (int j = y1; j <= y2; j++) {
              clipboard[i - x1][j - y1] = map[i][j];
            }
          }
        }
      }

      if (quad::cursorQuad != nullptr) {
        int x = quad::cursorQuad->mapX;
        int y = quad::cursorQuad->mapY;

        if (GetKey(olc::Key::SPACE).bPressed) {
          if (selectionLive) {
            int x1 = selectionStartX < selectionEndX ? selectionStartX
                                                     : selectionEndX;
            int y1 = selectionStartY < selectionEndY ? selectionStartY
                                                     : selectionEndY;
            int x2 = selectionStartX > selectionEndX ? selectionStartX
                                                     : selectionEndX;
            int y2 = selectionStartY > selectionEndY ? selectionStartY
                                                     : selectionEndY;

            if (x2 - x1 >= MAX_CLIPBOARD_SIZE) x2 = x1 + MAX_CLIPBOARD_SIZE - 1;
            if (y2 - y1 >= MAX_CLIPBOARD_SIZE) y2 = y1 + MAX_CLIPBOARD_SIZE - 1;

            for (int i = x1; i <= x2; i++) {
              for (int j = y1; j <= y2; j++) {
                undoBuffer.push_back(action(map[i][j], i, j, frame));
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
            undoBuffer.push_back(action(map[x][y], x, y, frame));
            map[x][y].type = selectedBlockTypes[selectedBlock];
            regenerateQuads();
            if (autoTexture) {
              setQuadTexture(x, y, selectedTexture, true);
              setQuadTexture(x, y, selectedTexture, false);
              setQuadTexture(x, y, selectedTexture, false, true);
            }
          }
          quad::cursorQuad = nullptr;
        }

        clipboardPreview = GetKey(olc::Key::V).bHeld;

        if (GetKey(olc::Key::V).bPressed && GetKey(olc::Key::CTRL).bHeld) {
          for (int i = 0; i < clipboardWidth; i++) {
            for (int j = 0; j < clipboardHeight; j++) {
              switch (cursorRotation) {
                case 0:
                  if (x + i < 0 || y + j < 0 || x + i > 2 * mazeWidth ||
                      y + j > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x + i][y + j], x + i, y + j, frame));
                  map[x + i][y + j] = clipboard[i][j];
                  break;

                case 1:
                  if (x + j < 0 || y + i < 0 || x + j > 2 * mazeWidth ||
                      y + i > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x + j][y + i], x + j, y + i, frame));
                  map[x + j][y + i] = clipboard[i][j];
                  break;

                case 2:
                  if (x - j < 0 || y + i < 0 || x - j > 2 * mazeWidth ||
                      y + i > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x - j][y + i], x - j, y + i, frame));
                  map[x - j][y + i] = clipboard[i][j];
                  break;

                case 3:
                  if (x - i < 0 || y + j < 0 || x - i > 2 * mazeWidth ||
                      y + j > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x - i][y + j], x - i, y + j, frame));
                  map[x - i][y + j] = clipboard[i][j];
                  break;

                case 4:
                  if (x - i < 0 || y - j < 0 || x - i > 2 * mazeWidth ||
                      y - j > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x - i][y - j], x - i, y - j, frame));
                  map[x - i][y - j] = clipboard[i][j];
                  break;

                case 5:
                  if (x - j < 0 || y - i < 0 || x - j > 2 * mazeWidth ||
                      y - i > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x - j][y - i], x - j, y - i, frame));
                  map[x - j][y - i] = clipboard[i][j];
                  break;

                case 6:
                  if (x + i < 0 || y - i < 0 || x + j > 2 * mazeWidth ||
                      y - i > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x + j][y - i], x + j, y - i, frame));
                  map[x + j][y - i] = clipboard[i][j];
                  break;

                case 7:
                  if (x + i < 0 || y - j < 0 || x + i > 2 * mazeWidth ||
                      y - j > 2 * mazeWidth)
                    continue;
                  undoBuffer.push_back(
                      action(map[x + i][y - j], x + i, y - j, frame));
                  map[x + i][y - j] = clipboard[i][j];
                  break;
              }
            }
          }
          regenerateQuads();
        }

        if (GetKey(olc::Key::E).bPressed && !GetKey(olc::Key::CTRL).bHeld) {
          for (int i = 0; i < 12; i++) {
            if (map[x][y].type == selectedBlockTypes[i]) {
              selectedBlock = i;
            }
          }
        }
      }

      if (GetKey(olc::Key::PERIOD).bHeld) zoom *= 1.05;
      if (GetKey(olc::Key::COMMA).bHeld) zoom /= 1.05;
      if (zoom < w / 4) zoom = w / 4;
      if (zoom > w * 5) zoom = w * 5;

      if (GetKey(olc::Key::N).bPressed && !GetKey(olc::Key::CTRL).bHeld) {
        day = !day;
        if (day) {
          Clear(olc::Pixel(96, 128, 255));
        } else {
          Clear(olc::BLACK);
        }
      }

      if (GetKey(olc::Key::H).bPressed) {
        showHelp = !showHelp;
      }

      if (cameraPitch < -1.571) cameraPitch = -1.571;
      if (cameraPitch > 1.571) cameraPitch = 1.571;

      if (!GetKey(olc::Key::CTRL).bHeld) {
        if (GetKey(olc::Key::W).bHeld) {
          cameraX += std::sin(cameraAngle) * unit * 2 * frameLength;
          cameraY += std::cos(cameraAngle) * unit * 2 * frameLength;
        }
        if (GetKey(olc::Key::S).bHeld) {
          cameraX -= std::sin(cameraAngle) * unit * 2 * frameLength;
          cameraY -= std::cos(cameraAngle) * unit * 2 * frameLength;
        }
        if (GetKey(olc::Key::A).bHeld) {
          cameraX += std::cos(cameraAngle) * unit * 2 * frameLength;
          cameraY += -std::sin(cameraAngle) * unit * 2 * frameLength;
        }
        if (GetKey(olc::Key::D).bHeld) {
          cameraX -= std::cos(cameraAngle) * unit * 2 * frameLength;
          cameraY -= -std::sin(cameraAngle) * unit * 2 * frameLength;
        }
      }
    }
  }

  void handlePlayerInteractions() {
    int mapX = playerX / unit + mazeWidth;
    int mapY = playerY / unit + mazeHeight;

    int mapZ = 0;
    if (playerZ < -5 * unit / 2 || playerZ > unit / 2)
      return;
    else if (playerZ < -3 * unit / 2)
      mapZ = 2;
    else if (playerZ < -unit / 2)
      mapZ = 1;

    uint8_t INTERACTION_BIT;
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

    float dx = playerX - lastPlayerX;
    float dy = playerY - lastPlayerY;

    if (mapX > 0 && mapY > 0 && mapX < mazeWidth * 2 + 1 &&
        mapY < mazeWidth * 2 + 1) {
      bool north = false, south = false, east = false, west = false;
      bool northEast = false, southEast = false, northWest = false,
           southWest = false;

      double intBit;
      float eastWest = std::modf(playerX / unit + mazeWidth - mapX, &intBit);
      float northSouth = std::modf(playerY / unit + mazeHeight - mapY, &intBit);

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

      eastWest = std::modf(playerX / unit + mazeWidth - mapX, &intBit);
      northSouth = std::modf(playerY / unit + mazeHeight - mapY, &intBit);

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

  void updateEntities() {
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

  void updateQuads() {
    int levelQuads = quads.size();

    for (int quadCounter = 0; quadCounter < quads.size(); quadCounter++) {
      quad* q = &quads[quadCounter];

      q->outOfRange = std::abs(q->vertices[0].x - cameraX) > drawDistance ||
                      std::abs(q->vertices[0].y - cameraY) > drawDistance;

      if (q->outOfRange) continue;

      float maxX = 0;
      float maxY = 0;
      float maxZ = 0;

      for (int i = 0; i < 4; i++) {
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
        int lostCornerCount = 0;

        for (int j = 0; j < 4; j++) {
          if (q->transformed[j].y < OMEGA) lostCornerCount++;
        }

        if (lostCornerCount > 0) {
          q->visible = false;

          if (lostCornerCount < 4) {
            int n = 0;
            float nY = 0;
            for (int j = 0; j < 4; j++) {
              if (q->transformed[j].y > nY) {
                nY = q->transformed[j].y;
                n = j;
              }
            }

            float x0 = q->transformed[n].x;
            float y0 = q->transformed[n].y;
            float z0 = q->transformed[n].z;

            float x3 = q->transformed[(n + 2) % 4].x;
            float y3 = q->transformed[(n + 2) % 4].y;
            float z3 = q->transformed[(n + 2) % 4].z;

            float d0 = std::sqrt(std::pow(x3 - x0, 2) + std::pow(y3 - y0, 2));
            float dx0 = (x3 - x0) / d0;
            float dy0 = (y3 - y0) / d0;

            float lambdaY = OMEGA;
            float lambda = (lambdaY - y0) / dy0;
            float lambdaX = x0 + lambda * dx0;

            float x1 = q->transformed[(n + 1) % 4].x;
            float y1 = q->transformed[(n + 1) % 4].y;
            float z1 = q->transformed[(n + 1) % 4].z;
            float d1 = std::sqrt(std::pow(x1 - x0, 2) + std::pow(y1 - y0, 2) +
                                 std::pow(z1 - z0, 2));
            float dx1 = (x1 - x0) / d1;
            float dy1 = (y1 - y0) / d1;
            float dz1 = (z1 - z0) / d1;

            float x2 = q->transformed[(n + 3) % 4].x;
            float y2 = q->transformed[(n + 3) % 4].y;
            float z2 = q->transformed[(n + 3) % 4].z;
            float d2 = std::sqrt(std::pow(x2 - x0, 2) + std::pow(y2 - y0, 2) +
                                 std::pow(z2 - z0, 2));
            float dx2 = (x2 - x0) / d2;
            float dy2 = (y2 - y0) / d2;
            float dz2 = (z2 - z0) / d2;

            float alpha = (lambdaX - x0) * dx1 + (lambdaY - y0) * dy1;

            float xAlpha = x0 + dx1 * alpha;
            float yAlpha = y0 + dy1 * alpha;
            float zAlpha = z0 + dz1 * alpha;

            float xBeta = x0 + dx2 * alpha;
            float yBeta = y0 + dy2 * alpha;
            float zBeta = z0 + dz2 * alpha;

            float xGamma = x0 + dx1 * alpha + dx2 * alpha;
            float yGamma = y0 + dy1 * alpha + dy2 * alpha;
            float zGamma = z0 + dz1 * alpha + dz2 * alpha;

            vertex vs[4] = {vertex(x0, y0, z0, -1),
                            vertex(xAlpha, yAlpha, zAlpha, -1),
                            vertex(xGamma, yGamma, zGamma, -1),
                            vertex(xBeta, yBeta, zBeta, -1)};

            float u, v, du, dv;
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

            q->partials.push_back(partial(
                {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                {static_cast<int>(u), static_cast<int>(v), static_cast<int>(du),
                 static_cast<int>(dv)},
                q->colour));

            if (y1 >= OMEGA) {
              float yTheta = OMEGA;
              float theta = (yTheta - y1) / (y3 - y1);
              float xTheta = x1 + theta * (x3 - x1);
              float zTheta = z1 + theta * (z3 - z1);

              vertex vs[4]{
                  vertex(xAlpha, yAlpha, zAlpha, -1), vertex(x1, y1, z1, -1),
                  vertex(xTheta, yTheta, zTheta, -1),
                  vertex(xTheta - (x1 - xAlpha), yTheta - (y1 - yAlpha),
                         zTheta - (z1 - zAlpha), -1)};

              float u, v, du, dv;
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

              q->partials.push_back(partial(
                  {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                  {static_cast<int>(u), static_cast<int>(v),
                   static_cast<int>(du), static_cast<int>(dv)},
                  q->colour));
            }

            if (y2 >= OMEGA) {
              float yTheta = OMEGA;
              float theta = (yTheta - y2) / (y3 - y2);
              float xTheta = x2 + theta * (x3 - x2);
              float zTheta = z2 + theta * (z3 - z2);

              vertex vs[4]{
                  vertex(xBeta, yBeta, zBeta, -1),
                  vertex(xTheta - (x2 - xBeta), yTheta - (y2 - yBeta),
                         zTheta - (z2 - zBeta), -1),
                  vertex(xTheta, yTheta, zTheta, -1),
                  vertex(x2, y2, z2, -1),
              };

              float u, v, du, dv;
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

              q->partials.push_back(partial(
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

        bool cropped[4]{false, false, false, false};

        int cropCount = 0;

        for (int i = 0; i < 4; i++) {
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
          int key = -1;
          float highestY = OMEGA;
          for (int i = 0; i < 4; i++) {
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

          float keyY = q->transformed[key].y;
          float pairY = q->transformed[q->vertices[key].pair].y;

          float delta = 1 - (OMEGA - pairY) / (keyY - pairY);

          for (int i = 0; i < 4; i++) {
            if (!cropped[i]) continue;
            int pair = q->vertices[i].pair;

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

      float ax = q->transformed[1].x - q->transformed[0].x;
      float ay = q->transformed[1].y - q->transformed[0].y;
      float az = q->transformed[1].z - q->transformed[0].z;

      float bx = q->transformed[3].x - q->transformed[0].x;
      float by = q->transformed[3].y - q->transformed[0].y;
      float bz = q->transformed[3].z - q->transformed[0].z;

      q->centre.x = 0;
      q->centre.y = 0;
      q->centre.z = 0;
      for (int i = 0; i < 4; i++) {
        q->centre.x += q->transformed[i].x;
        q->centre.y += q->transformed[i].y;
        q->centre.z += q->transformed[i].z;
      }
      q->centre.x /= 4;
      q->centre.y /= 4;
      q->centre.z /= 4;

      q->normal =
          vertex(ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx);

      float dotProduct = q->centre.x * q->normal.x + q->centre.y * q->normal.y +
                         q->centre.z * q->normal.z;

      q->dSquared = std::pow(q->centre.x, 2) + std::pow(q->centre.y, 2) +
                    std::pow(q->centre.z, 2);

      if (q->partials.size() == 0) {
        q->minProjectedX = w;
        q->minProjectedY = h;
        q->maxProjectedX = 0;
        q->maxProjectedY = 0;
        for (int i = 0; i < 4; i++) {
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
          for (int i = 0; i < 4; i++) {
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

  void updateCursor() {
    if (GetMouse(2).bHeld || GetKey(olc::Key::OEM_5).bHeld) return;

    float bestDsquared = drawDistance * drawDistance;

    for (auto& q : quads) {
      if (!q.visible || q.partials.size() > 0) continue;
      if (mousePos.x > q.minProjectedX && mousePos.x < q.maxProjectedX &&
          mousePos.y > q.minProjectedY && mousePos.y < q.maxProjectedY) {
        float dx[4];
        float dy[4];

        for (int i = 0; i < 4; i++) {
          dx[i] = q.projected[i].x - mousePos.x;
          dy[i] = q.projected[i].y - mousePos.y;
          float d = std::sqrt(std::pow(dx[i], 2) + std::pow(dy[i], 2));
          dx[i] /= d;
          dy[i] /= d;
        }

        float totalAngle = 0;
        for (int i = 0; i < 4; i++) {
          float dotProduct = dx[i] * dx[(i + 1) % 4] + dy[i] * dy[(i + 1) % 4];
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

  void sortEntities() {
    sortedEntities.clear();

    for (auto& entity : entities) {
      if (entity.outOfRange) continue;
      if (entity.dSquared > drawDistance * drawDistance) continue;

      float fade = 1 - std::sqrt(entity.dSquared) / drawDistance;
      if (fade < 0) continue;

      if (day) {
        int alpha = 255;
        if (fade < 0.25) {
          alpha = static_cast<int>(255.0f * 4 * fade);
        }
        int rgb = static_cast<int>(128 + 128 * std::pow(fade, 2));
        entity.colour = olc::Pixel(rgb, rgb, rgb, alpha);
      } else {
        int rgb = static_cast<int>(255.0f * std::pow(fade, 2));
        entity.colour = olc::Pixel(rgb, rgb, rgb);
      }

      sortedEntities.push_back(&entity);
    }

    std::sort(sortedEntities.begin(), sortedEntities.end(),
              [](entity* a, entity* b) { return a->dSquared > b->dSquared; });
  }

  void sortQuads() {
    sortedQuads.clear();

    for (int quadCounter = 0; quadCounter < quads.size(); quadCounter++) {
      quad* quadPointer = &quads[quadCounter];

      if (quadPointer->outOfRange) continue;
      if (quadPointer->dSquared > drawDistance * drawDistance) continue;
      if (quadPointer->cropped && cameraPitch != 0) continue;
      float fade = 1 - std::sqrt(quadPointer->dSquared) / drawDistance;
      if (fade < 0) continue;

      bool inSelection = false;
      bool inPreview = false;
      bool isPlayer = false;

      if (editMode) {
        int x1 =
            selectionStartX < selectionEndX ? selectionStartX : selectionEndX;
        int y1 =
            selectionStartY < selectionEndY ? selectionStartY : selectionEndY;
        int x2 =
            selectionStartX > selectionEndX ? selectionStartX : selectionEndX;
        int y2 =
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
        int mapX = playerX / unit + mazeWidth;
        int mapY = playerY / unit + mazeHeight;

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
          int alpha = 255;
          if (fade < 0.25) {
            alpha = static_cast<int>(255.0f * 4 * fade);
          }
          int rgb = static_cast<int>(128 + 128 * std::pow(fade, 2));
          quadPointer->colour = olc::Pixel(rgb, rgb, rgb, alpha);
        } else {
          int rgb = static_cast<int>(255.0f * std::pow(fade, 2));
          quadPointer->colour = olc::Pixel(rgb, rgb, rgb);
        }
      }

      sortedQuads.push_back(quadPointer);
    }

    std::sort(sortedQuads.begin(), sortedQuads.end(),
              [](quad* a, quad* b) { return a->dSquared > b->dSquared; });
  }

  void renderMazeMap() {
    const int size = 8;
    for (int i = 0; i < mazeWidth * 2 + 1; i++) {
      for (int j = 0; j < mazeHeight * 2 + 1; j++) {
        if (map[i][j].type & WALL_BIT) {
          if (!(map[i][j].type & HIGH_BIT)) {
            if (!(map[i][j].type & LOW_BIT)) {
              FillRect({i * size, j * size}, {size, size}, olc::YELLOW);
            } else {
              FillRect({i * size, j * size}, {size, size}, olc::MAGENTA);
            }
          } else {
            FillRect({i * size, j * size}, {size, size}, olc::WHITE);
          }
        } else {
          if (!(map[i][j].type & HIGH_BIT)) {
            FillRect({i * size, j * size}, {size, size}, olc::VERY_DARK_BLUE);
          } else if (!(map[i][j].type & LOW_BIT)) {
            FillRect({i * size, j * size}, {size, size}, olc::DARK_BLUE);
          } else {
            FillRect({i * size, j * size}, {size, size}, olc::BLUE);
          }
        }
      }
    }

    int mapX = cameraX / unit + mazeWidth;
    int mapY = cameraY / unit + mazeHeight;

    FillRect({mapX * size, mapY * size}, {size, size}, olc::RED);
    DrawLine({mapX * size + size / 2, mapY * size + size / 2},
             {mapX * size + size / 2 +
                  static_cast<int>(size * 2 * std::sin(cameraAngle)),
              mapY * size + size / 2 +
                  static_cast<int>(size * 2 * std::cos(cameraAngle))},
             olc::RED);
  }

  void renderQuads(bool renderEntities = false) {
    int qCount = 0;
    int eCount = 0;

    int e = 0;

    for (auto* q : sortedQuads) {
      while (e < sortedEntities.size() &&
             sortedEntities[e]->dSquared > q->dSquared) {
        auto E = sortedEntities[e];

        if (E->visible) {
          DrawDecal(E->projected, E->renderable->Decal(), E->scale, E->colour);
          eCount++;
        }

        e++;
      }

      if (q->renderable != nullptr) {
        float dotProduct = q->centre.x * q->normal.x +
                           q->centre.y * q->normal.y +
                           q->centre.z * q->normal.z;

        auto decal = q->renderable->Decal();

        if (dotProduct > 0) {
          if (q->partials.size() > 0) {
            for (auto& p : q->partials) {
              DrawPartialWarpedDecal(decal, p.projected, p.sourcePos,
                                     p.sourceSize, p.colour);
              qCount++;
            }
          }
          if (q->visible) {
            DrawPartialWarpedDecal(decal, q->projected, q->sourcePos,
                                   q->sourceSize, q->colour);
            qCount++;
          }
        }
      }
    }

    if (qCount > qMax) qMax = qCount;

    if (editMode) {
      std::ostringstream stringStream;

      stringStream << "./maps/level" << levelNo << ".dat" << std::endl;
      stringStream << std::endl;

      stringStream << "Quads: " << qCount << " (Max: " << qMax << ")"
                   << std::endl
                   << "Entities: " << eCount << " / " << entities.size()
                   << std::endl
                   << "Zoom: " << static_cast<int>(200 * zoom / w)
                   << "% | Range: " << drawDistance << " | " << GetFPS()
                   << " FPS" << std::endl
                   << "X: " << cameraX << " Y: " << cameraY << " Z: " << cameraZ
                   << std::endl;

      if (quad::cursorQuad != nullptr) {
        stringStream << "Cursor " << cursorX << ", " << cursorY << ", "
                     << "Wall: " << (quad::cursorQuad->wall ? "True" : "False")
                     << ", Level: " << quad::cursorQuad->level
                     << ", Direction: " << quad::cursorQuad->direction
                     << (autoTexture ? " [Auto]" : "") << std::endl;
      }

      if (clipboardWidth > -1 && clipboardHeight > -1) {
        stringStream << "Clipboard: " << clipboardWidth << " x "
                     << clipboardHeight << " [" << cursorRotation << "]"
                     << std::endl;
      }

      stringStream << std::endl;
      stringStream << "1 2 3 4 5 6 7 8 9 0 - =" << std::endl;
      stringStream << "        _ _ _ _ _ _ _ _" << std::endl;
      stringStream << "    _ _ # # # #     _ _" << std::endl;
      stringStream << "    # #   _ # #   _ # #" << std::endl;
      stringStream << "_ # _ # _ # _ # _ # _ #" << std::endl << std::endl;

      for (int i = 0; i < 12; i++) {
        if (i == selectedBlock) {
          stringStream << "^ ";
        } else {
          stringStream << "  ";
        }
      }

      DrawStringDecal({10, 10}, stringStream.str(), olc::WHITE);
    }
  }

  void renderTileSelector() {
    int iMax = static_cast<int>(w / (TILE_SIZE + 10));
    int jMax = static_cast<int>(176 / iMax) + 1;
    for (int j = 0; j < jMax; j++) {
      for (int i = 0; i < iMax; i++) {
        int k = j * iMax + i;
        if (k >= 176) continue;
        DrawDecal({static_cast<float>(i * (TILE_SIZE + 10) + 10),
                   static_cast<float>(j * (TILE_SIZE + 10) + 10)},
                  texture[k].Decal(), {1, 1},
                  k == selectedTexture ? olc::WHITE : olc::GREY);
      }
    }
  }

  void renderHelp() {
    std::ostringstream stringStream;
    stringStream << "W/S/A/D - Move" << std::endl
                 << "Left Click - Apply Texture (One surface)" << std::endl
                 << "Right Click (Drag) or Left/Right - Turn" << std::endl
                 << "Middle Click (Hold) or \\ - Choose Texture" << std::endl
                 << "T - Apply Texture (walls)" << std::endl
                 << "F - Apply Texture (floor)" << std::endl
                 << "C - Apply Texture (ceiling)" << std::endl
                 << "Q - Pick Texture" << std::endl
                 << "Ctrl + 1,2,3...-,= or Mouse Wheel - Pick Block Type"
                 << std::endl
                 << "Space - Apply block type" << std::endl
                 << "E - Pick block type" << std::endl
                 << "Shift - Select region" << std::endl
                 << "Escape - Cancel selection" << std::endl
                 << "Ctrl + C - Copy column on selection" << std::endl
                 << "V - Preview paste location" << std::endl
                 << "Ctrl + V - Paste column on selection" << std::endl
                 << "R / Ctrl + R - Change paste transformation" << std::endl
                 << "Ctrl + Z - Undo texture or block change" << std::endl
                 << "Ctrl + E - Generate random entities" << std::endl
                 << "-/+ - Zoom" << std::endl
                 << ",/. - Render Distance" << std::endl
                 << "PgUp/PgDn - Fly Up / Down" << std::endl
                 << "End - Reset Height" << std::endl
                 << "Up/Down - Look up / down (Experimental)" << std::endl
                 << "Home - Reset Pitch" << std::endl
                 << "Ctrl + Home - Reset View Zoom & Render Distance"
                 << std::endl
                 << "Tab - Show map" << std::endl
                 << "Ctrl + G - Generate new maze" << std::endl
                 << "Ctrl + M - Generate empty map" << std::endl
                 << "N - Switch between night and day" << std::endl
                 << "Ctrl + N - No-clip mode" << std::endl
                 << "Ctrl + S - Save level" << std::endl
                 << "Ctrl + L - Load level" << std::endl
                 << "Ctrl + 1,2,3...0 - Pick save slot, 1-10" << std::endl
                 << "H - Toggle Help (this text!)" << std::endl;

    DrawStringDecal({2 * w / 3, 10}, stringStream.str(), olc::WHITE);
  }

  bool OnUserUpdate(float fElapsedTime) override {
    frame++;

    for (int s = 0; s < 5; s++) {
      for (int i = 0; i < SPRITE_SIZE; i++) {
        for (int j = 0; j < SPRITE_SIZE; j++) {
          sprite[s].Sprite()->SetPixel(
              i, j,
              olc::Pixel(
                  std::rand() % 255, std::rand() % 255, std::rand() % 255,
                  (i == SPRITE_SIZE / 2 && j == SPRITE_SIZE / 2)
                      ? 255
                      : static_cast<int>(255.0f * 10 /
                                         (std::abs(i - SPRITE_SIZE / 2) *
                                          std::abs(j - SPRITE_SIZE / 2)))));
        }
      }
      sprite[s].Decal()->Update();
    }

    if (editMode) {
      handleEditInputs(fElapsedTime);
    } else {
      handlePlayInputs(fElapsedTime);
    }

    if (!editMode) playProcessing(fElapsedTime);

    updateMatrix();

    if (!editMode && !noClip) handlePlayerInteractions();

    updateQuads();
    updateEntities();
    if (editMode) updateCursor();

    if (GetKey(olc::Key::TAB).bHeld) {
      renderMazeMap();
    } else {
      sortQuads();
      sortEntities();
      renderQuads(true);
      if (editMode) {
        if (GetMouse(2).bHeld || GetKey(olc::Key::OEM_5).bHeld) {
          renderTileSelector();
        } else {
          DrawDecal({w / 2 - TILE_SIZE / 2, 10},
                    texture[selectedTexture].Decal(), {1, 1});
        }
        if (showHelp) renderHelp();
      }
    }

    return !exitSignal;
  }
};

int main() {
  Olc3d2 demo;
  if (demo.Construct(w, h, 1, 1)) demo.Start();
  return 0;
}
