#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

const int w = 1280;
const int h = 1024;
const float omega = 10;

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

Renderable plasmaTexture;
Renderable wallTexture;
Renderable floorTexture;

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
  std::array<vertex, 4> vertices;
  std::array<vertex, 4> adjusted;
  bool visible;
  bool wall;
  float zOrder;
  olc::vf2d projected[4];
  std::vector<partial> partials;
  vertex normal;
  vertex centre;
  olc::Pixel colour;
  quad(vertex _p0, vertex _p1, vertex _p2, vertex _p3, bool _wall,
       olc::Pixel _colour = olc::WHITE) {
    vertices[0] = _p0;
    vertices[1] = _p1;
    vertices[2] = _p2;
    vertices[3] = _p3;
    wall = _wall;
    colour = _colour;
  }
};

std::vector<quad> quads;
std::vector<quad*> sortedQuads;

float unit = 100;

class Olc3d2 : public olc::PixelGameEngine {
 public:
  Olc3d2() { sAppName = "Olc3d2"; }

 public:
  bool OnUserCreate() override {
    plasmaTexture.Load("./plasma.png");
    wallTexture.Load("./wall.png");
    floorTexture.Load("./floor.png");

    std::srand(std::time(nullptr));

    for (int i = -20; i < 20; i++) {
      for (int j = -20; j < 20; j++) {
        float x1 = i * unit;
        float y1 = j * unit;
        float x2 = x1 + unit;
        float y2 = y1;
        float x4 = x1;
        float y4 = y1 + unit;
        float x3 = x1 + (x2 - x1) + (x4 - x1);
        float y3 = y1 + (y2 - y1) + (y4 - y1);

        quads.push_back(quad(vertex(x1, y1, unit, -1), vertex(x2, y2, unit, -1),
                             vertex(x3, y3, unit, -1), vertex(x4, y4, unit, -1),
                             false));
      }
    }

    for (int i = 0; i < 100; i++) {
      float x1 = unit * (rand() % 40 - 20);

      float y1 = unit * (rand() % 40 - 20);

      float x2 = x1 + unit;
      float y2 = y1;
      float x4 = x1;
      float y4 = y1 + unit;
      float x3 = x1 + (x2 - x1) + (x4 - x1);
      float y3 = y1 + (y2 - y1) + (y4 - y1);

      quads.push_back(quad(vertex(x2, y2, unit, 3), vertex(x2, y2, 0.0f, 2),
                           vertex(x1, y1, 0.0f, 1), vertex(x1, y1, unit, 0),
                           true));

      quads.push_back(quad(vertex(x3, y3, unit, 3), vertex(x3, y3, 0.0f, 2),
                           vertex(x2, y2, 0.0f, 1), vertex(x2, y2, unit, 0),
                           true));

      quads.push_back(quad(vertex(x4, y4, unit, 3), vertex(x4, y4, 0.0f, 2),
                           vertex(x3, y3, 0.0f, 1), vertex(x3, y3, unit, 0),
                           true));

      quads.push_back(quad(vertex(x1, y1, unit, 3), vertex(x1, y1, 0.0f, 2),
                           vertex(x4, y4, 0.0f, 1), vertex(x4, y4, unit, 0),
                           true));
    }

    return true;
  }

  float angle = 0;
  float pitch = 0;
  float myX = 0;
  float myY = 0;
  float myZ = unit / 2;

