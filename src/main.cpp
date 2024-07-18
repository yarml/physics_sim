/*
        The fked up physics simulator

        The fked up physics simulator starts paused, use TAB to unpause

        You can't add objects from the ui, you must manually add them from the
   code

        Build:
                1- Install sbs in your system if not already downloaded (I'm
   sorry, I was young when I made this decision) 2- type sbs all-run Controls:
                * Always:
                        CTRL + ESC: Quit
                        R         : Reset
                        TAB		    : Pause/Resume
                        ESC       : Open/Close menu
                * In menu:
                        UP/DOWN: Switch selected object
                        T      : Adds/Removes the selected object from the trace
   list F      : Focus/Remove focus on the selected object UI: You can
   open/close the menu with ESC, every object in the universe is listed in the
   menu You can change the selected object by using keyboard arrows Pressing F
   will move the focus to the selected object if the selected object is already
   focused, then it will focus on nothing Pressing T will enable/disable tracing
   on the selected object

                If the focused object is traced, nothing will happen,
                because the relative movement between the focused object and
   camera is 0

                You can use the mouse to move around, and scroll to zoom in or
   out
*/

#include <limits>
#include <map>
#include <olcPixelGameEngine.hpp>
#include <sstream>

using unit = long double;

// Completely proportionate to real values
constexpr unit SUN_MASS = 100;
constexpr unit EARTH_MASS = 20;
constexpr unit MARS_MASS = 10;
constexpr unit G = 1;

using vec2 = olc::v2d_generic<unit>;

template <typename T> using list = std::vector<T>;

template <typename K, typename V> using map = std::map<K, V>;

typedef std::stringstream sstream;

#define TEXT_SCALE (4)

class physics_object {
public:
  unit mass;
  unit rad;
  vec2 pos;
  vec2 vel;
  olc::Pixel color;
  list<vec2> forces;
  std::string name;

public:
  physics_object(const std::string &pname, unit pmass, unit prad,
                 olc::Pixel pcolor, const vec2 &ipos, const vec2 &ivel)
      : mass(pmass), rad(prad), pos(ipos), color(pcolor), vel(ivel),
        name(pname) {}

public:
  unit distanceto(const physics_object &other) {
    return sqrt(pow(pos.x - other.pos.x, 2) + pow(pos.y - other.pos.y, 2));
  }
  void applyforce(const vec2 &force) { forces.push_back(force); }
  vec2 relpos(const physics_object &other) {
    return vec2(other.pos.x - pos.x, other.pos.y - pos.y);
  }
};

