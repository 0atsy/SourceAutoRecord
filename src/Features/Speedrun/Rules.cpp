#include "Rules.hpp"
#include "Categories.hpp"
#include "Modules/Console.hpp"
#include "Modules/Engine.hpp"
#include "Modules/Server.hpp"
#include "Features/EntityList.hpp"
#include "Features/Session.hpp"

#define TAU 6.28318530718

#ifdef _WIN32
#    define PLAT_CALL(fn, ...) fn(__VA_ARGS__)
#else
#    define PLAT_CALL(fn, ...) fn(nullptr, __VA_ARGS__)
#endif

static int g_overlayId = 100;

template<typename V>
static inline V *lookupMap(std::map<std::string, V> &m, std::string k)
{
    auto search = m.find(k);
    if (search == m.end()) {
        return nullptr;
    }
    return &search->second;
}


bool EntityInputRule::Test(std::string targetname, std::string classname, std::string inputname, std::string parameter)
{
    if ((this->typeMask & ENTRULE_TARGETNAME) && targetname != this->targetname) return false;
    if ((this->typeMask & ENTRULE_CLASSNAME) && classname != this->classname) return false;
    if (inputname != this->inputname) return false;
    if ((this->typeMask & ENTRULE_PARAMETER) && parameter != this->parameter) return false;
    return true;
}

std::optional<SpeedrunRule> EntityInputRule::Create(std::map<std::string, std::string> params)
{
    std::string *targetname = lookupMap(params, "targetname");
    std::string *classname = lookupMap(params, "classname");
    std::string *inputname = lookupMap(params, "inputname");
    std::string *parameter = lookupMap(params, "parameter");

    EntityInputRule rule;

    if (targetname) {
        rule.targetname = *targetname;
        rule.typeMask |= ENTRULE_TARGETNAME;
    }

    if (classname) {
        rule.classname = *classname;
        rule.typeMask |= ENTRULE_CLASSNAME;
    }

    if (!inputname) {
        console->Print("inputname not specified\n");
        return {};
    }

    rule.inputname = *inputname;

    if (parameter) {
        rule.parameter = *parameter;
        rule.typeMask |= ENTRULE_PARAMETER;
    }

    return SpeedrunRule(RuleAction::START, "", rule);
}

static bool pointInBox(Vector point, Vector center, Vector size, double rotation)
{
    // Recenter point
    point -= center;

    // Rotate point
    // We rotate it by -rotation to simulate rotating the collision box
    // by rotation
    double s = sin(-rotation);
    double c = cos(-rotation);
    double x = point.x;
    double y = point.y;
    point.x = x * c - y * s;
    point.y = x * s + y * c;

    // Is final point within the bounding box?
    bool inX = abs(point.x) < size.x / 2;
    bool inY = abs(point.y) < size.y / 2;
    bool inZ = abs(point.z) < size.z / 2;

    return inX && inY && inZ;
}

bool ZoneTriggerRule::Test(Vector pos)
{
    return pointInBox(pos, this->center, this->size, this->rotation);
}

void ZoneTriggerRule::DrawInWorld(float time)
{
    PLAT_CALL(
        engine->AddBoxOverlay,
        this->center,
        -this->size / 2,
        this->size / 2,
        { 0, (float)(this->rotation * 360.0f / TAU), 0 },
        140, 6, 195, 100,
        time
    );
}

void ZoneTriggerRule::OverlayInfo(HudContext *ctx, SpeedrunRule *rule)
{
    Vector screenPos;
    engine->PointToScreen(this->center, screenPos);
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "type: zone");
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "center: %.2f, %.2f, %.2f", this->center.x, this->center.y, this->center.z);
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "size: %.2f, %.2f, %.2f", this->size.x, this->size.y, this->size.z);
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "angle: %.2f", this->rotation * 360.0f / TAU);
    if (rule->slot) {
        ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "player: %d", *rule->slot);
    }
    if (rule->onlyAfter) {
        ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "after: %s", rule->onlyAfter->c_str());
    }
}

