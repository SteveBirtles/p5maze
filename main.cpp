#define OLC_PGE_APPLICATION
#include <filesystem>

#include "olcPixelGameEngine.h"

const int w = 1280;
const int h = 1024;
const int tileSize = 64;
const float omega = 0.1;
float zoom = w / 2;
bool day = true;
bool showHelp = true;
bool autoTexture = false;
int levelNo = 1;
bool exitSignal = false;

const int defaultFloor = 97;
const int defaultWall = 154;
const int defaultCeiling = 102;

struct Renderable {
  Renderable() {}

  void Load(const std::string& sFile) {
    sprite = new olc::Sprite(sFile);
    decal = new olc::Decal(sprite);
  }

  ~Renderable() {
    delete decal;
    delete sprite;
  }

  olc::Sprite* sprite = nullptr;
  olc::Decal* decal = nullptr;
};

Renderable texture[176];

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
  olc::vi2d sourcePos;
  olc::vi2d sourceSize;
  std::array<olc::vf2d, 4> projected;
  std::array<vertex, 4> adjusted;
  olc::Pixel colour;
  partial(std::array<vertex, 4> _adjusted, std::array<int, 4> _texture,
          olc::Pixel _colour = olc::WHITE) {
    sourcePos.x = _texture[0];
    sourcePos.y = _texture[1];
    sourceSize.x = _texture[2];
    sourceSize.y = _texture[3];
    adjusted = _adjusted;
    colour = _colour;
  }
};

