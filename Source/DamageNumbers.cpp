#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <API/ARK/Ark.h>
#include <API/UE/Math/ColorList.h>

#include "MiniJson.h"

#pragma comment(lib, "ArkApi.lib")

namespace DamageNumbers {

constexpr const char* kPluginName = "DamageNumbers";

struct Config {
    bool enabled = true;
    bool show_dealt = true;
    bool show_taken = true;
    bool show_players = true;
    bool show_dinos = true;
    bool show_structures = true;
    bool show_self_damage = false;
    bool show_zero_damage = false;

    float dealt_scale = 1.85f;
    float taken_scale = 1.85f;
    float display_time = 0.85f;
    int min_damage = 1;
    int max_damage = 100000000;

    std::string dealt_color = "Green";
    std::string taken_color = "Red";
    std::string dealt_prefix = "";
    std::string taken_prefix = "";
    bool thousands_separator = true;

    std::string toggle_command = "/damage";
    std::string sender = "DamageNumbers";
    std::string enabled_message = "Damage numbers enabled.";
    std::string disabled_message = "Damage numbers disabled.";
    std::string reload_ok = "DamageNumbers config reloaded.";
};

Config g_config;

DECLARE_HOOK(APrimalCharacter_TakeDamage, float,
    APrimalCharacter*, float, FDamageEvent*, AController*, AActor*);
DECLARE_HOOK(APrimalStructure_TakeDamage, float,
    APrimalStructure*, float, FDamageEvent*, AController*, AActor*);

std::string ConfigPath() {
    return ArkApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/DamageNumbers/config.json";
}

FString F(const std::string& utf8) {
    return FString(ArkApi::Tools::Utf8Decode(utf8).c_str());
}

void SendChat(AShooterPlayerController* pc, const std::string& msg) {
    if (!pc) return;
    const FString sender = F(g_config.sender);
    const FString text = F(msg);
    ArkApi::GetApiUtils().SendChatMessage(pc, sender, *text);
}

FLinearColor ColorByName(const std::string& value) {
    std::string s = value;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (s == "red") return FLinearColor{1.0f, 0.0f, 0.0f, 1.0f};
    if (s == "green") return FLinearColor{0.0f, 1.0f, 0.0f, 1.0f};
    if (s == "blue") return FLinearColor{0.0f, 0.45f, 1.0f, 1.0f};
    if (s == "yellow") return FLinearColor{1.0f, 1.0f, 0.0f, 1.0f};
    if (s == "cyan") return FLinearColor{0.0f, 1.0f, 1.0f, 1.0f};
    if (s == "orange") return FLinearColor{1.0f, 0.5f, 0.0f, 1.0f};
    if (s == "white") return FLinearColor{1.0f, 1.0f, 1.0f, 1.0f};
    return FLinearColor{0.0f, 1.0f, 0.0f, 1.0f};
}

std::string FormatDamage(float damage) {
    long long value = static_cast<long long>(std::llround(std::max(0.0f, damage)));
    std::string s = std::to_string(value);
    if (!g_config.thousands_separator) return s;

    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), ",");
    return s;
}

bool DamageInRange(float damage) {
    const int rounded = static_cast<int>(std::llround(damage));
    if (!g_config.show_zero_damage && rounded <= 0) return false;
    return rounded >= g_config.min_damage && rounded <= g_config.max_damage;
}

AShooterPlayerController* ResolvePlayer(AController* event_instigator, AActor* damage_causer) {
    if (event_instigator &&
        event_instigator->IsA(AShooterPlayerController::GetPrivateStaticClass())) {
        return static_cast<AShooterPlayerController*>(event_instigator);
    }

    if (damage_causer &&
        damage_causer->IsA(AShooterCharacter::GetPrivateStaticClass())) {
        return ArkApi::GetApiUtils().FindControllerFromCharacter(
            static_cast<AShooterCharacter*>(damage_causer));
    }

    return nullptr;
}

AShooterPlayerController* ResolveVictimPlayer(APrimalCharacter* victim) {
    if (!victim || !victim->IsA(AShooterCharacter::GetPrivateStaticClass()))
        return nullptr;

    return ArkApi::GetApiUtils().FindControllerFromCharacter(
        static_cast<AShooterCharacter*>(victim));
}

bool IsPlayerCharacter(APrimalCharacter* actor) {
    return actor && actor->IsA(AShooterCharacter::GetPrivateStaticClass());
}

bool IsDinoCharacter(APrimalCharacter* actor) {
    return actor && actor->IsA(APrimalDinoCharacter::GetPrivateStaticClass());
}

