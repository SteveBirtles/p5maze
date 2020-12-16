#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

const int w = 1280;
const int h = 1024;
const int tileSize = 64;
const float omega = 0.1;
float drawDistance = 1500;
float zoom = w / 2;
float cursorRange = 2.5;

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
  Renderable* renderable;
  std::array<vertex, 4> vertices;
  std::array<vertex, 4> adjusted;
  olc::vi2d sourcePos;
  olc::vi2d sourceSize;
  bool visible;
  bool wall;
  bool cropped;
  double dSquared;
  olc::vf2d projected[4];
  std::vector<partial> partials;
  vertex normal;
  vertex centre;
  olc::Pixel colour;
  quad(){};
  quad(vertex _p0, vertex _p1, vertex _p2, vertex _p3, bool _wall,
       int textureNo = 7, olc::Pixel _colour = olc::WHITE) {
    vertices[0] = _p0;
    vertices[1] = _p1;
    vertices[2] = _p2;
    vertices[3] = _p3;
    sourcePos.x = 0;
    sourcePos.y = 0;
    sourceSize.x = tileSize;
    sourceSize.y = tileSize;
    wall = _wall;
    colour = _colour;
    renderable = &texture[textureNo];
  }
};

std::vector<quad> quads;
std::vector<quad*> sortedQuads;
quad cursor;

enum class cellType { wall, corridor, lowroom, highroom };

struct mapCell {
  cellType type;
  int floor;
  int wall;
  int ceiling;
  mapCell(cellType _type = cellType::wall, int _floor = 174, int _wall = 174,
          int _ceiling = 174) {
    type = _type;
    floor = _floor;
    wall = _wall;
    ceiling = _ceiling;
  }
};

struct mazeCell {
  int set;
  bool right;
  bool down;
};

const float unit = 100;
const int mazeWidth = 40;
const int mazeHeight = 40;