struct quad {
  static quad* cursorQuad;
  Renderable* renderable;
  std::array<vertex, 4> vertices;
  std::array<vertex, 4> adjusted;
  olc::vi2d sourcePos;
  olc::vi2d sourceSize;
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
  int wallF;
  int wallD;
  quad() {
    mapX = -1;
    mapY = -1;
  };
  quad(vertex _p0, vertex _p1, vertex _p2, vertex _p3, bool _wall, int _mapX,
       int _mapY, int textureNo = 7, int _wallF = 0, int _wallD = -1) {
    vertices[0] = _p0;
    vertices[1] = _p1;
    vertices[2] = _p2;
    vertices[3] = _p3;
    sourcePos.x = 0;
    sourcePos.y = 0;
    sourceSize.x = tileSize;
    sourceSize.y = tileSize;
    mapX = _mapX;
    mapY = _mapY;
    wall = _wall;
    wallF = _wallF;
    wallD = _wallD;
    renderable = &texture[textureNo];
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

enum class cellType { wall, corridor, lowroom, highroom, sky };

struct mapCell {
  cellType type;
  int floor;
  int ceiling;
  int wall[3][4];
  mapCell() { type = cellType::sky; }
  mapCell(cellType _type, int _floor, int _wall[3][4], int _ceiling) {
    type = _type;
    floor = _floor;
    for (int f = 0; f < 3; f++) {
      for (int d = 0; d < 4; d++) {
        wall[f][d] = _wall[f][d];
      }
    }
    ceiling = _ceiling;
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
float drawDistance = 15 * unit;

mapCell map[MAX_WIDTH * 2 + 1][MAX_HEIGHT * 2 + 1];
kruskalCell kruskalMaze[MAX_WIDTH][MAX_HEIGHT];

void clearMap() {
  for (int i = -mazeWidth; i <= mazeWidth; i++) {
    for (int j = -mazeHeight; j <= mazeHeight; j++) {
      if (i == -mazeWidth || j == -mazeWidth || i == mazeWidth ||
          j == mazeWidth) {
        map[i + mazeWidth][j + mazeHeight].type = cellType::wall;
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i + mazeWidth][j + mazeHeight].wall[f][d] = defaultWall;
          }
        }
        map[i + mazeWidth][j + mazeHeight].floor = defaultFloor;
        map[i + mazeWidth][j + mazeHeight].ceiling = defaultCeiling;
      } else {
        map[i + mazeWidth][j + mazeHeight].type = cellType::sky;
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i + mazeWidth][j + mazeHeight].wall[f][d] = defaultWall;
          }
        }
        map[i + mazeWidth][j + mazeHeight].floor = defaultFloor;
        map[i + mazeWidth][j + mazeHeight].ceiling = defaultCeiling;
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

int setQuadTexture(int x, int y, int id, bool wall, bool ceiling = false) {
  if (x < 0 || y < 0 || x > 2 * mazeWidth || y > 2 * mazeWidth) return 0;
  int setCount = 0;
  for (int i = 0; i < quads.size(); i++) {
    if (quads[i].mapX == x && quads[i].mapY == y) {
      if (wall && quads[i].wall ||
          !wall && !quads[i].wall &&
              (!ceiling && quads[i].vertices[0].z > unit / 2 ||
               ceiling && quads[i].vertices[0].z < unit / 2)) {
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
  } else if (ceiling) {
    map[x][y].ceiling = id;
  } else if (ceiling) {
    map[x][y].floor = id;
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

      if (map[i + mazeWidth][j + mazeHeight].type == cellType::corridor ||
          map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom ||
          map[i + mazeWidth][j + mazeHeight].type == cellType::highroom ||
          map[i + mazeWidth][j + mazeHeight].type == cellType::sky) {
        quads.push_back(quad(vertex(x1, y1, unit, -1), vertex(x2, y2, unit, -1),
                             vertex(x3, y3, unit, -1), vertex(x4, y4, unit, -1),
                             false, i + mazeWidth, j + mazeHeight,
                             map[i + mazeWidth][j + mazeHeight].floor, 0));

        if (map[i + mazeWidth][j + mazeHeight].type == cellType::corridor) {
          quads.push_back(quad(vertex(x4, y4, 0, -1), vertex(x3, y3, 0, -1),
                               vertex(x2, y2, 0, -1), vertex(x1, y1, 0, -1),
                               false, i + mazeWidth, j + mazeHeight,
                               map[i + mazeWidth][j + mazeHeight].ceiling, 1));
        }

        if (map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom) {
          quads.push_back(
              quad(vertex(x4, y4, -unit, -1), vertex(x3, y3, -unit, -1),
                   vertex(x2, y2, -unit, -1), vertex(x1, y1, -unit, -1), false,
                   i + mazeWidth, j + mazeHeight,
                   map[i + mazeWidth][j + mazeHeight].ceiling, 2));
        }

        if (map[i + mazeWidth][j + mazeHeight].type == cellType::highroom) {
          quads.push_back(
              quad(vertex(x4, y4, -unit * 2, -1), vertex(x3, y3, -unit * 2, -1),
                   vertex(x2, y2, -unit * 2, -1), vertex(x1, y1, -unit * 2, -1),
                   false, i + mazeWidth, j + mazeHeight,
                   map[i + mazeWidth][j + mazeHeight].ceiling, 3));
        }

      } else if (map[i + mazeWidth][j + mazeHeight].type == cellType::wall) {
        if (j > -mazeHeight &&
            map[i + mazeWidth][j + mazeHeight - 1].type != cellType::wall) {
          quads.push_back(quad(vertex(x2, y2, unit, 3), vertex(x2, y2, 0.0f, 2),
                               vertex(x1, y1, 0.0f, 1), vertex(x1, y1, unit, 0),
                               true, i + mazeWidth, j + mazeHeight,
                               map[i + mazeWidth][j + mazeHeight].wall[0][0], 0,
                               0));
        }

        if (i < mazeWidth - 1 &&
            map[i + mazeWidth + 1][j + mazeHeight].type != cellType::wall) {
          quads.push_back(quad(vertex(x3, y3, unit, 3), vertex(x3, y3, 0.0f, 2),
                               vertex(x2, y2, 0.0f, 1), vertex(x2, y2, unit, 0),
                               true, i + mazeWidth, j + mazeHeight,
                               map[i + mazeWidth][j + mazeHeight].wall[0][1], 0,
                               1));
        }

        if (j < mazeHeight - 1 &&
            map[i + mazeWidth][j + mazeHeight + 1].type != cellType::wall) {
          quads.push_back(quad(vertex(x4, y4, unit, 3), vertex(x4, y4, 0.0f, 2),
                               vertex(x3, y3, 0.0f, 1), vertex(x3, y3, unit, 0),
                               true, i + mazeWidth, j + mazeHeight,
                               map[i + mazeWidth][j + mazeHeight].wall[0][2], 0,
                               2));
        }

        if (i > -mazeHeight &&
            map[i + mazeWidth - 1][j + mazeHeight].type != cellType::wall) {
          quads.push_back(quad(vertex(x1, y1, unit, 3), vertex(x1, y1, 0.0f, 2),
                               vertex(x4, y4, 0.0f, 1), vertex(x4, y4, unit, 0),
                               true, i + mazeWidth, j + mazeHeight,
                               map[i + mazeWidth][j + mazeHeight].wall[0][3], 0,
                               3));
        }
      }

      for (int k = -1; k <= 0; k++) {
        if (map[i + mazeWidth][j + mazeHeight].type != cellType::sky &&
            j > -mazeHeight &&
            ((map[i + mazeWidth][j + mazeHeight - 1].type ==
                      cellType::lowroom &&
                  k == 0 ||
              map[i + mazeWidth][j + mazeHeight - 1].type ==
                  cellType::highroom) &&
                 map[i + mazeWidth][j + mazeHeight - 1].type !=
                     map[i + mazeWidth][j + mazeHeight].type &&
                 !((map[i + mazeWidth][j + mazeHeight - 1].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth][j + mazeHeight - 1].type ==
                        cellType::highroom ||
                    map[i + mazeWidth][j + mazeHeight - 1].type ==
                        cellType::sky) &&
                   (map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::highroom) &&
                   k == 0) ||
             map[i + mazeWidth][j + mazeHeight - 1].type == cellType::sky &&
                 map[i + mazeWidth][j + mazeHeight].type !=
                     cellType::highroom &&
                 !(k == 0 &&
                   map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom)

                 )) {
          quads.push_back(quad(
              vertex(x2, y2, k * unit, 3), vertex(x2, y2, k * unit - unit, 2),
              vertex(x1, y1, k * unit - unit, 1), vertex(x1, y1, k * unit, 0),
              true, i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[1 - k][0], 1 - k, 0));
        }

        if (map[i + mazeWidth][j + mazeHeight].type != cellType::sky &&
            i < mazeWidth - 1 &&
            ((map[i + mazeWidth + 1][j + mazeHeight].type ==
                      cellType::lowroom &&
                  k == 0 ||
              map[i + mazeWidth + 1][j + mazeHeight].type ==
                  cellType::highroom) &&
                 map[i + mazeWidth + 1][j + mazeHeight].type !=
                     map[i + mazeWidth][j + mazeHeight].type &&
                 !((map[i + mazeWidth + 1][j + mazeHeight].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth + 1][j + mazeHeight].type ==
                        cellType::highroom ||
                    map[i + mazeWidth + 1][j + mazeHeight].type ==
                        cellType::sky) &&
                   (map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::highroom) &&
                   k == 0) ||
             map[i + mazeWidth + 1][j + mazeHeight].type == cellType::sky &&
                 map[i + mazeWidth][j + mazeHeight].type !=
                     cellType::highroom &&
                 !(k == 0 &&
                   map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom)

                 )) {
          quads.push_back(quad(
              vertex(x3, y3, k * unit, 3), vertex(x3, y3, k * unit - unit, 2),
              vertex(x2, y2, k * unit - unit, 1), vertex(x2, y2, k * unit, 0),
              true, i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[1 - k][1], 1 - k, 1));
        }

        if (map[i + mazeWidth][j + mazeHeight].type != cellType::sky &&
            j < mazeHeight - 1 &&
            ((map[i + mazeWidth][j + mazeHeight + 1].type ==
                      cellType::lowroom &&
                  k == 0 ||
              map[i + mazeWidth][j + mazeHeight + 1].type ==
                  cellType::highroom) &&
                 map[i + mazeWidth][j + mazeHeight + 1].type !=
                     map[i + mazeWidth][j + mazeHeight].type &&
                 !((map[i + mazeWidth][j + mazeHeight + 1].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth][j + mazeHeight + 1].type ==
                        cellType::highroom ||
                    map[i + mazeWidth][j + mazeHeight + 1].type ==
                        cellType::sky) &&
                   (map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::highroom) &&
                   k == 0) ||
             map[i + mazeWidth][j + mazeHeight + 1].type == cellType::sky &&
                 map[i + mazeWidth][j + mazeHeight].type !=
                     cellType::highroom &&
                 !(k == 0 &&
                   map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom)

                 )) {
          quads.push_back(quad(
              vertex(x4, y4, k * unit, 3), vertex(x4, y4, k * unit - unit, 2),
              vertex(x3, y3, k * unit - unit, 1), vertex(x3, y3, k * unit, 0),
              true, i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[1 - k][2], 1 - k, 2));
        }

        if (map[i + mazeWidth][j + mazeHeight].type != cellType::sky &&
            i > -mazeHeight &&
            ((map[i + mazeWidth - 1][j + mazeHeight].type ==
                      cellType::lowroom &&
                  k == 0 ||
              map[i + mazeWidth - 1][j + mazeHeight].type ==
                  cellType::highroom) &&
                 map[i + mazeWidth - 1][j + mazeHeight].type !=
                     map[i + mazeWidth][j + mazeHeight].type &&
                 !((map[i + mazeWidth - 1][j + mazeHeight].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth - 1][j + mazeHeight].type ==
                        cellType::highroom ||
                    map[i + mazeWidth - 1][j + mazeHeight].type ==
                        cellType::sky) &&
                   (map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::lowroom ||
                    map[i + mazeWidth][j + mazeHeight].type ==
                        cellType::highroom) &&
                   k == 0) ||
             map[i + mazeWidth - 1][j + mazeHeight].type == cellType::sky &&
                 map[i + mazeWidth][j + mazeHeight].type !=
                     cellType::highroom &&
                 !(k == 0 &&
                   map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom)

                 )) {
          quads.push_back(quad(
              vertex(x1, y1, k * unit, 3), vertex(x1, y1, k * unit - unit, 2),
              vertex(x4, y4, k * unit - unit, 1), vertex(x4, y4, k * unit, 0),
              true, i + mazeWidth, j + mazeHeight,
              map[i + mazeWidth][j + mazeHeight].wall[1 - k][3], 1 - k, 3));
        }
      }
    }
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
              ? cellType::corridor
              : cellType::wall;
      map[i + mazeWidth][j + mazeHeight].floor = defaultFloor;