  bool OnUserUpdate(float fElapsedTime) override {
    if (GetKey(olc::Key::LEFT).bHeld) angle += 0.025;
    if (GetKey(olc::Key::RIGHT).bHeld) angle -= 0.025;

    if (GetKey(olc::Key::PGDN).bHeld) pitch += 0.025;
    if (GetKey(olc::Key::PGUP).bHeld) pitch -= 0.025;

    if (GetKey(olc::Key::HOME).bHeld) pitch = 0;

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

    for (auto& q : quads) {
      for (int i = 0; i < 4; i++) {
        q.adjusted[i].x = (q.vertices[i].x - myX) * m[0][0] +
                          (q.vertices[i].y - myY) * m[0][1] +
                          (q.vertices[i].z - myZ) * m[0][2];
        q.adjusted[i].y = (q.vertices[i].x - myX) * m[1][0] +
                          (q.vertices[i].y - myY) * m[1][1] +
                          (q.vertices[i].z - myZ) * m[1][2];
        q.adjusted[i].z = (q.vertices[i].x - myX) * m[2][0] +
                          (q.vertices[i].y - myY) * m[2][1] +
                          (q.vertices[i].z - myZ) * m[2][2];
        q.visible = true;

        q.partials.clear();
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
              du = 512 * alpha / d1;
              dv = 512 * alpha / d2;
              order = {0, 1, 2, 3};
            }

            if (n == 1) {
              u = 0;
              v = 512 * (1 - alpha / d2);
              du = 512 * alpha / d1;
              dv = 512 * alpha / d2;
              order = {3, 0, 1, 2};
            }

            if (n == 2) {
              u = 512 * (1 - alpha / d1);
              v = 512 * (1 - alpha / d2);
              du = 512 * alpha / d1;
              dv = 512 * alpha / d2;
              order = {2, 3, 0, 1};
            }

            if (n == 3) {
              u = 512 * (1 - alpha / d1);
              v = 0;
              du = 512 * alpha / d1;
              dv = 512 * alpha / d2;
              order = {1, 2, 3, 0};
            }

            q.partials.push_back(partial(
                {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                {static_cast<int>(u), static_cast<int>(v), static_cast<int>(du),
                 static_cast<int>(dv)}));

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
                v = 512 * alpha / d1;
                du = 512 * theta;
                dv = 512 * (1 - alpha / d1);
                order = {0, 1, 2, 3};
              }

              if (n == 1) {
                u = 512 * alpha / d1;
                v = 512 * (1 - theta);
                du = 512 * (1 - alpha / d1);
                dv = 512 * theta;
                order = {3, 0, 1, 2};
              }

              if (n == 2) {
                u = 512 * (1 - theta);
                v = 0;
                du = 512 * theta;
                dv = 512 * (1 - alpha / d1);
                order = {2, 3, 0, 1};
              }

              if (n == 3) {
                u = 0;
                v = 0;
                du = 512 * (1 - alpha / d1);
                dv = 512 * theta;
                order = {1, 2, 3, 0};
              }

              q.partials.push_back(partial(
                  {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                  {static_cast<int>(u), static_cast<int>(v),
                   static_cast<int>(du), static_cast<int>(dv)}));
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
                u = 512 * alpha / d1;
                v = 0;
                du = 512 * (1 - alpha / d1);
                dv = 512 * theta;
                order = {0, 1, 2, 3};
              }

              if (n == 1) {
                u = 0;
                v = 0;
                du = 512 * theta;
                dv = 512 * (1 - alpha / d1);
                order = {3, 0, 1, 2};
              }

              if (n == 2) {
                u = 0;
                v = 512 * (1 - theta);
                du = 512 * (1 - alpha / d1);
                dv = 512 * theta;
                order = {2, 3, 0, 1};
              }

              if (n == 3) {
                u = 512 * (1 - theta);
                v = 512 * alpha / d1;
                du = 512 * theta;
                dv = 512 * (1 - alpha / d1);
                order = {1, 2, 3, 0};
              }

              q.partials.push_back(partial(
                  {vs[order[0]], vs[order[1]], vs[order[2]], vs[order[3]]},
                  {static_cast<int>(u), static_cast<int>(v),
                   static_cast<int>(du), static_cast<int>(dv)}));
            }
          }
        }
      } else {
        for (int i = 0; i < 4; i++) {
          if (q.adjusted[i].y < omega) {
            int pair = q.vertices[i].pair;

            if (q.adjusted[pair].y < omega) {
              q.visible = false;
              break;
            }

            float x0 = q.adjusted[i].x;
            float y0 = q.adjusted[i].y;
            float x1 = q.adjusted[pair].x;
            float y1 = q.adjusted[pair].y;

            float delta = (omega - y0) / (y1 - y0);

            q.adjusted[i].x = q.adjusted[i].x +
                              delta * (q.adjusted[pair].x - q.adjusted[i].x);

            q.adjusted[i].y = q.adjusted[i].y +
                              delta * (q.adjusted[pair].y - q.adjusted[i].y);
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

      q.zOrder = 0;
      for (int i = 0; i < 4; i++) {
        q.zOrder += q.adjusted[i].y;
        q.projected[i] = {
            w / 2 - (q.adjusted[i].x * 800) / (q.adjusted[i].y + omega),
            h / 2 + (q.adjusted[i].z * 800) / (q.adjusted[i].y + omega)};
      }
      q.zOrder /= 4;

      if (q.partials.size() > 0) {
        for (auto& p : q.partials) {
          for (int i = 0; i < 4; i++) {
            p.projected[i].x =
                w / 2 - (p.adjusted[i].x * 800) / (p.adjusted[i].y + omega);
            p.projected[i].y =
                h / 2 + (p.adjusted[i].z * 800) / (p.adjusted[i].y + omega);
          }
        }
      }

      if (dotProduct < 0) {
        q.visible = false;
      }
    }

    Clear(olc::BLACK);

    if (!GetKey(olc::Key::TAB).bHeld) {
      sortedQuads.clear();
      for (auto& quad : quads) {
        sortedQuads.push_back(&quad);
      }

      std::sort(sortedQuads.begin(), sortedQuads.end(),
                [](quad* a, quad* b) { return a->zOrder > b->zOrder; });

      for (auto& q : sortedQuads) {
        float dotProduct = q->centre.x * q->normal.x +
                           q->centre.y * q->normal.y +
                           q->centre.z * q->normal.z;

        auto decal = plasmaTexture.decal;
        if (q->wall) {
          decal = wallTexture.decal;
        } else {
          decal = floorTexture.decal;
        }

        if (q->zOrder > -omega && dotProduct > 0) {
          if (q->partials.size() > 0) {
            for (auto& p : q->partials) {
              DrawPartialWarpedDecal(decal, p.projected, p.sourcePos,
                                     p.sourceSize, p.colour);
            }
          }
          if (q->visible) {
            DrawWarpedDecal(decal, q->projected, q->colour);
          }
        }
      }

    } else {
      DrawLine(0, h / 2, w, h / 2, olc::BLUE);
      DrawLine(0, h / 2 - omega, w, h / 2 - omega, olc::YELLOW);

      for (auto q : quads) {
        if (q.partials.size() > 0) {
          for (auto p : q.partials) {
            olc::vf2d pv[4] = {
                {-p.adjusted[0].x + w / 2, -p.adjusted[0].y + h / 2},
                {-p.adjusted[1].x + w / 2, -p.adjusted[1].y + h / 2},
                {-p.adjusted[2].x + w / 2, -p.adjusted[2].y + h / 2},
                {-p.adjusted[3].x + w / 2, -p.adjusted[3].y + h / 2}};

            DrawPartialWarpedDecal(floorTexture.decal, pv, p.sourcePos,
                                   p.sourceSize, p.colour);

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

            DrawWarpedDecal(floorTexture.decal, pv, q.colour);

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
    }

    return true;
  }
};

int main() {
  Olc3d2 demo;
  if (demo.Construct(w, h, 1, 1)) demo.Start();
  return 0;
}