void ShowNumber(AShooterPlayerController* pc, float damage, bool dealt) {
    if (!pc || !DamageInRange(damage)) return;

    const std::string text =
        (dealt ? g_config.dealt_prefix : g_config.taken_prefix) +
        FormatDamage(damage);

    const FString ftext = F(text);
    const FLinearColor color =
        dealt ? ColorByName(g_config.dealt_color) : ColorByName(g_config.taken_color);
    const float scale = dealt ? g_config.dealt_scale : g_config.taken_scale;

    ArkApi::GetApiUtils().SendNotification(
        pc, color, scale, g_config.display_time, nullptr, *ftext);
}

void HandleCharacterDamage(APrimalCharacter* victim,
                           float actual_damage,
                           AController* event_instigator,
                           AActor* damage_causer) {
    if (!g_config.enabled || !victim || !DamageInRange(actual_damage)) return;

    const bool victim_is_player = IsPlayerCharacter(victim);
    const bool victim_is_dino = IsDinoCharacter(victim);

    if (victim_is_player && !g_config.show_players) return;
    if (victim_is_dino && !g_config.show_dinos) return;
    if (!victim_is_player && !victim_is_dino) return;

    AShooterPlayerController* attacker = ResolvePlayer(event_instigator, damage_causer);
    AShooterPlayerController* victim_pc = victim_is_player ? ResolveVictimPlayer(victim) : nullptr;

    if (g_config.show_dealt && attacker) {
        if (g_config.show_self_damage || attacker != victim_pc)
            ShowNumber(attacker, actual_damage, true);
    }

    if (g_config.show_taken && victim_pc) {
        if (g_config.show_self_damage || attacker != victim_pc)
            ShowNumber(victim_pc, actual_damage, false);
    }
}

float Hook_APrimalCharacter_TakeDamage(APrimalCharacter* _this,
                                       float damage,
                                       FDamageEvent* damage_event,
                                       AController* event_instigator,
                                       AActor* damage_causer) {
    const float actual_damage = APrimalCharacter_TakeDamage_original(
        _this, damage, damage_event, event_instigator, damage_causer);

    try {
        HandleCharacterDamage(
            _this, actual_damage, event_instigator, damage_causer);
    } catch (const std::exception& e) {
        Log::GetLog()->error("DamageNumbers character hook: {}", e.what());
    } catch (...) {
        Log::GetLog()->error("DamageNumbers character hook: unknown exception");
    }

    return actual_damage;
}

float Hook_APrimalStructure_TakeDamage(APrimalStructure* _this,
                                       float damage,
                                       FDamageEvent* damage_event,
                                       AController* event_instigator,
                                       AActor* damage_causer) {
    const float actual_damage = APrimalStructure_TakeDamage_original(
        _this, damage, damage_event, event_instigator, damage_causer);

    try {
        if (g_config.enabled && g_config.show_structures &&
            DamageInRange(actual_damage)) {
            AShooterPlayerController* attacker =
                ResolvePlayer(event_instigator, damage_causer);
            if (g_config.show_dealt && attacker)
                ShowNumber(attacker, actual_damage, true);
        }
    } catch (const std::exception& e) {
        Log::GetLog()->error("DamageNumbers structure hook: {}", e.what());
    } catch (...) {
        Log::GetLog()->error("DamageNumbers structure hook: unknown exception");
    }

    return actual_damage;
}

Config ParseConfig(const minijson::Value& root) {
    Config c;

    c.enabled = minijson::boolean(root, "General", "Enabled", c.enabled);
    c.show_dealt = minijson::boolean(root, "General", "ShowDealtDamage", c.show_dealt);
    c.show_taken = minijson::boolean(root, "General", "ShowTakenDamage", c.show_taken);
    c.show_players = minijson::boolean(root, "General", "ShowPlayers", c.show_players);
    c.show_dinos = minijson::boolean(root, "General", "ShowDinos", c.show_dinos);
    c.show_structures = minijson::boolean(root, "General", "ShowStructures", c.show_structures);
    c.show_self_damage = minijson::boolean(root, "General", "ShowSelfDamage", c.show_self_damage);
    c.show_zero_damage = minijson::boolean(root, "General", "ShowZeroDamage", c.show_zero_damage);

    c.dealt_scale = minijson::number(root, "Display", "DealtScale", c.dealt_scale);
    c.taken_scale = minijson::number(root, "Display", "TakenScale", c.taken_scale);
    c.display_time = minijson::number(root, "Display", "DisplayTime", c.display_time);
    c.min_damage = minijson::integer(root, "Display", "MinDamage", c.min_damage);
    c.max_damage = minijson::integer(root, "Display", "MaxDamage", c.max_damage);
    c.dealt_color = minijson::str(root, "Display", "DealtColor", c.dealt_color);
    c.taken_color = minijson::str(root, "Display", "TakenColor", c.taken_color);
    c.dealt_prefix = minijson::str(root, "Display", "DealtPrefix", c.dealt_prefix);
    c.taken_prefix = minijson::str(root, "Display", "TakenPrefix", c.taken_prefix);
    c.thousands_separator = minijson::boolean(
        root, "Display", "ThousandsSeparator", c.thousands_separator);

    c.toggle_command = minijson::str(root, "Commands", "ToggleCommand", c.toggle_command);

    c.sender = minijson::str(root, "Messages", "Sender", c.sender);
    c.enabled_message = minijson::str(root, "Messages", "Enabled", c.enabled_message);
    c.disabled_message = minijson::str(root, "Messages", "Disabled", c.disabled_message);
    c.reload_ok = minijson::str(root, "Messages", "ReloadOk", c.reload_ok);

    c.dealt_scale = std::clamp(c.dealt_scale, 0.1f, 5.0f);
    c.taken_scale = std::clamp(c.taken_scale, 0.1f, 5.0f);
    c.display_time = std::clamp(c.display_time, 0.05f, 10.0f);
    c.min_damage = std::max(0, c.min_damage);
    c.max_damage = std::max(c.min_damage, c.max_damage);

    return c;
}