std::optional<SpeedrunRule> ZoneTriggerRule::Create(std::map<std::string, std::string> params)
{
    std::string *posStr = lookupMap(params, "pos");
    std::string *sizeStr = lookupMap(params, "size");
    std::string *angleStr = lookupMap(params, "angle");

    if (!posStr || !sizeStr || !angleStr) {
        console->Print("pos, size, and angle must all be specified\n");
        return {};
    }

    Vector pos;
    Vector size;
    double angle;
    sscanf(posStr->c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
    sscanf(sizeStr->c_str(), "%f,%f,%f", &size.x, &size.y, &size.z);
    sscanf(angleStr->c_str(), "%lf", &angle);

    ZoneTriggerRule rule{
        pos,
        size,
        angle / 360.0f * TAU,
        g_overlayId++,
    };

    if (g_overlayId > 255) {
        g_overlayId = 100;
    }

    return SpeedrunRule(RuleAction::START, "", rule);
}

bool PortalPlacementRule::Test(Vector pos, PortalColor portal)
{
    if (this->portal && portal != this->portal) return false;
    return pointInBox(pos, this->center, this->size, this->rotation);
}

void PortalPlacementRule::DrawInWorld(float time)
{
    int r = 255, g = 0, b = 0;

    if (this->portal) {
        switch (*this->portal) {
        case PortalColor::BLUE:
            r = 0;
            g = 28;
            b = 188;
            break;
        case PortalColor::ORANGE:
            r = 218;
            g = 64;
            b = 3;
            break;
        }
    }

    PLAT_CALL(
        engine->AddBoxOverlay,
        this->center,
        -this->size / 2,
        this->size / 2,
        { 0, (float)(this->rotation * 360.0f / TAU), 0 },
        r, g, b, 100,
        time
    );
}

void PortalPlacementRule::OverlayInfo(HudContext *ctx, SpeedrunRule *rule)
{
    Vector screenPos;
    engine->PointToScreen(this->center, screenPos);
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "type: portal");
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "center: %.2f, %.2f, %.2f", this->center.x, this->center.y, this->center.z);
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "size: %.2f, %.2f, %.2f", this->size.x, this->size.y, this->size.z);
    ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "angle: %.2f", this->rotation * 360.0f / TAU);
    if (rule->slot) {
        ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "player: %d", *rule->slot);
    }
    if (this->portal) {
        ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "portal: %s", *this->portal == PortalColor::BLUE ? "blue (primary)" : "orange (secondary)");
    }
    if (rule->onlyAfter) {
        ctx->DrawElementOnScreen(this->overlayId, screenPos.x, screenPos.y, "after: %s", rule->onlyAfter->c_str());
    }
}

std::optional<SpeedrunRule> PortalPlacementRule::Create(std::map<std::string, std::string> params)
{
    std::string *posStr = lookupMap(params, "pos");
    std::string *sizeStr = lookupMap(params, "size");
    std::string *angleStr = lookupMap(params, "angle");
    std::string *portalStr = lookupMap(params, "portal");

    if (!posStr || !sizeStr || !angleStr) {
        console->Print("pos, size, and angle must all be specified\n");
        return {};
    }

    Vector pos;
    Vector size;
    double angle;
    sscanf(posStr->c_str(), "%f,%f,%f", &pos.x, &pos.y, &pos.z);
    sscanf(sizeStr->c_str(), "%f,%f,%f", &size.x, &size.y, &size.z);
    sscanf(angleStr->c_str(), "%lf", &angle);

    std::optional<PortalColor> portal;

    if (portalStr) {
        if (*portalStr == "blue") {
            portal = PortalColor::BLUE;
        } else if (*portalStr == "orange") {
            portal = PortalColor::ORANGE;
        } else {
            console->Print("Invalid portal color '%s'\n", portalStr->c_str());
            return {};
        }
    }

    PortalPlacementRule rule{
        pos,
        size,
        angle / 360.0f * TAU,
        portal,
        g_overlayId++,
    };

    if (g_overlayId > 255) {
        g_overlayId = 100;
    }


    return SpeedrunRule(RuleAction::START, "", rule);
}

std::optional<SpeedrunRule> ChallengeFlagsRule::Create(std::map<std::string, std::string> params)
{
    if (!lookupMap(params, "player")) {
        console->Print("player must be specified\n");
        return {};
    }

    return SpeedrunRule(RuleAction::START, "", ChallengeFlagsRule{});
}