mapCell map[mazeWidth * 2 + 1][mazeHeight * 2 + 1];
mazeCell maze[mazeWidth][mazeHeight];

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
          map[i + mazeWidth][j + mazeHeight].type == cellType::highroom) {
        quads.push_back(quad(vertex(x1, y1, unit, -1), vertex(x2, y2, unit, -1),
                             vertex(x3, y3, unit, -1), vertex(x4, y4, unit, -1),
                             false, map[i + mazeWidth][j + mazeHeight].floor));

        if (map[i + mazeWidth][j + mazeHeight].type == cellType::corridor) {
          quads.push_back(quad(vertex(x4, y4, 0, -1), vertex(x3, y3, 0, -1),
                               vertex(x2, y2, 0, -1), vertex(x1, y1, 0, -1),
                               false,
                               map[i + mazeWidth][j + mazeHeight].ceiling));
        }

        if (map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom) {
          quads.push_back(
              quad(vertex(x4, y4, -unit, -1), vertex(x3, y3, -unit, -1),
                   vertex(x2, y2, -unit, -1), vertex(x1, y1, -unit, -1), false,
                   map[i + mazeWidth][j + mazeHeight].ceiling));
        }

        if (map[i + mazeWidth][j + mazeHeight].type == cellType::highroom) {
          quads.push_back(
              quad(vertex(x4, y4, -unit * 2, -1), vertex(x3, y3, -unit * 2, -1),
                   vertex(x2, y2, -unit * 2, -1), vertex(x1, y1, -unit * 2, -1),
                   false, map[i + mazeWidth][j + mazeHeight].ceiling));
        }

      } else if (map[i + mazeWidth][j + mazeHeight].type == cellType::wall) {
        if (j > -mazeHeight &&
            map[i + mazeWidth][j + mazeHeight - 1].type != cellType::wall) {
          quads.push_back(quad(vertex(x2, y2, unit, 3), vertex(x2, y2, 0.0f, 2),
                               vertex(x1, y1, 0.0f, 1), vertex(x1, y1, unit, 0),
                               true,
                               map[i + mazeWidth][j + mazeHeight - 1].wall));
        }

        if (i < mazeWidth - 1 &&
            map[i + mazeWidth + 1][j + mazeHeight].type != cellType::wall) {
          quads.push_back(quad(vertex(x3, y3, unit, 3), vertex(x3, y3, 0.0f, 2),
                               vertex(x2, y2, 0.0f, 1), vertex(x2, y2, unit, 0),
                               true,
                               map[i + mazeWidth + 1][j + mazeHeight].wall));
        }

        if (j < mazeHeight - 1 &&
            map[i + mazeWidth][j + mazeHeight + 1].type != cellType::wall) {
          quads.push_back(quad(vertex(x4, y4, unit, 3), vertex(x4, y4, 0.0f, 2),
                               vertex(x3, y3, 0.0f, 1), vertex(x3, y3, unit, 0),
                               true,
                               map[i + mazeWidth][j + mazeHeight + 1].wall));
        }

        if (i > -mazeHeight &&
            map[i + mazeWidth - 1][j + mazeHeight].type != cellType::wall) {
          quads.push_back(quad(vertex(x1, y1, unit, 3), vertex(x1, y1, 0.0f, 2),
                               vertex(x4, y4, 0.0f, 1), vertex(x4, y4, unit, 0),
                               true,
                               map[i + mazeWidth - 1][j + mazeHeight].wall));
        }
      }

      for (int k = -1; k <= 0; k++) {
        if (j > -mazeHeight &&
            (map[i + mazeWidth][j + mazeHeight - 1].type == cellType::lowroom &&
                 k == 0 ||
             map[i + mazeWidth][j + mazeHeight - 1].type ==
                 cellType::highroom) &&
            map[i + mazeWidth][j + mazeHeight - 1].type !=
                map[i + mazeWidth][j + mazeHeight].type &&
            !((map[i + mazeWidth][j + mazeHeight - 1].type ==
                   cellType::lowroom ||
               map[i + mazeWidth][j + mazeHeight - 1].type ==
                   cellType::highroom) &&
              (map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom ||
               map[i + mazeWidth][j + mazeHeight].type == cellType::highroom) &&
              k == 0)

        ) {
          quads.push_back(quad(
              vertex(x2, y2, k * unit, 3), vertex(x2, y2, k * unit - unit, 2),
              vertex(x1, y1, k * unit - unit, 1), vertex(x1, y1, k * unit, 0),
              true, map[i + mazeWidth][j + mazeHeight].wall));
        }

        if (i < mazeWidth - 1 &&
            (map[i + mazeWidth + 1][j + mazeHeight].type == cellType::lowroom &&
                 k == 0 ||
             map[i + mazeWidth + 1][j + mazeHeight].type ==
                 cellType::highroom) &&
            map[i + mazeWidth + 1][j + mazeHeight].type !=
                map[i + mazeWidth][j + mazeHeight].type &&
            !((map[i + mazeWidth + 1][j + mazeHeight].type ==
                   cellType::lowroom ||
               map[i + mazeWidth + 1][j + mazeHeight].type ==
                   cellType::highroom) &&
              (map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom ||
               map[i + mazeWidth][j + mazeHeight].type == cellType::highroom) &&
              k == 0)

        ) {
          quads.push_back(quad(
              vertex(x3, y3, k * unit, 3), vertex(x3, y3, k * unit - unit, 2),
              vertex(x2, y2, k * unit - unit, 1), vertex(x2, y2, k * unit, 0),
              true, map[i + mazeWidth][j + mazeHeight].wall));
        }

        if (j < mazeHeight - 1 &&
            (map[i + mazeWidth][j + mazeHeight + 1].type == cellType::lowroom &&
                 k == 0 ||
             map[i + mazeWidth][j + mazeHeight + 1].type ==
                 cellType::highroom) &&
            map[i + mazeWidth][j + mazeHeight + 1].type !=
                map[i + mazeWidth][j + mazeHeight].type &&
            !((map[i + mazeWidth][j + mazeHeight + 1].type ==
                   cellType::lowroom ||
               map[i + mazeWidth][j + mazeHeight + 1].type ==
                   cellType::highroom) &&
              (map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom ||
               map[i + mazeWidth][j + mazeHeight].type == cellType::highroom) &&
              k == 0)

        ) {
          quads.push_back(quad(
              vertex(x4, y4, k * unit, 3), vertex(x4, y4, k * unit - unit, 2),
              vertex(x3, y3, k * unit - unit, 1), vertex(x3, y3, k * unit, 0),
              true, map[i + mazeWidth][j + mazeHeight].wall));
        }

        if (i > -mazeHeight &&
            (map[i + mazeWidth - 1][j + mazeHeight].type == cellType::lowroom &&
                 k == 0 ||
             map[i + mazeWidth - 1][j + mazeHeight].type ==
                 cellType::highroom) &&
            map[i + mazeWidth - 1][j + mazeHeight].type !=
                map[i + mazeWidth][j + mazeHeight].type &&
            !((map[i + mazeWidth - 1][j + mazeHeight].type ==
                   cellType::lowroom ||
               map[i + mazeWidth - 1][j + mazeHeight].type ==
                   cellType::highroom) &&
              (map[i + mazeWidth][j + mazeHeight].type == cellType::lowroom ||
               map[i + mazeWidth][j + mazeHeight].type == cellType::highroom) &&
              k == 0)

        ) {
          quads.push_back(quad(
              vertex(x1, y1, k * unit, 3), vertex(x1, y1, k * unit - unit, 2),
              vertex(x4, y4, k * unit - unit, 1), vertex(x4, y4, k * unit, 0),
              true, map[i + mazeWidth][j + mazeHeight].wall));
        }
      }
    }
  }
}

