#pragma once
#include "common.h"

enum class Kind { Toggle, Action };
enum class Ctrl { None, Slider, Color, Combo };

struct Target {
  enum Type { Bytes, Value } type = Value;
  uintptr_t addr = 0;
  std::vector<uint8_t> orig, patch;     // Bytes
  std::vector<uint32_t> chain;          // Value: cadeia de ponteiros (opcional)
  std::string vtype = "i32";            // u8 i16 i32 f32 rgb_u8 rgb_f32
  bool hasFixed = false; double fixed = 0;
  std::vector<int> values;              // Combo: indice -> valor gravado
  bool applied = false;
};

struct Feature {
  const char* id = "";
  const char* label = "";
  const char* tab = "";
  const char* tip = "";
  Kind kind = Kind::Toggle;
  Ctrl ctrl = Ctrl::None;
  float minv = 0, maxv = 1, defv = 0;
  const char* fmt = "%.2f";
  std::vector<const char*> options;
  bool needsCode = false;               // exige codigo proprio (ainda nao implementado)
  // estado
  bool on = false, pendingOn = false;
  float value = 0; int sel = 0; float rgb[3] = {1.f, 0.f, 0.f}; int vk = 0;
  std::vector<Target> targets;
  std::string status;
};

extern std::vector<Feature> g_features;
extern bool g_remember; extern int g_menuKey; extern bool g_cfgDirty;
extern std::string g_gameInfo; extern bool g_gameOk;

void Features_Init();
void Features_LoadOffsets();      // le offsets.ini (pode ser chamado de novo)
void Features_Reload();           // desativa, relê offsets.ini e reaplica
bool Features_Usable(const Feature& f);
void Features_Toggle(Feature& f);
void Features_Trigger(Feature& f);
void Features_Tick();
void Features_ApplyPending();
void Features_DisableAll();
void Config_Load();
void Config_Save();