      for (int f = 0; f < 3; f++) {
        for (int d = 0; d < 4; d++) {
          map[i + mazeWidth][j + mazeHeight].wall[f][d] = defaultWall;
        }
      }
      map[i + mazeWidth][j + mazeHeight].ceiling = defaultCeiling;
    }
  }

  for (int i = 0; i < mazeWidth; i++) {
    for (int j = 0; j < mazeHeight; j++) {
      if (kruskalMaze[i][j].right) {
        map[i * 2 + 2][j * 2 + 1].type = cellType::corridor;
        map[i * 2 + 2][j * 2 + 1].floor = defaultFloor;

        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i * 2 + 2][j * 2 + 1].wall[f][d] = defaultWall;
          }
        }
        map[i * 2 + 2][j * 2 + 1].ceiling = defaultCeiling;
      }
      if (kruskalMaze[i][j].down) {
        map[i * 2 + 1][j * 2 + 2].type = cellType::corridor;
        map[i * 2 + 1][j * 2 + 2].floor = defaultFloor;

        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i * 2 + 1][j * 2 + 2].wall[f][d] = defaultWall;
          }
        }
        map[i * 2 + 1][j * 2 + 2].ceiling = defaultCeiling;
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
        map[i][j].type = cellType::lowroom;
        map[i][j].floor = defaultFloor;

        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i][j].wall[f][d] = defaultWall;
          }
        }
        map[i][j].ceiling = defaultCeiling;
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
        map[i][j].type = cellType::highroom;
        map[i][j].floor = defaultFloor;
        for (int f = 0; f < 3; f++) {
          for (int d = 0; d < 4; d++) {
            map[i][j].wall[f][d] = defaultWall;
          }
        }
        map[i][j].ceiling = defaultCeiling;
      }
    }
  }
}

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
      output.write((char*)&map[i + mazeWidth][j + mazeHeight], sizeof(mapCell));
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

class Olc3d2 : public olc::PixelGameEngine {
 public:
  Olc3d2() { sAppName = "Olc3d2"; }

 public:
  bool OnUserCreate() override {
    for (int i = 0; i < 176; i++) {
      std::ostringstream filename;
      filename << "./tiles/tile" << i << ".png";
      texture[i].Load(filename.str());
    }

    std::srand(std::time(nullptr));
    loadLevel();

    return true;
  }

  float angle = 0;
  float pitch = 0;
  float myX = unit / 2;
  float myY = unit / 2;
  float myZ = unit / 2;
  float lastMyX, lastMyY, lastAngle;
  olc::vi2d mousePos;
  olc::vi2d lastMousePos;
  int cursorX, cursorY;

  int selectedTexture = 1;

  int qMax = 0;

  float m[3][3];