bool ChallengeFlagsRule::Test()
{
    return true;
}

bool SpeedrunRule::TestGeneral(std::optional<int> slot)
{
    if (this->fired) return false;
    if (this->onlyAfter) {
        auto prereq = SpeedrunTimer::GetRule(*this->onlyAfter);
        if (!prereq || !prereq->fired) return false;
    }
    if (engine->GetCurrentMapName() != this->map) return false;
    if (this->slot) {
        if (this->slot != slot) return false;
    }
    return true;
}

// Describe {{{

static const char *printRuleAction(RuleAction action)
{
    switch (action) {
    case RuleAction::START:
        return "start";
    case RuleAction::FORCE_START:
        return "force-start";
    case RuleAction::STOP:
        return "stop";
    case RuleAction::SPLIT:
        return "split";
    case RuleAction::PAUSE:
        return "pause";
    case RuleAction::RESUME:
        return "resume";
    }
    return "(unknown)";
}

std::string SpeedrunRule::Describe()
{
    std::string s = std::string("action='") + printRuleAction(this->action) + "'";
    s += " map='" + this->map + "'";
    if (this->onlyAfter) {
        s += " after='" + *this->onlyAfter + "'";
    }
    if (this->slot) {
        s += " player='" + std::to_string(*this->slot) + "'";
    }

    switch (this->rule.index()) {
    case 0: { // EntityInputRule
        s = std::string("[entity] ") + s;
        EntityInputRule entRule = std::get<EntityInputRule>(this->rule);
        if (entRule.typeMask & ENTRULE_TARGETNAME) {
            s += " targetname='" + entRule.targetname + "'";
        }
        if (entRule.typeMask & ENTRULE_CLASSNAME) {
            s += " classname='" + entRule.classname + "'";
        }
        s += " inputname='" + entRule.inputname + "'";
        if (entRule.typeMask & ENTRULE_PARAMETER) {
            s += " parameter='" + entRule.parameter + "'";
        }
        break;
    }

    case 1: { // ZoneTriggerRule
        s = std::string("[zone] ") + s;
        ZoneTriggerRule zoneRule = std::get<ZoneTriggerRule>(this->rule);
        char buf[128];
        snprintf(buf, sizeof buf, " pos='%f,%f,%f'", zoneRule.center.x, zoneRule.center.y, zoneRule.center.z);
        s += buf;
        snprintf(buf, sizeof buf, " size='%f,%f,%f'", zoneRule.size.x, zoneRule.size.y, zoneRule.size.z);
        s += buf;
        s += std::string(" angle='") + std::to_string(zoneRule.rotation) + "'";
        break;
    }

    case 2: { // PortalPlacementRule
        s = std::string("[portal] ") + s;
        PortalPlacementRule portalRule = std::get<PortalPlacementRule>(this->rule);
        char buf[128];
        snprintf(buf, sizeof buf, " pos='%f,%f,%f'", portalRule.center.x, portalRule.center.y, portalRule.center.z);
        s += buf;
        snprintf(buf, sizeof buf, " size='%f,%f,%f'", portalRule.size.x, portalRule.size.y, portalRule.size.z);
        s += buf;
        s += std::string(" angle='") + std::to_string(portalRule.rotation) + "'";
        if (portalRule.portal) {
            s += " portal='";
            s += *portalRule.portal == PortalColor::BLUE ? "blue" : "orange";
            s += "'";
        }
        break;
    }

    case 3: { // ChallengeFlagsRule
        s = std::string("[flags] ") + s;
        break;
    }
    }

    return s;
}

// }}}