void ReadConfig() {
    std::ifstream file(ConfigPath(), std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Can't open " + ConfigPath());

    std::ostringstream ss;
    ss << file.rdbuf();

    const minijson::Value root = minijson::parse(ss.str());
    if (!root.is_object())
        throw std::runtime_error("config root must be a JSON object");

    g_config = ParseConfig(root);
}

void ToggleCommand(AShooterPlayerController* pc,
                   FString* message,
                   EChatSendMode::Type) {
    if (!pc || !message) return;

    std::istringstream stream(message->ToString());
    std::string command;
    std::string action;
    stream >> command >> action;

    std::transform(action.begin(), action.end(), action.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (action == "on") {
        g_config.enabled = true;
        SendChat(pc, g_config.enabled_message);
    } else if (action == "off") {
        g_config.enabled = false;
        SendChat(pc, g_config.disabled_message);
    } else {
        SendChat(pc, std::string("Usage: ") + g_config.toggle_command + " on|off");
    }
}

void ReloadCommand(APlayerController* player_controller, FString*, bool) {
    auto* pc = player_controller &&
        player_controller->IsA(AShooterPlayerController::GetPrivateStaticClass())
        ? static_cast<AShooterPlayerController*>(player_controller)
        : nullptr;

    try {
        ReadConfig();
        if (pc)
            ArkApi::GetApiUtils().SendServerMessage(
                pc, FColorList::Green, g_config.reload_ok.c_str());
        Log::GetLog()->info("DamageNumbers config reloaded");
    } catch (const std::exception& e) {
        if (pc)
            ArkApi::GetApiUtils().SendServerMessage(
                pc, FColorList::Red, e.what());
        Log::GetLog()->error("DamageNumbers reload failed: {}", e.what());
    }
}

void Load() {
    Log::Get().Init(kPluginName);
    ReadConfig();

    ArkApi::GetHooks().SetHook(
        "APrimalCharacter.TakeDamage",
        &Hook_APrimalCharacter_TakeDamage,
        &APrimalCharacter_TakeDamage_original);

    ArkApi::GetHooks().SetHook(
        "APrimalStructure.TakeDamage",
        &Hook_APrimalStructure_TakeDamage,
        &APrimalStructure_TakeDamage_original);

    ArkApi::GetCommands().AddChatCommand(F(g_config.toggle_command), &ToggleCommand);
    ArkApi::GetCommands().AddConsoleCommand("DamageNumbers.Reload", &ReloadCommand);

    Log::GetLog()->info("Loaded plugin - DamageNumbers v1.0");
}

void Unload() {
    ArkApi::GetCommands().RemoveChatCommand(F(g_config.toggle_command));
    ArkApi::GetCommands().RemoveConsoleCommand("DamageNumbers.Reload");

    ArkApi::GetHooks().DisableHook(
        "APrimalCharacter.TakeDamage",
        &Hook_APrimalCharacter_TakeDamage);

    ArkApi::GetHooks().DisableHook(
        "APrimalStructure.TakeDamage",
        &Hook_APrimalStructure_TakeDamage);
}

} // namespace DamageNumbers

extern "C" __declspec(dllexport) void __fastcall Plugin_Init() noexcept {
    try {
        DamageNumbers::Load();
    } catch (const std::exception& e) {
        Log::Get().Init("DamageNumbers");
        Log::GetLog()->error("DamageNumbers failed to initialize: {}", e.what());
    } catch (...) {
        Log::Get().Init("DamageNumbers");
        Log::GetLog()->error("DamageNumbers failed to initialize");
    }
}

extern "C" __declspec(dllexport) void __fastcall Plugin_Unload() noexcept {
    try {
        DamageNumbers::Unload();
    } catch (const std::exception& e) {
        Log::GetLog()->error("DamageNumbers unload error: {}", e.what());
    } catch (...) {
        Log::GetLog()->error("DamageNumbers unload unknown error");
    }
}
