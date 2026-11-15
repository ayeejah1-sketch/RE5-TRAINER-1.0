#include "ui.h"
#include "features.h"
#include "imgui.h"
#include <cstring>

bool  g_binding = false;
float g_menuX = 80.f, g_menuY = 80.f;

static int g_tab = 0;
static Feature* g_bindFeat = nullptr;
static bool g_bindMenu = false;

static const char* kTabs[] = {"Jogador", "Personagem", "Inimigos", "Gameplay", "Câmera e HUD", "Laser", "Config"};
static const int kTabCount = 7;
static const float kW = 720.f, kH = 480.f, kHead = 52.f, kSide = 168.f;
static const ImVec4 ACC(0.88f, 0.20f, 0.25f, 1.f);

void UI_Style() {
  ImGuiStyle& s = ImGui::GetStyle();
  s.WindowRounding = 14; s.ChildRounding = 10; s.FrameRounding = 7; s.GrabRounding = 7;
  s.PopupRounding = 8; s.ScrollbarRounding = 8; s.WindowBorderSize = 0; s.ChildBorderSize = 0;
  s.FrameBorderSize = 0; s.PopupBorderSize = 0;
  s.ItemSpacing = ImVec2(10, 10); s.FramePadding = ImVec2(10, 6); s.ScrollbarSize = 8;
  ImVec4* c = s.Colors;
  c[ImGuiCol_WindowBg]       = ImVec4(0.055f, 0.060f, 0.075f, 0.94f);
  c[ImGuiCol_ChildBg]        = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  c[ImGuiCol_PopupBg]        = ImVec4(0.09f, 0.095f, 0.115f, 0.98f);
  c[ImGuiCol_Text]           = ImVec4(0.92f, 0.93f, 0.96f, 1.f);
  c[ImGuiCol_TextDisabled]   = ImVec4(0.50f, 0.52f, 0.58f, 1.f);
  c[ImGuiCol_FrameBg]        = ImVec4(0.13f, 0.14f, 0.17f, 1.f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.18f, 0.22f, 1.f);
  c[ImGuiCol_FrameBgActive]  = ImVec4(0.20f, 0.21f, 0.26f, 1.f);
  c[ImGuiCol_SliderGrab]     = ACC;
  c[ImGuiCol_SliderGrabActive] = ImVec4(1.f, 0.35f, 0.38f, 1.f);
  c[ImGuiCol_CheckMark]      = ACC;
  c[ImGuiCol_Button]         = ImVec4(0.16f, 0.17f, 0.21f, 1.f);
  c[ImGuiCol_ButtonHovered]  = ImVec4(0.88f, 0.20f, 0.25f, 0.55f);
  c[ImGuiCol_ButtonActive]   = ImVec4(0.88f, 0.20f, 0.25f, 0.85f);
  c[ImGuiCol_Header]         = ImVec4(0.88f, 0.20f, 0.25f, 0.30f);
  c[ImGuiCol_HeaderHovered]  = ImVec4(0.88f, 0.20f, 0.25f, 0.50f);
  c[ImGuiCol_HeaderActive]   = ImVec4(0.88f, 0.20f, 0.25f, 0.70f);
  c[ImGuiCol_ScrollbarBg]    = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ScrollbarGrab]  = ImVec4(0.30f, 0.31f, 0.36f, 1.f);
  c[ImGuiCol_Separator]      = ImVec4(1.f, 1.f, 1.f, 0.07f);
}

static std::string KeyName(int vk) {
  if (vk <= 0) return "Nenhuma";
  if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
  char b[64] = {};
  LONG lp = (LONG)MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) << 16;
  if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN || vk == VK_INSERT ||
      vk == VK_DELETE || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT) lp |= (1 << 24);
  if (GetKeyNameTextA(lp, b, sizeof b) > 0) return b;
  char h[16]; snprintf(h, sizeof h, "0x%02X", vk); return h;
}

// Captura a proxima tecla pressionada (mouse e Shift/Ctrl/Alt genericos sao ignorados).
static void CaptureKey() {
  for (int vk = 8; vk < 255; ++vk) {
    if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;
    if (!(GetAsyncKeyState(vk) & 0x8000)) continue;
    if (vk != VK_ESCAPE) {
      if (g_bindMenu) g_menuKey = vk;
      else if (g_bindFeat) g_bindFeat->vk = vk;
      g_cfgDirty = true;
    }
    g_bindFeat = nullptr; g_bindMenu = false;
    return;
  }
}