bool kruskalStep() {
  bool oneSet = true;
  for (int i = 0; i < mazeWidth; i++) {
    for (int j = 0; j < mazeHeight; j++) {
      if (maze[i][j].set != maze[0][0].set) {
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

    if (horizontal > 0 && (maze[x][y].right || x == mazeWidth - 1)) continue;
    if (vertical > 0 && (maze[x][y].down || y == mazeHeight - 1)) continue;

    a = maze[x][y].set;
    b = maze[x + horizontal][y + vertical].set;

    if (a == b) continue;

    if (vertical > 0) {
      maze[x][y].down = true;
    } else {
      maze[x][y].right = true;
    }
    for (int i = 0; i < mazeWidth; i++) {
      for (int j = 0; j < mazeHeight; j++) {
        if (maze[i][j].set == b) {
          maze[i][j].set = a;
        }
      }
    }

    return false;
  }
}

void randomiseMap() {
  int n = 0;
  for (int i = 0; i < mazeWidth; i++) {
    for (int j = 0; j < mazeHeight; j++) {
      n++;
      maze[i][j].set = n;
      maze[i][j].right = false;
      maze[i][j].down = false;
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
      map[i + mazeWidth][j + mazeHeight].floor = 1;
      map[i + mazeWidth][j + mazeHeight].wall = 4;
      map[i + mazeWidth][j + mazeHeight].ceiling = 7;
    }
  }

  for (int i = 0; i < mazeWidth; i++) {
    for (int j = 0; j < mazeHeight; j++) {
      if (maze[i][j].right) {
        map[i * 2 + 2][j * 2 + 1].type = cellType::corridor;
        map[i * 2 + 2][j * 2 + 1].floor = 1;
        map[i * 2 + 2][j * 2 + 1].wall = 4;
        map[i * 2 + 2][j * 2 + 1].ceiling = 7;
      }
      if (maze[i][j].down) {
        map[i * 2 + 1][j * 2 + 2].type = cellType::corridor;
        map[i * 2 + 1][j * 2 + 2].floor = 1;
        map[i * 2 + 1][j * 2 + 2].wall = 4;
        map[i * 2 + 1][j * 2 + 2].ceiling = 7;
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
        map[i][j].floor = 1;
        map[i][j].wall = 4;
        map[i][j].ceiling = 7;
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
        map[i][j].floor = 1;
        map[i][j].wall = 4;
        map[i][j].ceiling = 7;
      }
    }
  }

  regenerateQuads();
}

class Olc3d2 : public olc::PixelGameEngine {
 public:
  Olc3d2() { sAppName = "Olc3d2"; }

 public:
  bool OnUserCreate() override {
    for (int i = 0; i < 176; i++) {
      std::ostringstream filename;
      filename << "tiles/tile" << i << ".png";
      texture[i].Load(filename.str());
    }

    std::srand(std::time(nullptr));
    randomiseMap();

    return true;
  }

  float angle = 0;
  float pitch = 0;
  float myX = unit / 2;
  float myY = unit / 2;
  float myZ = unit / 2;

  int qMax = 0;

  olc::vi2d lastMousePos = {w / 2, h / 2};

  bool OnUserUpdate(float fElapsedTime) override {
    if (GetKey(olc::Key::LEFT).bHeld) angle += 0.025;
    if (GetKey(olc::Key::RIGHT).bHeld) angle -= 0.025;

    olc::vi2d mousePos = GetMousePos();

    if (GetMouse(0).bHeld)
      angle += static_cast<float>(mousePos.x - lastMousePos.x) / 600;

    lastMousePos = mousePos;

    int cursorX = std::round(myX / unit + mazeWidth);
    int cursorY = std::round(myY / unit + mazeHeight);

    cursorX += cursorRange * std::sin(angle);
    cursorY += cursorRange * std::cos(angle);

    if (cursorX >= 0 && cursorY >= 0 && cursorX <= mazeWidth * 2 &&
        cursorY <= mazeHeight * 2) {
      if (GetKey(olc::Key::SPACE).bHeld) map[cursorX][cursorY].floor = 116;
      if (GetKey(olc::Key::K1).bHeld)
        map[cursorX][cursorY].type = cellType::wall;
      if (GetKey(olc::Key::K2).bHeld)
        map[cursorX][cursorY].type = cellType::corridor;
      if (GetKey(olc::Key::K3).bHeld)
        map[cursorX][cursorY].type = cellType::lowroom;
      if (GetKey(olc::Key::K4).bHeld)
        map[cursorX][cursorY].type = cellType::highroom;
    }
    if (GetKey(olc::Key::SPACE).bReleased || GetKey(olc::Key::K1).bReleased ||
        GetKey(olc::Key::K2).bReleased || GetKey(olc::Key::K3).bReleased ||
        GetKey(olc::Key::K4).bReleased) {
      regenerateQuads();
    }

    if (GetKey(olc::Key::OEM_4).bHeld) drawDistance /= 1.05;
    if (GetKey(olc::Key::OEM_6).bHeld) drawDistance *= 1.05;

    if (drawDistance < unit) drawDistance = unit;

    if (GetKey(olc::Key::PGDN).bHeld) pitch += 0.025;
    if (GetKey(olc::Key::PGUP).bHeld) pitch -= 0.025;

    if (GetKey(olc::Key::HOME).bHeld) {
      pitch = 0;
      zoom = w / 2;
      drawDistance = 1500;
    }

    if (GetKey(olc::Key::EQUALS).bHeld) zoom *= 1.05;
    if (GetKey(olc::Key::MINUS).bHeld) zoom /= 1.05;
    if (zoom < w / 4) zoom = w / 4;
    if (zoom > w * 5) zoom = w * 5;

    if (GetKey(olc::Key::UP).bHeld) cursorRange *= 1.05;
    if (GetKey(olc::Key::DOWN).bHeld) cursorRange /= 1.05;
    if (cursorRange < 2) cursorRange = 2;
    if (cursorRange > 10) cursorRange = 10;

    if (GetKey(olc::Key::M).bPressed) randomiseMap();

    if (GetKey(olc::Key::G).bPressed) {
      for (int i = -mazeWidth; i <= mazeWidth; i++) {
        for (int j = -mazeHeight; j <= mazeHeight; j++) {
          if (i == -mazeWidth || j == -mazeWidth || i == mazeWidth ||
              j == mazeWidth) {
            map[i + mazeWidth][j + mazeHeight].type = cellType::wall;
            map[i + mazeWidth][j + mazeHeight].wall = 60;
            map[i + mazeWidth][j + mazeHeight].floor = 60;
            map[i + mazeWidth][j + mazeHeight].ceiling = 60;
          } else {
            map[i + mazeWidth][j + mazeHeight].type = cellType::highroom;
            map[i + mazeWidth][j + mazeHeight].wall = 60;
            map[i + mazeWidth][j + mazeHeight].floor = 60;
            map[i + mazeWidth][j + mazeHeight].ceiling = 60;
          }
        }
      }
      regenerateQuads();
    }

    if (pitch < -1.571) pitch = -1.571;
    if (pitch > 1.571) pitch = 1.571;

    if (GetKey(olc::Key::W).bHeld) {
      myX += std::sin(angle) * 5;
      myY += std::cos(angle) * 5;
    }
    if (GetKey(olc::Key::S).bHeld) {
      myX -= std::sin(angle) * 5;
      myY -= std::cos(angle) * 5;
    }
    if (GetKey(olc::Key::A).bHeld) {
      myX += std::cos(angle) * 5;
      myY += -std::sin(angle) * 5;
    }
    if (GetKey(olc::Key::D).bHeld) {
      myX -= std::cos(angle) * 5;
      myY -= -std::sin(angle) * 5;
    }
    if (GetKey(olc::Key::R).bHeld) myZ -= 5;
    if (GetKey(olc::Key::F).bHeld) myZ += 5;
    if (GetKey(olc::Key::END).bHeld) myZ = unit / 2;

    int mapX = myX / unit + mazeWidth;
    int mapY = myY / unit + mazeHeight;

    bool north = false, south = false, east = false, west = false;
    float dNorth = 0, dSouth = 0, dEast = 0, dWest = 0;
    double intBit;

    if (mapX > 0 && mapY >= 0 && mapX < mazeWidth * 2 + 1 &&
        mapY < mazeWidth * 2 + 1) {
      if (map[mapX - 1][mapY].type == cellType::wall) east = true;
    }

    if (mapX >= 0 && mapY >= 0 && mapX < mazeWidth * 2 &&
        mapY < mazeWidth * 2 + 1) {
      if (map[mapX + 1][mapY].type == cellType::wall) west = true;
    }

    if (mapX >= 0 && mapY > 0 && mapX < mazeWidth * 2 + 1 &&
        mapY < mazeWidth * 2 + 1) {
      if (map[mapX][mapY - 1].type == cellType::wall) north = true;
    }

    if (mapX >= 0 && mapY >= 0 && mapX < mazeWidth * 2 + 1 &&
        mapY < mazeWidth * 2) {
      if (map[mapX][mapY + 1].type == cellType::wall) south = true;
    }

    if (east) {
      dEast = std::modf(myX / unit + mazeWidth - mapX, &intBit);
      if (dEast <= 0.25) myX += (0.25 - dEast) * unit;
    }
    if (west) {
      dWest = 1 - std::modf(myX / unit + mazeWidth - mapX, &intBit);
      if (dWest <= 0.25) myX -= (0.25 - dWest) * unit;
    }
    if (north) {
      dNorth = std::modf(myY / unit + mazeHeight - mapY, &intBit);
      if (dNorth <= 0.25) myY += (0.25 - dNorth) * unit;
    }
    if (south) {
      dSouth = 1 - std::modf(myY / unit + mazeHeight - mapY, &intBit);
      if (dSouth <= 0.25) myY -= (0.25 - dSouth) * unit;
    }

    float m1[3][3] = {{1, 0, 0},
                      {0, std::cos(pitch), std::sin(pitch)},
                      {0, -std::sin(pitch), std::cos(pitch)}};

    float m2[3][3] = {{std::cos(angle), -std::sin(angle), 0},
                      {std::sin(angle), std::cos(angle), 0},
                      {0, 0, 1}};

    float m[3][3];
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        m[i][j] =
            m1[i][0] * m2[0][j] + m1[i][1] * m2[1][j] + m1[i][2] * m2[2][j];
      }
    }

    float x1 = unit * (cursorX - mazeWidth);
    float y1 = unit * (cursorY - mazeHeight);
    float x2 = x1 + unit;
    float y2 = y1;
    float x4 = x1;
    float y4 = y1 + unit;
    float x3 = x1 + (x2 - x1) + (x4 - x1);
    float y3 = y1 + (y2 - y1) + (y4 - y1);

    cursor =
        quad(vertex(x1, y1, unit, -1), vertex(x2, y2, unit, -1),
             vertex(x3, y3, unit, -1), vertex(x4, y4, unit, -1), false, 124);

    for (int i = 0; i < 4; i++) {
      cursor.adjusted[i].x = (cursor.vertices[i].x - myX) * m[0][0] +
                             (cursor.vertices[i].y - myY) * m[0][1] +
                             (cursor.vertices[i].z - myZ) * m[0][2];

      cursor.adjusted[i].y = (cursor.vertices[i].x - myX) * m[1][0] +
                             (cursor.vertices[i].y - myY) * m[1][1] +
                             (cursor.vertices[i].z - myZ) * m[1][2];

      cursor.adjusted[i].z = (cursor.vertices[i].x - myX) * m[2][0] +
                             (cursor.vertices[i].y - myY) * m[2][1] +
                             (cursor.vertices[i].z - myZ) * m[2][2];
    }

    int cropCount = 0;

    for (int i = 0; i < 4; i++) {
      if (cursor.adjusted[i].y < omega) cropCount++;
    }

    if (cropCount > 0) {
      cursor.visible = false;
    } else {
      cursor.visible = true;
      for (int i = 0; i < 4; i++) {
        cursor.projected[i] = {w / 2 - (cursor.adjusted[i].x * zoom) /
                                           (cursor.adjusted[i].y + omega),
                               h / 2 + (cursor.adjusted[i].z * zoom) /
                                           (cursor.adjusted[i].y + omega)};
      }
    }

    for (auto& q : quads) {
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
        for (int i = 0; i < 4; i++) {
          q.projected[i] = {
              w / 2 - (q.adjusted[i].x * zoom) / (q.adjusted[i].y + omega),
              h / 2 + (q.adjusted[i].z * zoom) / (q.adjusted[i].y + omega)};
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

    sortedQuads.clear();

    for (auto& quad : quads) {
      if (quad.dSquared > drawDistance * drawDistance) continue;
      if (quad.cropped && pitch != 0) continue;
      float fade = 1 - std::sqrt(quad.dSquared) / drawDistance;
      if (fade < 0) continue;

      int rgb = static_cast<int>(255.0f * std::pow(fade, 2));
      quad.colour = olc::Pixel(rgb, rgb, rgb);

      sortedQuads.push_back(&quad);
    }

    Clear(olc::BLACK);

    if (GetKey(olc::Key::TAB).bHeld) {
      const int size = 8;
      for (int i = 0; i < mazeWidth * 2 + 1; i++) {
        for (int j = 0; j < mazeHeight * 2 + 1; j++) {
          if (map[i][j].type == cellType::wall) {
            FillRect({i * size, j * size}, {size, size}, olc::WHITE);
          } else if (map[i][j].type == cellType::lowroom) {
            FillRect({i * size, j * size}, {size, size}, olc::VERY_DARK_BLUE);
          } else {
            FillRect({i * size, j * size}, {size, size}, olc::DARK_BLUE);
          }
        }
      }

      int mapX = myX / unit + mazeWidth;
      int mapY = myY / unit + mazeHeight;

      FillRect({mapX * size, mapY * size}, {size, size}, olc::RED);
      DrawLine({mapX * size + size / 2, mapY * size + size / 2},
               {mapX * size + size / 2 +
                    static_cast<int>(size * 2 * std::sin(angle)),
                mapY * size + size / 2 +
                    static_cast<int>(size * 2 * std::cos(angle))},
               olc::RED);
    } else if (GetKey(olc::Key::OEM_5).bHeld) {
      for (auto q : quads) {
        if (q.partials.size() > 0) {
          for (auto p : q.partials) {
            olc::vf2d pv[4] = {
                {-p.adjusted[0].x + w / 2, -p.adjusted[0].y + h / 2},
                {-p.adjusted[1].x + w / 2, -p.adjusted[1].y + h / 2},
                {-p.adjusted[2].x + w / 2, -p.adjusted[2].y + h / 2},
                {-p.adjusted[3].x + w / 2, -p.adjusted[3].y + h / 2}};

            /*DrawPartialWarpedDecal(floorTexture.decal, pv, p.sourcePos,
                                   p.sourceSize, p.colour);*/

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

            // DrawWarpedDecal(floorTexture.decal, pv, q.colour);

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

    } else {
      std::sort(sortedQuads.begin(), sortedQuads.end(),
                [](quad* a, quad* b) { return a->dSquared > b->dSquared; });

      int qCount = 0;

      for (auto& q : sortedQuads) {
        float dotProduct = q->centre.x * q->normal.x +
                           q->centre.y * q->normal.y +
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

      std::ostringstream stringStream;
      stringStream << "Quads: " << qCount << " (Max: " << qMax
                   << ") Zoom: " << static_cast<int>(200 * zoom / w) << "%";

      DrawStringDecal({10, 10}, stringStream.str(), olc::WHITE);

      if (cursor.visible) {
        DrawWarpedDecal(cursor.renderable->decal, cursor.projected);
      }
    }

    return true;
  }
};

int main() {
  Olc3d2 demo;
  if (demo.Construct(w, h, 1, 1)) demo.Start();
  return 0;
}