  void updateMatrix() {
    float m1[3][3] = {{1, 0, 0},
                      {0, std::cos(pitch), std::sin(pitch)},
                      {0, -std::sin(pitch), std::cos(pitch)}};

    float m2[3][3] = {{std::cos(angle), -std::sin(angle), 0},
                      {std::sin(angle), std::cos(angle), 0},
                      {0, 0, 1}};

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        m[i][j] =
            m1[i][0] * m2[0][j] + m1[i][1] * m2[1][j] + m1[i][2] * m2[2][j];
      }
    }
  }

  void handleInputs(float frameLength) {
    mousePos = GetMousePos();

    if (GetMouse(2).bHeld || GetKey(olc::Key::SHIFT).bHeld) {
      int iMax = static_cast<int>(w / (tileSize + 10));
      int jMax = static_cast<int>(176 / iMax) + 1;

      selectedTexture =
          static_cast<int>(mousePos.y / (tileSize + 10.0f)) * iMax +
          static_cast<int>(mousePos.x < (iMax - 1) * (tileSize + 10.0f)
                               ? mousePos.x / (tileSize + 10.0f)
                               : (iMax - 1));

      if (selectedTexture < 0) selectedTexture = 0;
      if (selectedTexture > 175) selectedTexture = 175;

    } else {
      int mouseWheel = GetMouseWheel();

      if (mouseWheel < 0) selectedTexture = (selectedTexture + 1) % 176;
      if (mouseWheel > 0) selectedTexture = (selectedTexture + 175) % 176;

      if (GetKey(olc::Key::LEFT).bHeld) angle += 2 * frameLength;
      if (GetKey(olc::Key::RIGHT).bHeld) angle -= 2 * frameLength;

      if (cursorX >= 0 && cursorY >= 0 && cursorX <= mazeWidth * 2 &&
          cursorY <= mazeHeight * 2) {
        if (GetMouse(0).bHeld && quad::cursorQuad != nullptr) {
          int x = quad::cursorQuad->mapX;
          int y = quad::cursorQuad->mapY;
          int f = quad::cursorQuad->wallF;
          int d = quad::cursorQuad->wallD;

          if (quad::cursorQuad->wall) {
            map[x][y].wall[f][d] = selectedTexture;
          } else {
            if (f == 0) {
              map[x][y].floor = selectedTexture;
            } else {
              map[x][y].ceiling = selectedTexture;
            }
          }

          quad::cursorQuad->renderable = &texture[selectedTexture];

        } else if (GetMouse(1).bHeld && quad::cursorQuad != nullptr) {
          angle += static_cast<float>(mousePos.x - lastMousePos.x) / 600 *
                   ((w / 2) / zoom);

        } else if (GetKey(olc::Key::Q).bPressed) {
          int x = quad::cursorQuad->mapX;
          int y = quad::cursorQuad->mapY;
          int f = quad::cursorQuad->wallF;
          int d = quad::cursorQuad->wallD;

          if (quad::cursorQuad->wall) {
            selectedTexture = map[x][y].wall[f][d];
          } else if (f == 0) {
            selectedTexture = map[x][y].floor;
          } else {
            selectedTexture = map[x][y].ceiling;
          }

        } else if (GetKey(olc::Key::T).bHeld && !GetKey(olc::Key::CTRL).bHeld) {
          setQuadTexture(cursorX, cursorY, selectedTexture, true);
          setQuadTexture(cursorX, cursorY, selectedTexture, false, true);
        }
      }

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

      if (GetKey(olc::Key::T).bPressed && GetKey(olc::Key::CTRL).bHeld) {
        autoTexture = !autoTexture;
      }

      if (GetKey(olc::Key::Q).bPressed && GetKey(olc::Key::CTRL).bHeld) {
        exitSignal = true;
      }

      if (GetKey(olc::Key::S).bPressed && GetKey(olc::Key::CTRL).bHeld) {
        saveLevel();
      }

      if (GetKey(olc::Key::L).bPressed && GetKey(olc::Key::CTRL).bHeld) {
        loadLevel();
      }

      if (GetKey(olc::Key::OEM_4).bHeld) drawDistance /= 1.01;
      if (GetKey(olc::Key::OEM_6).bHeld) drawDistance *= 1.01;

      if (drawDistance < unit) drawDistance = unit;
      if (drawDistance > unit * mazeWidth * 2)
        drawDistance = unit * mazeWidth * 2;

      if (GetKey(olc::Key::CTRL).bHeld) {
        if (GetKey(olc::Key::PGDN).bHeld) pitch += 2 * frameLength;
        if (GetKey(olc::Key::PGUP).bHeld) pitch -= 2 * frameLength;

      } else {
        if (GetKey(olc::Key::PGUP).bHeld) myZ -= unit * 2 * frameLength;
        if (GetKey(olc::Key::PGDN).bHeld) myZ += unit * 2 * frameLength;
        if (GetKey(olc::Key::END).bHeld) myZ = unit / 2;
      }

      if (quad::cursorQuad != nullptr) {
        int x = quad::cursorQuad->mapX;
        int y = quad::cursorQuad->mapY;

        if (GetKey(olc::Key::F).bPressed) {
          switch (map[x][y].type) {
            case cellType::sky:
              map[x][y].type = cellType::highroom;
              regenerateQuads();
              break;
            case cellType::highroom:
              map[x][y].type = cellType::lowroom;
              regenerateQuads();
              break;
            case cellType::lowroom:
              map[x][y].type = cellType::corridor;
              regenerateQuads();
              break;
            case cellType::corridor:
              map[x][y].type = cellType::wall;
              regenerateQuads();
              break;
          }
          if (autoTexture) {
            setQuadTexture(x, y, selectedTexture, true);
            setQuadTexture(x, y, selectedTexture, false, true);
          }
          quad::cursorQuad = nullptr;
        }

        if (GetKey(olc::Key::R).bPressed) {
          switch (map[x][y].type) {
            case cellType::highroom:
              map[x][y].type = cellType::sky;
              regenerateQuads();
              break;
            case cellType::lowroom:
              map[x][y].type = cellType::highroom;
              regenerateQuads();
              break;
            case cellType::corridor:
              map[x][y].type = cellType::lowroom;
              regenerateQuads();
              break;
            case cellType::wall:
              map[x][y].type = cellType::corridor;
              regenerateQuads();
              break;
          }
          if (autoTexture) {
            setQuadTexture(x, y, selectedTexture, true);
            setQuadTexture(x, y, selectedTexture, false, true);
          }
          quad::cursorQuad = nullptr;
        }
      }

      if (GetKey(olc::Key::HOME).bHeld) {
        pitch = 0;
        zoom = w / 2;
        drawDistance = 1500;
      }

      if (GetKey(olc::Key::EQUALS).bHeld) zoom *= 1.05;
      if (GetKey(olc::Key::MINUS).bHeld) zoom /= 1.05;
      if (zoom < w / 4) zoom = w / 4;
      if (zoom > w * 5) zoom = w * 5;

      if (GetKey(olc::Key::N).bPressed) {
        day = !day;
      }

      if (GetKey(olc::Key::H).bPressed) {
        showHelp = !showHelp;
      }

      if (GetKey(olc::Key::M).bPressed && GetKey(olc::Key::CTRL).bHeld) {
        day = true;
        clearMap();
        regenerateQuads();
      }

      if (GetKey(olc::Key::G).bPressed && GetKey(olc::Key::CTRL).bHeld) {
        day = false;
        makeMaze();
        regenerateQuads();
      }

      if (pitch < -1.571) pitch = -1.571;
      if (pitch > 1.571) pitch = 1.571;

      lastMyX = myX;
      lastMyY = myY;
      lastAngle = angle;

      if (!GetKey(olc::Key::CTRL).bHeld) {
        if (GetKey(olc::Key::W).bHeld) {
          myX += std::sin(angle) * unit * 2 * frameLength;
          myY += std::cos(angle) * unit * 2 * frameLength;
        }
        if (GetKey(olc::Key::S).bHeld) {
          myX -= std::sin(angle) * unit * 2 * frameLength;
          myY -= std::cos(angle) * unit * 2 * frameLength;
        }
        if (GetKey(olc::Key::A).bHeld) {
          myX += std::cos(angle) * unit * 2 * frameLength;
          myY += -std::sin(angle) * unit * 2 * frameLength;
        }
        if (GetKey(olc::Key::D).bHeld) {
          myX -= std::cos(angle) * unit * 2 * frameLength;
          myY -= -std::sin(angle) * unit * 2 * frameLength;
        }
      }
    }
  }

  void handleInteractions() {
    int mapX = myX / unit + mazeWidth;
    int mapY = myY / unit + mazeHeight;

    float dx = myX - lastMyX;
    float dy = myY - lastMyY;

    if (mapX > 0 && mapY > 0 && mapX < mazeWidth * 2 + 1 &&
        mapY < mazeWidth * 2 + 1) {
      bool north = false, south = false, east = false, west = false;
      bool northEast = false, southEast = false, northWest = false,
           southWest = false;

      double intBit;
      float eastWest = std::modf(myX / unit + mazeWidth - mapX, &intBit);
      float northSouth = std::modf(myY / unit + mazeHeight - mapY, &intBit);

      east = map[mapX - 1][mapY].type == cellType::wall;
      west = map[mapX + 1][mapY].type == cellType::wall;
      north = map[mapX][mapY - 1].type == cellType::wall;
      south = map[mapX][mapY + 1].type == cellType::wall;

      northEast = map[mapX - 1][mapY - 1].type == cellType::wall;
      southEast = map[mapX - 1][mapY + 1].type == cellType::wall;
      northWest = map[mapX + 1][mapY - 1].type == cellType::wall;
      southWest = map[mapX + 1][mapY + 1].type == cellType::wall;

      if (!north && !east && northSouth < 0.25 && eastWest < 0.25 &&
          northEast) {
        if (dx < 0 && dy < 0) {
          if (-dx > -dy)
            myX = (mapX + 0.25 - mazeWidth) * unit;
          else
            myY = (mapY + 0.25 - mazeHeight) * unit;
        } else {
          if (dx < 0) myX = (mapX + 0.25 - mazeWidth) * unit;
          if (dy < 0) myY = (mapY + 0.25 - mazeHeight) * unit;
        }
      }

      if (!north && !west && northSouth < 0.25 && eastWest > 0.75 &&
          northWest) {
        if (dx > 0 && dy < 0) {
          if (dx > -dy)
            myX = (mapX + 0.75 - mazeWidth) * unit;
          else
            myY = (mapY + 0.25 - mazeHeight) * unit;
        } else {
          if (dx > 0) myX = (mapX + 0.75 - mazeWidth) * unit;
          if (dy < 0) myY = (mapY + 0.25 - mazeHeight) * unit;
        }
      }

      if (!south && !east && northSouth > 0.75 && eastWest < 0.25 &&
          southEast) {
        if (dx < 0 && dy > 0) {
          if (-dx < dy)
            myX = (mapX + 0.25 - mazeWidth) * unit;
          else
            myY = (mapY + 0.75 - mazeHeight) * unit;
        } else {
          if (dx < 0) myX = (mapX + 0.25 - mazeWidth) * unit;
          if (dy > 0) myY = (mapY + 0.75 - mazeHeight) * unit;
        }
      }

      if (!south && !west && northSouth > 0.75 && eastWest > 0.75 &&
          southWest) {
        if (dx > 0 && dy > 0) {
          if (dx > dy)
            myX = (mapX + 0.75 - mazeWidth) * unit;
          else
            myY = (mapY + 0.75 - mazeHeight) * unit;
        } else {
          if (dx > 0) myX = (mapX + 0.75 - mazeWidth) * unit;
          if (dy > 0) myY = (mapY + 0.75 - mazeHeight) * unit;
        }
      }

      eastWest = std::modf(myX / unit + mazeWidth - mapX, &intBit);
      northSouth = std::modf(myY / unit + mazeHeight - mapY, &intBit);

      if (northSouth < 0.25 && north && dy < 0)
        myY = (mapY + 0.25 - mazeHeight) * unit;
      if (northSouth > 0.75 && south && dy > 0)
        myY = (mapY + 0.75 - mazeHeight) * unit;
      if (eastWest < 0.25 && east && dx < 0)
        myX = (mapX + 0.25 - mazeWidth) * unit;
      if (eastWest > 0.75 && west && dx > 0)
        myX = (mapX + 0.75 - mazeWidth) * unit;
    }
  }

  void updateQuads() {
    for (auto& q : quads) {
      q.outOfRange = std::abs(q.vertices[0].x - myX) > drawDistance ||
                     std::abs(q.vertices[0].y - myY) > drawDistance;

      if (q.outOfRange) continue;

      float maxX = 0;
      float maxY = 0;
      float maxZ = 0;

      for (int i = 0; i < 4; i++) {
        q.adjusted[i].x = (q.vertices[i].x - myX) * m[0][0] +
                          (q.vertices[i].y - myY) * m[0][1] +
                          (q.vertices[i].z - myZ) * m[0][2];
        if (std::abs(q.adjusted[i].x) > maxX) maxX = std::abs(q.adjusted[i].x);
        q.adjusted[i].y = (q.vertices[i].x - myX) * m[1][0] +
                          (q.vertices[i].y - myY) * m[1][1] +
                          (q.vertices[i].z - myZ) * m[1][2];
        if (std::abs(q.adjusted[i].y) > maxY) maxY = std::abs(q.adjusted[i].y);
        q.adjusted[i].z = (q.vertices[i].x - myX) * m[2][0] +
                          (q.vertices[i].y - myY) * m[2][1] +
                          (q.vertices[i].z - myZ) * m[2][2];
        if (std::abs(q.adjusted[i].z) > maxZ) maxZ = std::abs(q.adjusted[i].z);
        q.visible = true;
        q.cropped = false;

        q.partials.clear();
      }

      if (maxX > drawDistance || maxY > drawDistance || maxZ > drawDistance) {
        q.visible = false;
        continue;
      }

      if (!q.wall) {
        int lostCornerCount = 0;

        for (int j = 0; j < 4; j++) {
          if (q.adjusted[j].y < omega) lostCornerCount++;
        }

        if (lostCornerCount > 0) {
          q.visible = false;

          if (lostCornerCount < 4) {
            int n = 0;
            float nY = 0;
            for (int j = 0; j < 4; j++) {
              if (q.adjusted[j].y > nY) {
                nY = q.adjusted[j].y;
                n = j;
              }
            }

            float x0 = q.adjusted[n].x;
            float y0 = q.adjusted[n].y;
            float z0 = q.adjusted[n].z;

            float x3 = q.adjusted[(n + 2) % 4].x;
            float y3 = q.adjusted[(n + 2) % 4].y;
            float z3 = q.adjusted[(n + 2) % 4].z;

            float d0 = std::sqrt(std::pow(x3 - x0, 2) + std::pow(y3 - y0, 2));
            float dx0 = (x3 - x0) / d0;
            float dy0 = (y3 - y0) / d0;

            float lambdaY = omega;
            float lambda = (lambdaY - y0) / dy0;
            float lambdaX = x0 + lambda * dx0;

            float x1 = q.adjusted[(n + 1) % 4].x;
            float y1 = q.adjusted[(n + 1) % 4].y;
            float z1 = q.adjusted[(n + 1) % 4].z;
            float d1 = std::sqrt(std::pow(x1 - x0, 2) + std::pow(y1 - y0, 2) +
                                 std::pow(z1 - z0, 2));
            float dx1 = (x1 - x0) / d1;
            float dy1 = (y1 - y0) / d1;
            float dz1 = (z1 - z0) / d1;

            float x2 = q.adjusted[(n + 3) % 4].x;
            float y2 = q.adjusted[(n + 3) % 4].y;
            float z2 = q.adjusted[(n + 3) % 4].z;
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
              du = tileSize * alpha / d1;
              dv = tileSize * alpha / d2;
              order = {0, 1, 2, 3};
            }

            if (n == 1) {
              u = 0;
              v = tileSize * (1 - alpha / d2);
              du = tileSize * alpha / d1;
              dv = tileSize * alpha / d2;
              order = {3, 0, 1, 2};
            }

            if (n == 2) {
              u = tileSize * (1 - alpha / d1);
              v = tileSize * (1 - alpha / d2);
              du = tileSize * alpha / d1;
              dv = tileSize * alpha / d2;
              order = {2, 3, 0, 1};
            }

            if (n == 3) {
              u = tileSize * (1 - alpha / d1);
              v = 0;
              du = tileSize * alpha / d1;
              dv = tileSize * alpha / d2;
              order = {1, 2, 3, 0};
            }

            q.partials.push_back(partial(
                {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                {static_cast<int>(u), static_cast<int>(v), static_cast<int>(du),
                 static_cast<int>(dv)},
                q.colour));

            if (y1 >= omega) {
              float yTheta = omega;
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
                v = tileSize * alpha / d1;
                du = tileSize * theta;
                dv = tileSize * (1 - alpha / d1);
                order = {0, 1, 2, 3};
              }

              if (n == 1) {
                u = tileSize * alpha / d1;
                v = tileSize * (1 - theta);
                du = tileSize * (1 - alpha / d1);
                dv = tileSize * theta;
                order = {3, 0, 1, 2};
              }

              if (n == 2) {
                u = tileSize * (1 - theta);
                v = 0;
                du = tileSize * theta;
                dv = tileSize * (1 - alpha / d1);
                order = {2, 3, 0, 1};
              }

              if (n == 3) {
                u = 0;
                v = 0;
                du = tileSize * (1 - alpha / d1);
                dv = tileSize * theta;
                order = {1, 2, 3, 0};
              }

              q.partials.push_back(partial(
                  {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                  {static_cast<int>(u), static_cast<int>(v),
                   static_cast<int>(du), static_cast<int>(dv)},
                  q.colour));
            }

            if (y2 >= omega) {
              float yTheta = omega;
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
                u = tileSize * alpha / d1;
                v = 0;
                du = tileSize * (1 - alpha / d1);
                dv = tileSize * theta;
                order = {0, 1, 2, 3};
              }

              if (n == 1) {
                u = 0;
                v = 0;
                du = tileSize * theta;
                dv = tileSize * (1 - alpha / d1);
                order = {3, 0, 1, 2};
              }

              if (n == 2) {
                u = 0;
                v = tileSize * (1 - theta);
                du = tileSize * (1 - alpha / d1);
                dv = tileSize * theta;
                order = {2, 3, 0, 1};
              }

              if (n == 3) {
                u = tileSize * (1 - theta);
                v = tileSize * alpha / d1;
                du = tileSize * theta;
                dv = tileSize * (1 - alpha / d1);
                order = {1, 2, 3, 0};
              }

              q.partials.push_back(partial(
                  {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                  {static_cast<int>(u), static_cast<int>(v),
                   static_cast<int>(du), static_cast<int>(dv)},
                  q.colour));
            }
          }
        }
      } else {
        q.sourcePos.x = 0;
        q.sourceSize.x = tileSize;

        bool cropped[4]{false, false, false, false};

        int cropCount = 0;

        for (int i = 0; i < 4; i++) {
          if (q.adjusted[i].y < omega) {
            cropped[i] = true;
            cropCount++;
          }
        }

        if (cropCount > 2) {
          q.visible = false;
          continue;
        }

        if (cropCount > 0) {
          int key = -1;
          float lowestY = omega;
          for (int i = 0; i < 4; i++) {
            if (q.adjusted[i].y < lowestY) {
              lowestY = q.adjusted[i].y;
              key = i;
            }
          }

          if (key == -1) {
            q.visible = false;
            continue;
          }

          q.cropped = true;

          float keyY = q.adjusted[key].y;
          float pairY = q.adjusted[q.vertices[key].pair].y;

          float delta = 1 - (omega - pairY) / (keyY - pairY);

          for (int i = 0; i < 4; i++) {
            if (!cropped[i]) continue;

            int pair = q.vertices[i].pair;

            q.adjusted[i].x = q.adjusted[i].x +
                              delta * (q.adjusted[pair].x - q.adjusted[i].x);

            q.adjusted[i].y = q.adjusted[i].y +
                              delta * (q.adjusted[pair].y - q.adjusted[i].y);

            q.adjusted[i].z = q.adjusted[i].z +
                              delta * (q.adjusted[pair].z - q.adjusted[i].z);

            if (i > pair) {
              q.sourcePos.x = 0;
              q.sourceSize.x = tileSize * (1 - delta);
            } else {
              q.sourcePos.x = delta * tileSize;
              q.sourceSize.x = (1 - delta) * tileSize;
            }
          }
        }
      }

      float ax = q.adjusted[1].x - q.adjusted[0].x;
      float ay = q.adjusted[1].y - q.adjusted[0].y;
      float az = q.adjusted[1].z - q.adjusted[0].z;

      float bx = q.adjusted[3].x - q.adjusted[0].x;
      float by = q.adjusted[3].y - q.adjusted[0].y;
      float bz = q.adjusted[3].z - q.adjusted[0].z;

      float cx = q.adjusted[0].x + (q.adjusted[3].x - q.adjusted[0].x) / 2 +
                 (q.adjusted[1].x - q.adjusted[0].x) / 2;
      float cy = q.adjusted[0].y + (q.adjusted[3].y - q.adjusted[0].y) / 2 +
                 (q.adjusted[1].y - q.adjusted[0].y) / 2;
      float cz = q.adjusted[0].z + (q.adjusted[3].z - q.adjusted[0].z) / 2 +
                 (q.adjusted[1].z - q.adjusted[0].z) / 2;

      q.normal =
          vertex(ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx);
      q.centre = vertex(cx, cy, cz);

      float dotProduct = q.centre.x * q.normal.x + q.centre.y * q.normal.y +
                         q.centre.z * q.normal.z;

      q.dSquared = std::pow(q.centre.x, 2) + std::pow(q.centre.y, 2) +
                   std::pow(q.centre.z, 2);

      if (q.partials.size() == 0) {
        q.minProjectedX = w;
        q.minProjectedY = h;
        q.maxProjectedX = 0;
        q.maxProjectedY = 0;
        for (int i = 0; i < 4; i++) {
          q.projected[i] = {
              w / 2 - (q.adjusted[i].x * zoom) / (q.adjusted[i].y + omega),
              h / 2 + (q.adjusted[i].z * zoom) / (q.adjusted[i].y + omega)};
          if (q.projected[i].x < q.minProjectedX)
            q.minProjectedX = q.projected[i].x;
          if (q.projected[i].y < q.minProjectedY)
            q.minProjectedY = q.projected[i].y;
          if (q.projected[i].x > q.maxProjectedX)
            q.maxProjectedX = q.projected[i].x;
          if (q.projected[i].y > q.maxProjectedY)
            q.maxProjectedY = q.projected[i].y;
        }
      } else {
        for (auto& p : q.partials) {
          for (int i = 0; i < 4; i++) {
            p.projected[i].x =
                w / 2 - (p.adjusted[i].x * zoom) / (p.adjusted[i].y + omega);
            p.projected[i].y =
                h / 2 + (p.adjusted[i].z * zoom) / (p.adjusted[i].y + omega);
          }
        }
      }

      if (dotProduct < 0) {
        q.visible = false;
      }
    }
  }

  void updateCursor() {
    if (GetMouse(2).bHeld || GetKey(olc::Key::SHIFT).bHeld) return;

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

  void sortQuads() {
    sortedQuads.clear();

    for (auto& quad : quads) {
      if (quad.outOfRange) continue;
      if (quad.dSquared > drawDistance * drawDistance) continue;
      if (quad.cropped && pitch != 0) continue;
      float fade = 1 - std::sqrt(quad.dSquared) / drawDistance;
      if (fade < 0) continue;

      if (cursorX == quad.mapX && cursorY == quad.mapY) {
        quad.colour = olc::Pixel(255, 255, 255);
      } else {
        if (day) {
          int alpha = 255;
          if (fade < 0.25) {
            alpha = static_cast<int>(255.0f * 4 * fade);
          }
          int rgb = static_cast<int>(64 + 128 * std::pow(fade, 2));
          quad.colour = olc::Pixel(rgb, rgb, rgb, alpha);
        } else {
          int rgb = static_cast<int>(255.0f * std::pow(fade, 2));
          quad.colour = olc::Pixel(rgb, rgb, rgb);
        }
      }

      sortedQuads.push_back(&quad);
    }

    std::sort(sortedQuads.begin(), sortedQuads.end(),
              [](quad* a, quad* b) { return a->dSquared > b->dSquared; });
  }

  void renderMazeMap() {
    const int size = 8;
    for (int i = 0; i < mazeWidth * 2 + 1; i++) {
      for (int j = 0; j < mazeHeight * 2 + 1; j++) {
        if (map[i][j].type == cellType::wall) {
          FillRect({i * size, j * size}, {size, size}, olc::WHITE);
        } else if (map[i][j].type == cellType::corridor) {
          FillRect({i * size, j * size}, {size, size}, olc::BLUE);
        } else if (map[i][j].type == cellType::lowroom) {
          FillRect({i * size, j * size}, {size, size}, olc::DARK_BLUE);
        } else if (map[i][j].type == cellType::highroom) {
          FillRect({i * size, j * size}, {size, size}, olc::VERY_DARK_BLUE);
        } else {
          FillRect({i * size, j * size}, {size, size}, olc::BLACK);
        }
      }
    }

    int mapX = myX / unit + mazeWidth;
    int mapY = myY / unit + mazeHeight;

    FillRect({mapX * size, mapY * size}, {size, size}, olc::RED);
    DrawLine(
        {mapX * size + size / 2, mapY * size + size / 2},
        {mapX * size + size / 2 + static_cast<int>(size * 2 * std::sin(angle)),
         mapY * size + size / 2 + static_cast<int>(size * 2 * std::cos(angle))},
        olc::RED);
  }

  void renderQuadMap() {
    for (auto q : quads) {
      if (q.partials.size() > 0) {
        for (auto p : q.partials) {
          olc::vf2d pv[4] = {
              {-p.adjusted[0].x + w / 2, -p.adjusted[0].y + h / 2},
              {-p.adjusted[1].x + w / 2, -p.adjusted[1].y + h / 2},
              {-p.adjusted[2].x + w / 2, -p.adjusted[2].y + h / 2},
              {-p.adjusted[3].x + w / 2, -p.adjusted[3].y + h / 2}};

          DrawLine(-p.adjusted[0].x + w / 2, -p.adjusted[0].y + h / 2,
                   -p.adjusted[1].x + w / 2, -p.adjusted[1].y + h / 2,
                   olc::YELLOW);

          DrawLine(-p.adjusted[1].x + w / 2, -p.adjusted[1].y + h / 2,
                   -p.adjusted[2].x + w / 2, -p.adjusted[2].y + h / 2,
                   olc::YELLOW);

          DrawLine(-p.adjusted[2].x + w / 2, -p.adjusted[2].y + h / 2,
                   -p.adjusted[3].x + w / 2, -p.adjusted[3].y + h / 2,
                   olc::YELLOW);

          DrawLine(-p.adjusted[3].x + w / 2, -p.adjusted[3].y + h / 2,
                   -p.adjusted[0].x + w / 2, -p.adjusted[0].y + h / 2,
                   olc::YELLOW);
        }

      } else if (q.visible) {
        if (!q.wall) {
          olc::vf2d pv[4] = {
              {-q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2},
              {-q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2},
              {-q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2},
              {-q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2}};

        } else {
          DrawLine(-q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                   -q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                   olc::GREEN);
          DrawLine(-q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                   -q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                   olc::GREEN);
          DrawLine(-q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                   -q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                   olc::GREEN);
          DrawLine(-q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                   -q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                   olc::GREEN);
          FillCircle(-q.centre.x + w / 2, -q.centre.y + h / 2, 3, olc::GREEN);
        }
      } else {
        DrawLine(-q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                 -q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
        DrawLine(-q.adjusted[1].x + w / 2, -q.adjusted[1].y + h / 2,
                 -q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
        DrawLine(-q.adjusted[2].x + w / 2, -q.adjusted[2].y + h / 2,
                 -q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
        DrawLine(-q.adjusted[3].x + w / 2, -q.adjusted[3].y + h / 2,
                 -q.adjusted[0].x + w / 2, -q.adjusted[0].y + h / 2,
                 q.wall ? olc::DARK_RED : olc::DARK_MAGENTA);
      }
    }

    DrawLine(0, h / 2, w, h / 2, olc::BLUE);
    DrawLine(0, h / 2 - omega, w, h / 2 - omega, olc::YELLOW);

    FillCircle(w / 2, h / 2, 5, olc::GREEN);
  }

  void renderQuads() {
    int qCount = 0;

    for (auto& q : sortedQuads) {
      float dotProduct = q->centre.x * q->normal.x + q->centre.y * q->normal.y +
                         q->centre.z * q->normal.z;

      auto decal = q->renderable->decal;

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

    if (qCount > qMax) qMax = qCount;

    if (showHelp) {
      std::ostringstream stringStream;
      stringStream << "Quads: " << qCount << " (Max: " << qMax
                   << ") | Zoom: " << static_cast<int>(200 * zoom / w)
                   << "% | Range: " << drawDistance << " | " << GetFPS()
                   << " FPS" << std::endl;

      if (quad::cursorQuad != nullptr) {
        stringStream << "Cursor " << cursorX << ", " << cursorY << ", "
                     << "Wall: " << (quad::cursorQuad->wall ? "True" : "False")
                     << ", Level: " << quad::cursorQuad->wallF
                     << ", Direction: " << quad::cursorQuad->wallD
                     << (autoTexture ? " [Auto]" : "") << std::endl;
      }

      stringStream << "./maps/level" << levelNo << ".dat";

      DrawStringDecal({10, 10}, stringStream.str(), olc::WHITE);
    }
  }

  void renderTileSelector() {
    int iMax = static_cast<int>(w / (tileSize + 10));
    int jMax = static_cast<int>(176 / iMax) + 1;
    for (int j = 0; j < jMax; j++) {
      for (int i = 0; i < iMax; i++) {
        int k = j * iMax + i;
        if (k >= 176) continue;
        DrawDecal({static_cast<float>(i * (tileSize + 10) + 10),
                   static_cast<float>(j * (tileSize + 10) + 10)},
                  texture[k].decal, {1, 1},
                  k == selectedTexture ? olc::WHITE : olc::GREY);
      }
    }
  }

  void renderHelp() {
    std::ostringstream stringStream;
    stringStream << "W/S/A/D - Move" << std::endl
                 << "Left Click - Apply Texture (One surface)" << std::endl
                 << "T - Apply Texture (whole column, exlc. floor)" << std::endl
                 << "Right Click (Drag) or Left/Right - Turn" << std::endl
                 << "Middle Click (Hold) or Shift or MouseWheel" << std::endl
                 << "    - Choose Texture" << std::endl
                 << "Q - Pick Texture" << std::endl
                 << "-/+ - Zoom" << std::endl
                 << "[/] - Render Distance" << std::endl
                 << "R/F - Raise / Lower Ceiling" << std::endl
                 << "PgUp/PgDn - Fly Up / Down" << std::endl
                 << "End - Reset Height" << std::endl
                 << "Ctrl + PgUp/PgDn - Look up / down (Experimental)"
                 << std::endl
                 << "Home - Reset View (Zoom, Render Distance, Pitch)"
                 << std::endl
                 << "Tab - Show map" << std::endl
                 << "Tab - Show map" << std::endl
                 << "Ctrl + \\ - Show rendering details" << std::endl
                 << "Ctrl + G - Generate new maze" << std::endl
                 << "Ctrl + M - Generate empty map" << std::endl
                 << "N - Switch between night and day" << std::endl
                 << "Ctrl + S - Save level" << std::endl
                 << "Ctrl + L - Load level" << std::endl
                 << "1,2,3...0 - Pick save slot, 1-10" << std::endl
                 << "H - Toggle Help (this text!)" << std::endl;

    DrawStringDecal({2 * w / 3, 10}, stringStream.str(), olc::WHITE);
  }

  bool OnUserUpdate(float fElapsedTime) override {
    handleInputs(fElapsedTime);

    updateMatrix();

    handleInteractions();

    updateQuads();

    updateCursor();

    if (day) {
      Clear(olc::Pixel(96, 128, 255));
    } else {
      Clear(olc::BLACK);
    }

    if (GetKey(olc::Key::TAB).bHeld) {
      renderMazeMap();
    } else if (GetKey(olc::Key::CTRL).bHeld && GetKey(olc::Key::OEM_5).bHeld) {
      renderQuadMap();
    } else {
      sortQuads();
      renderQuads();
      if (GetMouse(2).bHeld || GetKey(olc::Key::SHIFT).bHeld) {
        renderTileSelector();
      } else {
        DrawDecal({w - (tileSize + 10), 10}, texture[selectedTexture].decal,
                  {1, 1});
      }
      if (showHelp) renderHelp();
    }

    return !exitSignal;
  }
};

int main() {
  Olc3d2 demo;
  if (demo.Construct(w, h, 1, 1)) demo.Start();
  return 0;
}