static bool Switch(const char* id, bool on, bool enabled) {
  const float h = 22.f, w = 42.f;
  ImVec2 p = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, ImVec2(w, h));
  bool clicked = enabled && ImGui::IsItemClicked();
  ImU32 bg = !enabled ? IM_COL32(45, 47, 55, 255) : on ? ImGui::GetColorU32(ACC) : IM_COL32(64, 67, 80, 255);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, h * 0.5f);
  dl->AddCircleFilled(ImVec2(p.x + (on ? w - h * 0.5f : h * 0.5f), p.y + h * 0.5f), h * 0.5f - 3.f,
                      enabled ? IM_COL32(245, 245, 248, 255) : IM_COL32(120, 122, 130, 255));
  return clicked;
}

static void DrawFeature(Feature& f) {
  const bool usable = Features_Usable(f);
  ImGui::PushID(f.id);
  const float x0 = ImGui::GetCursorPosX();
  const float w = ImGui::GetContentRegionAvail().x;

  ImGui::AlignTextToFramePadding();
  ImGui::PushStyleColor(ImGuiCol_Text, usable ? ImVec4(0.92f, 0.93f, 0.96f, 1.f) : ImVec4(0.50f, 0.52f, 0.58f, 1.f));
  ImGui::TextUnformatted(f.label);
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered() && f.tip && f.tip[0]) ImGui::SetTooltip("%s", f.tip);
  if (!usable) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.25f, 1.f), " %s", f.status.c_str()); }
  else if (f.vk) { ImGui::SameLine(); ImGui::TextDisabled("[%s]", KeyName(f.vk).c_str()); }

  ImGui::SameLine();
  if (f.kind == Kind::Toggle) {
    ImGui::SetCursorPosX(x0 + w - 42.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.f);
    if (Switch("##sw", f.on, usable)) Features_Toggle(f);
  } else {
    ImGui::SetCursorPosX(x0 + w - 90.f);
    ImGui::BeginDisabled(!usable);
    if (ImGui::Button("Executar", ImVec2(90, 0))) Features_Trigger(f);
    ImGui::EndDisabled();
  }

  if (f.ctrl != Ctrl::None) {
    ImGui::BeginDisabled(!usable);
    if (f.ctrl == Ctrl::Slider) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::SliderFloat("##v", &f.value, f.minv, f.maxv, f.fmt)) g_cfgDirty = true;
    } else if (f.ctrl == Ctrl::Combo && !f.options.empty()) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::BeginCombo("##c", f.options[f.sel])) {
        for (int i = 0; i < (int)f.options.size(); ++i)
          if (ImGui::Selectable(f.options[i], f.sel == i)) { f.sel = i; g_cfgDirty = true; }
        ImGui::EndCombo();
      }
    } else if (f.ctrl == Ctrl::Color) {
      ImGui::SetNextItemWidth(240);
      if (ImGui::ColorPicker3("##col", f.rgb,
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_DisplayRGB |
            ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_InputRGB)) g_cfgDirty = true;
    }
    ImGui::EndDisabled();
  }
  ImGui::Separator();
  ImGui::PopID();
}