class PhysicsSimulator : public olc::PixelGameEngine {
private:
  list<physics_object *> universe;
  list<physics_object *> attractors;
  list<physics_object *> attracted;
  list<physics_object *> trackedobjs;
  map<physics_object *, list<vec2>> trackedobjs_pos;
  physics_object *focus;
  unit zoom;
  vec2 offset;
  vec2 old_mouse;
  vec2 mouse_mov;
  unit wheeldiv;
  bool menu;
  int selected;
  bool pause;
  unit time_mul;

public:
  PhysicsSimulator() { sAppName = "Physics Simulator"; }
  ~PhysicsSimulator() {
    for (physics_object *po : universe)
      if (po != nullptr)
        delete po;
  }

public:
  physics_object *addto_universe(physics_object *obj, bool attract = true,
                                 bool isattracted = true) {
    universe.push_back(obj);
    if (isattracted)
      attracted.push_back(obj);
    if (attract)
      attractors.push_back(obj);
    return obj;
  }

public:
  bool OnUserCreate() override {
    for (physics_object *o : universe) {
      if (o != nullptr)
        delete o;
    }
    universe.clear();
    attractors.clear();
    attracted.clear();
    trackedobjs.clear();
    trackedobjs_pos.clear();

    physics_object *sun =
        addto_universe(new physics_object("Sun", SUN_MASS, 20.0, olc::YELLOW,
                                          vec2(0.0, 0.0), vec2(0, 0)),
                       true, false);
    physics_object *earth =
        addto_universe(new physics_object("Earth", EARTH_MASS, 5.0, olc::CYAN,
                                          vec2(1000.0, 0.0), vec2(0.0, 0.0)),
                       false, true);
    physics_object *mars = addto_universe(
        new physics_object("Mars", MARS_MASS, 1.2, olc::YELLOW,
                           vec2(223.98 * 1000 / 147.53, 0.0), vec2(0.0, 0.0)),
        false, true);

    earth->vel.y = sqrt(G * SUN_MASS / earth->distanceto(*sun));
    mars->vel.y = sqrt(G * SUN_MASS / mars->distanceto(*sun));

    focus = sun;

    offset = vec2(0.0f, 0.0f);
    zoom = 1.0;
    wheeldiv = 6400;

    menu = false;
    selected = 0;

    pause = true;

    time_mul = 100.0f;

    return true;
  }
  bool OnUserUpdate(float fElapsedTime) override {
    int32_t wheel = GetMouseWheel();
    if (!menu) {
      zoom += (wheel / wheeldiv);
      if (zoom <= 0)
        zoom = 0.001;
    }
    vec2 mouse(GetMouseX(), GetMouseY());
    mouse_mov = mouse - old_mouse;
    old_mouse = mouse;

    if (GetKey(olc::R).bPressed)
      OnUserCreate();

    if (GetKey(olc::ESCAPE).bPressed && universe.size() != 0) {
      if (GetKey(olc::CTRL).bHeld)
        olc_Terminate();
      else
        menu = !menu;
    }
    if (GetMouse(0).bHeld && !menu) {
      offset.x += mouse_mov.x / zoom;
      offset.y -= mouse_mov.y / zoom;
    }
    if (GetKey(olc::TAB).bPressed)
      pause = !pause;
    if (menu) {
      if (GetKey(olc::UP).bPressed) {
        selected--;
        if (selected < 0)
          selected = universe.size() - 1;
      }
      if (GetKey(olc::DOWN).bPressed) {
        selected++;
        if (selected >= universe.size())
          selected = 0;
      }
      if (GetKey(olc::F).bPressed) {
        for (physics_object *obj : trackedobjs)
          trackedobjs_pos[obj].clear();
        if (focus == universe[selected]) {
          focus = nullptr;
          offset = universe[selected]->pos;
        } else {
          focus = universe[selected];
          offset = vec2(0, 0);
        }
      }
      if (GetKey(olc::T).bPressed) {
        list<physics_object *>::iterator it = std::find(
            trackedobjs.begin(), trackedobjs.end(), universe[selected]);
        if (it != trackedobjs.end()) {
          trackedobjs.erase(it);
          trackedobjs_pos.erase(*it);
        } else {
          trackedobjs.push_back(universe[selected]);
          trackedobjs_pos.insert(std::pair<physics_object *, list<vec2>>(
              universe[selected], list<vec2>()));
        }
      }
    }
    if (!pause) {
      for (physics_object *aed : attracted) {
        for (physics_object *aor : attractors) {
          if (aed != aor) {
            unit distance = aed->distanceto(*aor);

            unit magnitude = (G * aed->mass * aor->mass) / pow(distance, 2);

            vec2 relpos = aed->relpos(*aor);
            unit forcex = (relpos.x * magnitude) / distance;
            unit forcey = (relpos.y * magnitude) / distance;

            aed->applyforce(vec2(forcex, forcey));
          }
        }
      }
      for (physics_object *o : universe) {
        for (const vec2 &force : o->forces) {
          o->vel += force / o->mass;
          o->pos += o->vel;
        }
        o->forces.clear();
      }
    }
    Clear(olc::BLACK);
    for (physics_object *o : universe) {
      vec2 relpos;
      if (focus != nullptr)
        relpos = focus->relpos(*o);
      else {
        physics_object fake("Fake", 0, 0, olc::WHITE, vec2(0, 0), vec2(0, 0));
        relpos = fake.relpos(*o);
      }
      FillCircle((relpos.x + offset.x) * zoom + ScreenWidth() / 2,
                 -(relpos.y + offset.y) * zoom + ScreenHeight() / 2,
                 o->rad * zoom, o->color);
      Draw((relpos.x + offset.x) * zoom + ScreenWidth() / 2,
           -(relpos.y + offset.y) * zoom + ScreenHeight() / 2, o->color);
    }
    for (physics_object *tracked : trackedobjs) {
      if (tracked != nullptr) {
        list<vec2> &trackedpos = trackedobjs_pos[tracked];
        if (!pause) {
          physics_object fake("Fake", 0, 0, olc::WHITE, tracked->pos,
                              vec2(0, 0));
          vec2 relpos;
          if (focus != nullptr)
            relpos = focus->relpos(fake);
          else {
            physics_object fake2("Fake", 0, 0, olc::WHITE, vec2(0, 0),
                                 vec2(0, 0));
            relpos = fake2.relpos(fake);
          }
          trackedpos.push_back(relpos);
          if (trackedpos.size() > 16384)
            trackedpos.erase(trackedpos.begin());
        }
        for (const vec2 &p : trackedpos) {
          Draw((p.x + offset.x) * zoom + ScreenWidth() / 2,
               -(p.y + offset.y) * zoom + ScreenHeight() / 2, tracked->color);
        }
      }
    }
    if (menu) {
      int largest_name = 0;
      for (physics_object *o : universe) {
        if (o->name.length() > largest_name &&
            o->name.length() * 8 * TEXT_SCALE <= ScreenWidth() / 2)
          largest_name = o->name.length();
      }
      largest_name += 4; // Padding or " T F"
      FillRect(0, 0, largest_name * 8 * TEXT_SCALE, ScreenHeight(),
               olc::DARK_BLUE);
      FillRect(0, selected * 8 * TEXT_SCALE, largest_name * 8 * TEXT_SCALE,
               8 * TEXT_SCALE, olc::BLUE);
      for (size_t i = 0; i < universe.size(); i++) {
        sstream name_stream;
        name_stream << universe[i]->name;
        if (std::find(trackedobjs.begin(), trackedobjs.end(), universe[i]) !=
            trackedobjs.end())
          name_stream << " T";
        if (focus == universe[i])
          name_stream << " F";
        DrawString(0, i * 8 * TEXT_SCALE, name_stream.str(), olc::WHITE,
                   TEXT_SCALE);
      }
    }
    FillCircle(ScreenWidth() / 2, ScreenHeight() / 2, 1, olc::RED);
    return true;
  }
};

int main() {
  PhysicsSimulator sim;
  if (sim.Construct(720, 480, 1, 1))
    sim.Start();
  return 0;
}