void SpeedrunTimer::TickRules()
{
    const int MAX_SPLITSCREEN = 2; // HACK: we can't use MAX_SPLITSCREEN_PLAYERS since it's not a compile-time constant

    static std::optional<Vector> portalPositions[MAX_SPLITSCREEN][2];

    for (int slot = 0; slot < MAX_SPLITSCREEN; ++slot) {
        void *player = server->GetPlayer(slot + 1);
        if (!player) {
            portalPositions[slot][0] = {};
            portalPositions[slot][1] = {};
            continue;
        }

        SpeedrunTimer::TestZoneRules(server->GetAbsOrigin(player), slot);

        auto m_hActiveWeapon = *(CBaseHandle *)((uintptr_t)player + Offsets::m_hActiveWeapon);
        auto portalGun = entityList->LookupEntity(m_hActiveWeapon);

        if (!portalGun) {
            portalPositions[slot][0] = {};
            portalPositions[slot][1] = {};
            continue;
        }

        auto m_hPrimaryPortal = *(CBaseHandle *)((uintptr_t)portalGun + Offsets::m_hPrimaryPortal);
        auto m_hSecondaryPortal = *(CBaseHandle *)((uintptr_t)portalGun + Offsets::m_hSecondaryPortal);

        auto bluePortal = entityList->LookupEntity(m_hPrimaryPortal);
        auto orangePortal = entityList->LookupEntity(m_hSecondaryPortal);

        for (int i = 0; i < 2; ++i) {
            auto portal = i == 0 ? bluePortal : orangePortal;
            if (!portal) {
                portalPositions[slot][i] = {};
                continue;
            }

            bool m_bActivated = *(bool *)((uintptr_t)portal + Offsets::m_bActivated);
            if (!m_bActivated) {
                portalPositions[slot][i] = {};
                continue;
            }

            Vector pos = server->GetAbsOrigin(portal);
            if (pos != portalPositions[slot][i]) {
                // Portal position changed
                SpeedrunTimer::TestPortalRules(pos, slot, i ? PortalColor::ORANGE : PortalColor::BLUE);
                portalPositions[slot][i] = pos;
                if (engine->demorecorder->isRecordingDemo) {
                    // Record in demo
                    char data[15];
                    data[0] = 0x05;
                    data[1] = slot;
                    data[2] = i;
                    *(float *)(data + 3) = pos.x;
                    *(float *)(data + 7) = pos.y;
                    *(float *)(data + 11) = pos.z;
                    engine->demorecorder->RecordData(data, sizeof data);
                }
            }
        }
    }

    static bool hadBlueCompleted = false;
    static bool hadOrangeCompleted = false;
    static int prevFlagCount = 0;

    bool blueCompleted = false;
    bool orangeCompleted = false;

    int nflags = 0;

    for (int i = 0; i < Offsets::NUM_ENT_ENTRIES; ++i) {
        auto info = entityList->GetEntityInfoByIndex(i);
        if (!info->m_pEntity) continue;

        auto classname = server->GetEntityClassName(info->m_pEntity);
        if (!classname || strcmp(classname, "challenge_mode_end_node")) continue;

        ++nflags;

#ifdef _WIN32
        uintptr_t blueStatus = (uintptr_t)info->m_pEntity + 0x4B0;
        uintptr_t orangeStatus = (uintptr_t)info->m_pEntity + 0x4B1;
#else
        uintptr_t blueStatus = (uintptr_t)info->m_pEntity + 0x4C8;
        uintptr_t orangeStatus = (uintptr_t)info->m_pEntity + 0x4C9;
#endif

        if (*(uint8_t *)blueStatus) {
            blueCompleted = true;
        }

        if (*(uint8_t *)orangeStatus) {
            orangeCompleted = true;
        }
    }

    if (session->isRunning && session->GetTick() > 1 && server->GetChallengeStatus() != CMStatus::NONE && nflags < prevFlagCount) {
        // We're in an active CM session, and some flags have gone, so
        // the map must have been completed
        blueCompleted = true;
        orangeCompleted = engine->IsCoop();
    }

    bool newBlue = blueCompleted && !hadBlueCompleted;
    bool newOrange = orangeCompleted && !hadOrangeCompleted;

    hadBlueCompleted = blueCompleted;
    hadOrangeCompleted = orangeCompleted;
    prevFlagCount = nflags;

    if (newBlue) {
        if (engine->demorecorder->isRecordingDemo) {
            // Record in demo
            char data[2] = { 0x06, 0 };
            engine->demorecorder->RecordData(data, sizeof data);
        }
        SpeedrunTimer::TestFlagRules(0);
    }

    if (newOrange) {
        if (engine->demorecorder->isRecordingDemo) {
            // Record in demo
            char data[2] = { 0x06, 1 };
            engine->demorecorder->RecordData(data, sizeof data);
        }
        SpeedrunTimer::TestFlagRules(1);
    }
}
