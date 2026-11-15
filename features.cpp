#include "features.h"
#include "ui.h"
#include <cstdlib>
#include <cstring>

std::vector<Feature> g_features;
bool g_remember = true;
int  g_menuKey = VK_F8;
bool g_cfgDirty = false;
std::string g_gameInfo = "Jogo não identificado";
bool g_gameOk = true;
static uint32_t g_ts = 0;

// ---------------------------------------------------------------- memoria
static bool ReadMem(uintptr_t a, void* dst, size_t n) {
  __try { memcpy(dst, (const void*)a, n); return true; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool WriteRaw(uintptr_t a, const void* src, size_t n) {
  __try { memcpy((void*)a, src, n); return true; }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool WriteMem(uintptr_t a, const void* src, size_t n) {
  DWORD old = 0;
  if (!VirtualProtect((void*)a, n, PAGE_EXECUTE_READWRITE, &old)) return false;
  bool ok = WriteRaw(a, src, n);
  DWORD tmp;
  VirtualProtect((void*)a, n, old, &tmp);
  FlushInstructionCache(GetCurrentProcess(), (void*)a, n);
  return ok;
}

static bool Resolve(const Target& t, uintptr_t& out) {
  if (t.chain.empty()) { out = t.addr; return true; }
  uintptr_t cur = 0;
  if (!ReadMem(t.addr, &cur, 4)) return false;
  for (size_t i = 0; i < t.chain.size(); ++i) {
    if (cur < 0x10000) return false;
    uintptr_t a = cur + t.chain[i];
    if (i + 1 == t.chain.size()) { out = a; return true; }
    if (!ReadMem(a, &cur, 4)) return false;
  }
  return false;
}

static bool WriteValue(const Target& t, const Feature& f, uintptr_t a) {
  double v = t.hasFixed ? t.fixed : (double)f.value;
  if (f.ctrl == Ctrl::Combo) {
    v = (f.sel >= 0 && f.sel < (int)t.values.size()) ? t.values[f.sel] : f.sel;
  }
  const std::string& vt = t.vtype;
  if (vt == "u8")  { uint8_t x = (uint8_t)(int)v;  return WriteRaw(a, &x, 1); }
  if (vt == "i16") { int16_t x = (int16_t)(int)v;  return WriteRaw(a, &x, 2); }
  if (vt == "i32") { int32_t x = (int32_t)v;       return WriteRaw(a, &x, 4); }
  if (vt == "f32") { float x = (float)v;           return WriteRaw(a, &x, 4); }
  if (vt == "rgb_u8")  { uint8_t x[3] = {(uint8_t)(f.rgb[0]*255.f+.5f),(uint8_t)(f.rgb[1]*255.f+.5f),(uint8_t)(f.rgb[2]*255.f+.5f)}; return WriteRaw(a, x, 3); }
  if (vt == "rgb_f32") { float x[3] = {f.rgb[0], f.rgb[1], f.rgb[2]}; return WriteRaw(a, x, 12); }
  return false;
}

// ---------------------------------------------------------------- tabela de funcoes
static void Add(const char* id, const char* label, const char* tab, Kind k, Ctrl c,
                float mn, float mx, float df, const char* tip,
                bool code = false, std::vector<const char*> opts = {}, const char* fmt = "%.2f") {
  Feature f;
  f.id = id; f.label = label; f.tab = tab; f.kind = k; f.ctrl = c;
  f.minv = mn; f.maxv = mx; f.defv = df; f.value = df; f.tip = tip;
  f.needsCode = code; f.options = opts; f.fmt = fmt;
  g_features.push_back(f);
}

void Features_Init() {
  g_features.clear();
  const Kind T = Kind::Toggle, A = Kind::Action;
  const Ctrl N = Ctrl::None, S = Ctrl::Slider, C = Ctrl::Color, M = Ctrl::Combo;
  const char* J = "Jogador"; const char* P = "Personagem"; const char* I = "Inimigos";
  const char* G = "Gameplay"; const char* H = "Câmera e HUD"; const char* L = "Laser";
  std::vector<const char*> chars = {"Chris","Sheva","Jill","Wesker","Josh","Excella","Irving","Fong"};

  Add("god","Invencibilidade",J,T,N,0,1,0,"O jogador não sofre dano.");
  Add("hp_inf","Vida infinita",J,T,N,0,1,0,"Mantém a vida sempre cheia.");
  Add("heal","Recuperar vida",J,A,N,0,1,0,"Restaura a vida uma vez.");
  Add("ammo_inf","Munição infinita",J,T,N,0,1,0,"Munição não diminui.");
  Add("no_reload","Sem recarga",J,T,N,0,1,0,"Não precisa recarregar.");
  Add("items_inf","Granadas e itens infinitos",J,T,N,0,1,0,"Itens de uso não são consumidos.");
  Add("money","Dinheiro infinito",J,T,N,0,1,0,"Mantém o dinheiro no máximo.");
  Add("timer_freeze","Congelar temporizador",J,T,N,0,1,0,"Congela temporizadores do jogo.");
  Add("speed","Velocidade do personagem",J,T,S,0.5f,5.f,1.f,"Multiplicador de velocidade.");
  Add("size","Tamanho do personagem",J,T,S,0.5f,3.f,1.f,"Escala do personagem.");

  Add("char_p1","Personagem - Jogador 1",P,A,M,0,0,0,"Troca o personagem do jogador 1.",false,chars);
  Add("char_p2","Personagem - Jogador 2",P,A,M,0,0,0,"Troca o personagem do jogador 2.",false,chars);
  Add("costume_p1","Traje - Jogador 1",P,A,S,0,9,0,"Índice do traje.",false,{},"%.0f");
  Add("costume_p2","Traje - Jogador 2",P,A,S,0,9,0,"Índice do traje.",false,{},"%.0f");
  Add("char_stats","Características dos personagens",P,T,N,0,1,0,"Em desenvolvimento.",true);

  Add("kill_all","Matar todos os inimigos",I,A,N,0,1,0,"Em desenvolvimento.",true);
  Add("en_freeze","Congelar inimigos",I,T,N,0,1,0,"Inimigos param de agir.");
  Add("en_hp","Alterar vida dos inimigos",I,A,S,1,10000,100,"Em desenvolvimento.",true,{},"%.0f");
  Add("en_hp_mult","Multiplicar vida dos inimigos",I,T,S,0.1f,10.f,1.f,"Em desenvolvimento.",true);
  Add("en_speed","Velocidade dos inimigos",I,T,S,0.1f,3.f,1.f,"Multiplicador de velocidade.");
  Add("en_tp","Teleportar inimigos até o jogador",I,A,N,0,1,0,"Em desenvolvimento.",true);

  Add("no_collision","No Collision",G,T,N,0,1,0,"Atravessa objetos e inimigos.");
  Add("inf_dash","Infinite Dash",G,T,N,0,1,0,"Corrida sem limite.");
  Add("dodge","Habilitar esquiva",G,T,N,0,1,0,"Libera a esquiva.");
  Add("melee","Movimentos corpo a corpo",G,T,N,0,1,0,"Modifica o ataque corpo a corpo.");
  Add("mission_timer","Congelar tempo da missão",G,T,N,0,1,0,"Congela o cronômetro da missão.");

  Add("hide_hud","Ocultar HUD",H,T,N,0,1,0,"Esconde toda a interface.");
  Add("hide_map","Ocultar mapa",H,T,N,0,1,0,"Esconde o mapa.");
  Add("hide_inv","Ocultar inventário",H,T,N,0,1,0,"Esconde o inventário.");
  Add("hide_cross","Ocultar mira",H,T,N,0,1,0,"Esconde a mira.");

  Add("laser_off","Desabilitar laser",L,T,N,0,1,0,"Desliga o laser da arma.");
  Add("laser_color","Cor personalizada do laser",L,T,C,0,1,0,"Escolha qualquer cor (RGB ou HEX).");
  for (auto& f : g_features) if (!strcmp(f.id, "laser_color")) { f.rgb[0]=1.f; f.rgb[1]=0.f; f.rgb[2]=0.f; }
}

bool Features_Usable(const Feature& f) { return g_gameOk && !f.needsCode && !f.targets.empty(); }

// ---------------------------------------------------------------- offsets.ini
static std::vector<uint8_t> ParseBytes(const char* s) {
  std::vector<uint8_t> v;
  while (*s) {
    while (*s == ' ') ++s;
    if (!*s) break;
    char* e; unsigned long x = strtoul(s, &e, 16);
    if (e == s) break;
    v.push_back((uint8_t)x); s = e;
  }
  return v;
}
static std::vector<uint32_t> ParseList(const char* s, int base) {
  std::vector<uint32_t> v;
  while (*s) {
    while (*s == ' ' || *s == ',') ++s;
    if (!*s) break;
    char* e; unsigned long x = strtoul(s, &e, base);
    if (e == s) break;
    v.push_back((uint32_t)x); s = e;
  }
  return v;
}
static void ReadGameInfo() {
  HMODULE m = GetModuleHandleA(nullptr);
  auto dos = (PIMAGE_DOS_HEADER)m;
  auto nt = (PIMAGE_NT_HEADERS)((BYTE*)m + dos->e_lfanew);
  g_ts = nt->FileHeader.TimeDateStamp;
  char b[128];
  snprintf(b, sizeof b, "Executável: TS 0x%08X / Tamanho 0x%X", g_ts, (unsigned)nt->OptionalHeader.SizeOfImage);
  g_gameInfo = b;
}

void Features_LoadOffsets() {
  ReadGameInfo();
  std::string path = g_dir + "offsets.ini";
  char buf[1024];

  g_gameOk = true;
  GetPrivateProfileStringA("meta", "timestamp", "", buf, sizeof buf, path.c_str());
  if (buf[0]) {
    uint32_t want = (uint32_t)strtoul(buf, nullptr, 0);
    if (want != g_ts) {
      g_gameOk = false;
      g_gameInfo += "  -  VERSÃO DO JOGO DIFERENTE DA CALIBRADA";
      Log("timestamp diferente: ini=0x%08X jogo=0x%08X", want, g_ts);
    } else g_gameInfo += "  -  versão confere";
  }

  int calibrated = 0;
  for (auto& f : g_features) {
    f.targets.clear();
    for (int i = 1; i <= 16; ++i) {
      std::string sec = (i == 1) ? f.id : std::string(f.id) + "." + std::to_string(i);
      GetPrivateProfileStringA(sec.c_str(), "type", "", buf, sizeof buf, path.c_str());
      if (!buf[0]) break;
      Target t;
      std::string ty = buf;
      t.type = (ty == "bytes") ? Target::Bytes : Target::Value;
      GetPrivateProfileStringA(sec.c_str(), "addr", "0", buf, sizeof buf, path.c_str());
      t.addr = (uintptr_t)strtoul(buf, nullptr, 0);
      if (t.addr < 0x400000 || t.addr > 0x7FFE0000) { Log("[%s] addr invalido", sec.c_str()); continue; }
      if (t.type == Target::Bytes) {
        GetPrivateProfileStringA(sec.c_str(), "orig", "", buf, sizeof buf, path.c_str());  t.orig = ParseBytes(buf);
        GetPrivateProfileStringA(sec.c_str(), "patch", "", buf, sizeof buf, path.c_str()); t.patch = ParseBytes(buf);
        if (t.orig.empty() || t.orig.size() != t.patch.size()) { Log("[%s] orig/patch invalidos", sec.c_str()); continue; }
      } else {
        GetPrivateProfileStringA(sec.c_str(), "chain", "", buf, sizeof buf, path.c_str()); t.chain = ParseList(buf, 16);
        GetPrivateProfileStringA(sec.c_str(), "vtype", "i32", buf, sizeof buf, path.c_str()); t.vtype = buf;
        GetPrivateProfileStringA(sec.c_str(), "value", "", buf, sizeof buf, path.c_str());
        if (buf[0]) { t.hasFixed = true; t.fixed = atof(buf); }
        GetPrivateProfileStringA(sec.c_str(), "values", "", buf, sizeof buf, path.c_str());
        for (uint32_t v : ParseList(buf, 10)) t.values.push_back((int)v);
      }
      f.targets.push_back(t);
    }
    if (f.needsCode) f.status = "(em desenvolvimento)";
    else if (f.targets.empty()) f.status = "(não calibrado)";
    else { f.status = "OK"; ++calibrated; }
    if (!g_gameOk && !f.targets.empty()) f.status = "(versão incompatível)";
  }
  Log("offsets.ini: %d funcoes calibradas", calibrated);
}

// ---------------------------------------------------------------- ativar / desativar
static bool ApplyBytes(Target& t) {
  size_t n = t.patch.size();
  std::vector<uint8_t> cur(n);
  if (!ReadMem(t.addr, cur.data(), n)) return false;
  if (cur == t.patch) { t.applied = true; return true; }
  if (cur != t.orig) return false;            // bytes nao conferem: nao mexe
  if (!WriteMem(t.addr, t.patch.data(), n)) return false;
  t.applied = true; return true;
}
static void RestoreBytes(Target& t) {
  if (t.applied) { WriteMem(t.addr, t.orig.data(), t.orig.size()); t.applied = false; }
}
static void SetOn(Feature& f, bool on) {
  if (on) {
    for (auto& t : f.targets) {
      if (t.type != Target::Bytes) continue;
      if (!ApplyBytes(t)) {
        for (auto& r : f.targets) if (r.type == Target::Bytes) RestoreBytes(r);
        f.on = false; f.status = "(bytes não conferem)";
        Log("[%s] bytes nao conferem em 0x%08X", f.id, (unsigned)t.addr);
        return;
      }
    }
  } else {
    for (auto& t : f.targets) if (t.type == Target::Bytes) RestoreBytes(t);
  }
  f.on = on; g_cfgDirty = true;
}

void Features_Toggle(Feature& f) {
  if (f.kind != Kind::Toggle || !Features_Usable(f)) return;
  SetOn(f, !f.on);
}
void Features_Trigger(Feature& f) {
  if (!Features_Usable(f)) return;
  for (auto& t : f.targets) {
    if (t.type != Target::Value) continue;
    uintptr_t a;
    if (Resolve(t, a)) WriteValue(t, f, a);
  }
}
void Features_Tick() {
  if (!g_gameOk) return;
  for (auto& f : g_features) {
    if (!f.on) continue;
    for (auto& t : f.targets) {
      if (t.type != Target::Value) continue;
      uintptr_t a;
      if (Resolve(t, a)) WriteValue(t, f, a);
    }
  }
}
void Features_ApplyPending() {
  for (auto& f : g_features)
    if (f.pendingOn) { f.pendingOn = false; if (Features_Usable(f) && !f.on) SetOn(f, true); }
}
void Features_DisableAll() {
  for (auto& f : g_features) if (f.on) SetOn(f, false);
}
void Features_Reload() {
  std::vector<bool> was;
  for (auto& f : g_features) { was.push_back(f.on); if (f.on) SetOn(f, false); }
  Features_LoadOffsets();
  for (size_t i = 0; i < g_features.size(); ++i) g_features[i].pendingOn = was[i];
  Features_ApplyPending();
}

// ---------------------------------------------------------------- configuracao
static std::string CfgPath() { return g_dir + "RE5Trainer.cfg"; }

void Config_Save() {
  std::string p = CfgPath(); const char* c = p.c_str(); char b[64];
  auto W = [&](const char* sec, const std::string& k, const std::string& v) { WritePrivateProfileStringA(sec, k.c_str(), v.c_str(), c); };
  W("General", "menuKey", std::to_string(g_menuKey));
  W("General", "remember", g_remember ? "1" : "0");
  W("General", "menuX", std::to_string((int)g_menuX));
  W("General", "menuY", std::to_string((int)g_menuY));
  for (auto& f : g_features) {
    std::string k = f.id;
    W("Features", k + ".on", (g_remember && f.on) ? "1" : "0");
    snprintf(b, sizeof b, "%.4f", f.value); W("Features", k + ".val", b);
    W("Features", k + ".sel", std::to_string(f.sel));
    W("Features", k + ".vk", std::to_string(f.vk));
    snprintf(b, sizeof b, "%.4f", f.rgb[0]); W("Features", k + ".r", b);
    snprintf(b, sizeof b, "%.4f", f.rgb[1]); W("Features", k + ".g", b);
    snprintf(b, sizeof b, "%.4f", f.rgb[2]); W("Features", k + ".b", b);
  }
  g_cfgDirty = false;
}

void Config_Load() {
  std::string p = CfgPath(); const char* c = p.c_str();
  if (GetFileAttributesA(c) == INVALID_FILE_ATTRIBUTES) return;
  char b[64];
  g_menuKey = GetPrivateProfileIntA("General", "menuKey", VK_F8, c);
  g_remember = GetPrivateProfileIntA("General", "remember", 1, c) != 0;
  g_menuX = (float)(int)GetPrivateProfileIntA("General", "menuX", 80, c);
  g_menuY = (float)(int)GetPrivateProfileIntA("General", "menuY", 80, c);
  for (auto& f : g_features) {
    std::string k = f.id;
    f.pendingOn = g_remember && GetPrivateProfileIntA("Features", (k + ".on").c_str(), 0, c) != 0;
    GetPrivateProfileStringA("Features", (k + ".val").c_str(), "", b, sizeof b, c); if (b[0]) f.value = (float)atof(b);
    f.sel = GetPrivateProfileIntA("Features", (k + ".sel").c_str(), 0, c);
    if (f.sel < 0 || (!f.options.empty() && f.sel >= (int)f.options.size())) f.sel = 0;
    f.vk = GetPrivateProfileIntA("Features", (k + ".vk").c_str(), 0, c);
    GetPrivateProfileStringA("Features", (k + ".r").c_str(), "", b, sizeof b, c); if (b[0]) f.rgb[0] = (float)atof(b);
    GetPrivateProfileStringA("Features", (k + ".g").c_str(), "", b, sizeof b, c); if (b[0]) f.rgb[1] = (float)atof(b);
    GetPrivateProfileStringA("Features", (k + ".b").c_str(), "", b, sizeof b, c); if (b[0]) f.rgb[2] = (float)atof(b);
  }
}