static void DrawConfig() {
  ImGui::TextColored(g_gameOk ? ImVec4(0.45f, 0.85f, 0.5f, 1.f) : ImVec4(0.95f, 0.4f, 0.4f, 1.f), "%s", g_gameInfo.c_str());
  int ok = 0, total = 0;
  for (auto& f : g_features) { ++total; if (Features_Usable(f)) ++ok; }
  ImGui::TextDisabled("Funções calibradas: %d de %d (offsets.ini)", ok, total);
  ImGui::Separator();

  ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Tecla do menu");
  ImGui::SameLine(160);
  if (ImGui::Button(g_bindMenu ? "Pressione uma tecla..." : KeyName(g_menuKey).c_str(), ImVec2(170, 0))) {
    g_bindMenu = true; g_bindFeat = nullptr;
  }
  if (ImGui::Checkbox("Lembrar opções ativadas ao abrir o jogo", &g_remember)) g_cfgDirty = true;

  if (ImGui::Button("Salvar", ImVec2(110, 0))) Config_Save();
  ImGui::SameLine();
  if (ImGui::Button("Carregar", ImVec2(110, 0))) { Features_DisableAll(); Config_Load(); Features_ApplyPending(); }
  ImGui::SameLine();
  if (ImGui::Button("Recarregar offsets", ImVec2(150, 0))) Features_Reload();
  ImGui::SameLine();
  if (ImGui::Button("Desativar tudo", ImVec2(130, 0))) Features_DisableAll();
  ImGui::Separator();

  ImGui::TextColored(ACC, "Atalhos das funções");
  for (auto& f : g_features) {
    if (f.needsCode) continue;
    ImGui::PushID(f.id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(f.label);
    ImGui::SameLine(300);
    bool me = (g_bindFeat == &f);
    if (ImGui::Button(me ? "Pressione..." : KeyName(f.vk).c_str(), ImVec2(120, 0))) { g_bindFeat = &f; g_bindMenu = false; }
    ImGui::SameLine();
    if (ImGui::Button("Limpar", ImVec2(70, 0))) { f.vk = 0; g_cfgDirty = true; }
    ImGui::PopID();
  }
}

void UI_Draw() {
  ImGuiIO& io = ImGui::GetIO();
  g_binding = (g_bindFeat != nullptr) || g_bindMenu;
  if (g_binding) CaptureKey();

  ImGui::SetNextWindowPos(ImVec2(g_menuX, g_menuY), ImGuiCond_Once);
  ImGui::SetNextWindowSize(ImVec2(kW, kH));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("##re5trainer", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();

  ImVec2 wp = ImGui::GetWindowPos();
  g_menuX = wp.x; g_menuY = wp.y;
  ImDrawList* dl = ImGui::GetWindowDrawList();

  // cabecalho (arraste por aqui)
  ImGui::SetCursorPos(ImVec2(0, 0));
  ImGui::InvisibleButton("##drag", ImVec2(kW, kHead));
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) {
    float nx = wp.x + io.MouseDelta.x, ny = wp.y + io.MouseDelta.y;
    if (nx < -kW + 80) nx = -kW + 80; if (nx > io.DisplaySize.x - 80) nx = io.DisplaySize.x - 80;
    if (ny < 0) ny = 0;               if (ny > io.DisplaySize.y - 40) ny = io.DisplaySize.y - 40;
    ImGui::SetWindowPos(ImVec2(nx, ny));
  }
  dl->AddText(ImVec2(wp.x + 22, wp.y + 17), ImGui::GetColorU32(ACC), "RE5");
  dl->AddText(ImVec2(wp.x + 22 + ImGui::CalcTextSize("RE5 ").x, wp.y + 17), IM_COL32(235, 236, 242, 255), "ULTIMATE TRAINER");
  char hint[64]; snprintf(hint, sizeof hint, "%s para fechar", KeyName(g_menuKey).c_str());
  ImVec2 hs = ImGui::CalcTextSize(hint);
  dl->AddText(ImVec2(wp.x + kW - hs.x - 22, wp.y + 17), IM_COL32(130, 133, 145, 255), hint);
  dl->AddLine(ImVec2(wp.x + 16, wp.y + kHead), ImVec2(wp.x + kW - 16, wp.y + kHead), IM_COL32(255, 255, 255, 18));

  // barra lateral
  ImGui::SetCursorPos(ImVec2(12, kHead + 8));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
  ImGui::BeginChild("##side", ImVec2(kSide - 12, kH - kHead - 20), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
  for (int i = 0; i < kTabCount; ++i) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 sz(ImGui::GetContentRegionAvail().x, 38.f);
    if (ImGui::InvisibleButton(kTabs[i], sz)) g_tab = i;
    bool sel = (g_tab == i), hov = ImGui::IsItemHovered();
    ImDrawList* cdl = ImGui::GetWindowDrawList();
    ImU32 bg = sel ? IM_COL32(224, 51, 64, 46) : hov ? IM_COL32(255, 255, 255, 13) : 0;
    if (bg) cdl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), bg, 8.f);
    if (sel) cdl->AddRectFilled(ImVec2(p.x, p.y + 8), ImVec2(p.x + 3, p.y + sz.y - 8), ImGui::GetColorU32(ACC), 2.f);
    cdl->AddText(ImVec2(p.x + 16, p.y + 10), sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(150, 153, 165, 255), kTabs[i]);
  }
  ImGui::EndChild();
  ImGui::PopStyleVar(2);

  // conteudo
  ImGui::SetCursorPos(ImVec2(kSide + 4, kHead + 8));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 8));
  ImGui::BeginChild("##content", ImVec2(kW - kSide - 16, kH - kHead - 20), ImGuiChildFlags_None, 0);
  ImGui::PopStyleVar();
  ImGui::TextColored(ACC, "%s", kTabs[g_tab]);
  if (!g_gameOk) ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.4f, 1.f), "Versão do jogo diferente da calibrada: funções bloqueadas.");
  ImGui::Separator();
  if (g_tab == kTabCount - 1) DrawConfig();
  else for (auto& f : g_features) if (!strcmp(f.tab, kTabs[g_tab])) DrawFeature(f);
  ImGui::EndChild();

  ImGui::End();
}
