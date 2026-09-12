// LSDCXX_WeaponWheelVC — GTA Vice City weapon wheel ASI (Plugin-SDK)

#include <plugin.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

#include <CFont.h>
#include <CHud.h>
#include <CCamera.h>
#include <CMenuManager.h>
#include <CMouseControllerState.h>
#include <CPad.h>
#include <CPed.h>
#include <CPlayerInfo.h>
#include <CPlayerPed.h>
#include <CSprite2d.h>
#include <CTimer.h>
#include <CTxdStore.h>
#include <CWeapon.h>
#include <eWeaponType.h>
#include <common.h>
#include <RenderWare.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#include "../third_party/stb/stb_image.h"

#include "../third_party/IniReader/IniReader.h"

using namespace plugin;

// ---------------------------------------------------------------------------
// Settings & Constants
// ---------------------------------------------------------------------------
struct Settings {
    int primaryKey = 'Q';
    int secondaryKey = 0;
    int enableSlowMotion = 1;
    float slowMotionSpeed = 0.15f;
    int invertMouseVertical = 0;
    int skipEmptySlots = 1;
    int allowInVehicle = 0;
    int showAmmo = 1;
    int showWeaponName = 1;
    int enableBackgroundBlur = 1;
    float wheelRadius = 280.0f;
    float wheelInnerRadius = 110.0f;
    float selectDeadzone = 30.0f;
};

static Settings gSettings;

static bool gWheelOpen = false;
static bool gWheelHeld = false;
static float gAimX = 0.0f;
static float gAimY = 0.0f;
static int gSelectedSlot = 0;
static int gOpenSlot = 0;
static float gSavedTimeScale = 1.0f;
static bool gSlowMotionActive = false;
static float gSavedMouseAccelH = 0.0f;
static float gSavedMouseAccelV = 0.0f;
static bool gMouseAccelLocked = false;

static void LoadWeaponIcons();
static void FreeWeaponIcons();

static constexpr int MAX_SLOTS = 10;
static constexpr int MAX_WEAPON_TYPE = 36;

struct SlotInfo {
    int slot;
    eWeaponType type;
    unsigned int ammo;
    bool hasWeapon;
};

// Own icon sprites loaded from PNGs
static CSprite2d gWeaponIcons[MAX_WEAPON_TYPE + 1];
static bool gIconsTried = false;
static bool gIconsReady = false;

// 动态武器名称数组
static std::wstring gWeaponNames[MAX_WEAPON_TYPE + 1];

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool IsGameplayAllowed()
{
    if (FrontEndMenuManager.m_bMenuActive)
        return false;
    if (CTimer::m_UserPause || CTimer::m_CodePause)
        return false;
    return true;
}

static bool IsPlayerValidForWheel()
{
    CPlayerPed* player = FindPlayerPed();
    if (!player)
        return false;
    if (player->m_ePedState == PEDSTATE_DIE || player->m_ePedState == PEDSTATE_DEAD)
        return false;
    if (player->m_fHealth <= 0.0f)
        return false;
    if (FindPlayerVehicle() && !gSettings.allowInVehicle)
        return false;
    return true;
}

static const wchar_t* GetWeaponDisplayName(eWeaponType type)
{
    if (static_cast<unsigned int>(type) > MAX_WEAPON_TYPE)
        return L"";
    return gWeaponNames[type].c_str();
}

static void CollectSlots(CPlayerPed* player, std::vector<SlotInfo>& out)
{
    out.clear();
    const int currentSlot = player->m_nCurrentWeapon;

    for (int slot = 0; slot < MAX_SLOTS; ++slot) {
        CWeapon& weapon = player->m_aWeapons[slot];
        eWeaponType type = weapon.m_eWeaponType;
        const unsigned int ammo = weapon.m_nAmmoTotal;
        const bool isUnarmedSlot = (slot == 0);
        const bool realWeapon = (type != WEAPONTYPE_UNARMED && type <= WEAPONTYPE_ANYWEAPON)
            || ammo > 0
            || slot == currentSlot;

        if (!isUnarmedSlot && !realWeapon && gSettings.skipEmptySlots)
            continue;

        SlotInfo info{};
        info.slot = slot;
        info.type = (static_cast<unsigned int>(type) <= MAX_WEAPON_TYPE) ? type : WEAPONTYPE_UNARMED;
        info.ammo = ammo;
        info.hasWeapon = isUnarmedSlot || realWeapon;
        out.push_back(info);
    }

    if (out.empty()) {
        SlotInfo fist{};
        fist.slot = 0;
        fist.type = WEAPONTYPE_UNARMED;
        fist.hasWeapon = true;
        out.push_back(fist);
    }
}

static int SlotFromAngle(float angleDeg, int count)
{
    float a = angleDeg;
    while (a < 0.0f) a += 360.0f;
    while (a >= 360.0f) a -= 360.0f;
    int idx = static_cast<int>(a / (360.0f / count));
    if (idx >= count) idx = count - 1;
    if (idx < 0) idx = 0;
    return idx;
}

static void ApplySelectedWeapon()
{
    CPlayerPed* player = FindPlayerPed();
    if (!player)
        return;

    std::vector<SlotInfo> slots;
    CollectSlots(player, slots);
    if (gSelectedSlot < 0 || gSelectedSlot >= static_cast<int>(slots.size()))
        return;

    int targetSlot = slots[gSelectedSlot].slot;
    if (player->m_nCurrentWeapon != targetSlot) {
        player->SetCurrentWeapon(targetSlot);
        player->MakeChangesForNewWeapon(targetSlot);
    }
}

static void StealMouseForAim()
{
    if (!gWheelOpen)
        return;

    auto& mouse = CPad::NewMouseControllerState;
    float mx = mouse.x;
    float my = -mouse.y;
    if (gSettings.invertMouseVertical)
        my = -my;

    gAimX += mx;
    gAimY += my;

    float len = std::sqrt(gAimX * gAimX + gAimY * gAimY);
    const float maxLen = gSettings.wheelRadius * 1.15f;
    if (len > maxLen && len > 0.001f) {
        gAimX = (gAimX / len) * maxLen;
        gAimY = (gAimY / len) * maxLen;
    }

    mouse.x = 0.0f;
    mouse.y = 0.0f;
    CPad::PCTempMouseControllerState.x = 0.0f;
    CPad::PCTempMouseControllerState.y = 0.0f;
}

static void LockCameraMouse()
{
    if (!gMouseAccelLocked) {
        gSavedMouseAccelH = CCamera::m_fMouseAccelHorzntal;
        gSavedMouseAccelV = CCamera::m_fMouseAccelVertical;
        gMouseAccelLocked = true;
    }
    CCamera::m_fMouseAccelHorzntal = 0.0f;
    CCamera::m_fMouseAccelVertical = 0.0f;
}

static void UnlockCameraMouse()
{
    if (!gMouseAccelLocked)
        return;
    CCamera::m_fMouseAccelHorzntal = gSavedMouseAccelH;
    CCamera::m_fMouseAccelVertical = gSavedMouseAccelV;
    gMouseAccelLocked = false;
}

static void OpenWheel()
{
    if (gWheelOpen)
        return;
    CPlayerPed* player = FindPlayerPed();
    if (!player)
        return;

    gWheelOpen = true;
    gOpenSlot = player->m_nSelectedWepSlot;
    if (gOpenSlot >= MAX_SLOTS)
        gOpenSlot = player->m_nCurrentWeapon;
    gAimX = 0.0f;
    gAimY = 0.0f;

    if (!gIconsTried)
        LoadWeaponIcons();

    {
        std::vector<SlotInfo> slots;
        CollectSlots(player, slots);
        gSelectedSlot = 0;
        for (size_t i = 0; i < slots.size(); ++i) {
            if (slots[i].slot == gOpenSlot) {
                gSelectedSlot = static_cast<int>(i);
                break;
            }
        }
    }

    if (gSettings.enableSlowMotion && !gSlowMotionActive) {
        gSavedTimeScale = CTimer::ms_fTimeScale;
        CTimer::ms_fTimeScale = gSettings.slowMotionSpeed;
        gSlowMotionActive = true;
    }

    LockCameraMouse();

    RECT rc;
    HWND hwnd = GetActiveWindow();
    if (hwnd && GetClientRect(hwnd, &rc)) {
        POINT center{ (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
        ClientToScreen(hwnd, &center);
        SetCursorPos(center.x, center.y);
    }
}

static void CloseWheel(bool applySelection)
{
    if (!gWheelOpen)
        return;

    gWheelOpen = false;
    UnlockCameraMouse();

    if (gSlowMotionActive) {
        CTimer::ms_fTimeScale = gSavedTimeScale;
        gSlowMotionActive = false;
    }

    if (applySelection)
        ApplySelectedWeapon();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static bool IsPrimaryKeyDown()
{
    if (gSettings.primaryKey > 0 && (GetAsyncKeyState(gSettings.primaryKey) & 0x8000))
        return true;
    if (gSettings.secondaryKey > 0 && (GetAsyncKeyState(gSettings.secondaryKey) & 0x8000))
        return true;
    return false;
}

static void UpdateSelection()
{
    CPlayerPed* player = FindPlayerPed();
    if (!player)
        return;

    std::vector<SlotInfo> slots;
    CollectSlots(player, slots);

    float len = std::sqrt(gAimX * gAimX + gAimY * gAimY);
    if (len < gSettings.selectDeadzone) {
        for (size_t i = 0; i < slots.size(); ++i) {
            if (slots[i].slot == gOpenSlot) {
                gSelectedSlot = static_cast<int>(i);
                return;
            }
        }
        gSelectedSlot = 0;
        return;
    }

    float angleDeg = std::atan2(gAimX, -gAimY) * (180.0f / 3.14159265f);
    gSelectedSlot = SlotFromAngle(angleDeg, static_cast<int>(slots.size()));
}

// ---------------------------------------------------------------------------
// Camera lock
// ---------------------------------------------------------------------------
static int __fastcall LookAroundLeftRightHook(CPad* _this, void* /*edx*/)
{
    if (gWheelOpen)
        return 0;
    return _this->LookAroundLeftRight();
}

static int __fastcall LookAroundUpDownHook(CPad* _this, void* /*edx*/)
{
    if (gWheelOpen)
        return 0;
    return _this->LookAroundUpDown();
}

static void __cdecl UpdateMouseHook()
{
    plugin::CallDynGlobal(ADDRESS_BY_VERSION(0x4AD820, 0x4AD840, 0x4AD6F0));
    if (gWheelOpen) {
        StealMouseForAim();
        LockCameraMouse();
    }
}

static void InstallHooks()
{
    patch::RedirectCall(ADDRESS_BY_VERSION(0x4AB6CA, 0x4AB6EA, 0x4AB59A), UpdateMouseHook);

    static const uintptr_t kLookLR_10[] = { 0x4711F2, 0x47C062, 0x481F35, 0x482935 };
    static const uintptr_t kLookUD_10[] = { 0x471206, 0x47C082, 0x481F55, 0x482955 };
    static const uintptr_t kLookLR_11[] = { 0x4711F2, 0x47C062, 0x481F35, 0x482935 };
    static const uintptr_t kLookUD_11[] = { 0x471206, 0x47C082, 0x481F55, 0x482955 };
    static const uintptr_t kLookLR_ST[] = { 0x4700E7, 0x47BE8F, 0x481AB3, 0x48267A };
    static const uintptr_t kLookUD_ST[] = { 0x4700F1, 0x47BE99, 0x481ABD, 0x482684 };

    const uintptr_t* lr = kLookLR_10;
    const uintptr_t* ud = kLookUD_10;
    if (GetGameVersion() == GAME_11EN) {
        lr = kLookLR_11;
        ud = kLookUD_11;
    }
    else if (GetGameVersion() == GAME_STEAM) {
        lr = kLookLR_ST;
        ud = kLookUD_ST;
    }

    for (int i = 0; i < 4; ++i) {
        patch::RedirectCall(lr[i], LookAroundLeftRightHook);
        patch::RedirectCall(ud[i], LookAroundUpDownHook);
    }
}

static void SuppressCombatInput()
{
    if (!gWheelOpen)
        return;

    auto& st = Pads[0].NewState;
    st.ButtonCircle = 0;
    st.ButtonCross = 0;
    st.ButtonSquare = 0;
    st.ButtonTriangle = 0;
    st.LeftShoulder1 = 0;
    st.RightShoulder1 = 0;
    st.DPadLeft = 0;
    st.DPadRight = 0;

    CPad::NewMouseControllerState.lmb = 0;
    CPad::NewMouseControllerState.rmb = 0;
    CPad::NewMouseControllerState.x = 0;
    CPad::NewMouseControllerState.y = 0;
}

// ---------------------------------------------------------------------------
// Texture loading & cleanup
// ---------------------------------------------------------------------------
static RwTexture* CreateTextureFromPng(const char* path)
{
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load(path, &w, &h, &comp, 4);
    if (!data || w <= 0 || h <= 0) {
        if (data)
            stbi_image_free(data);
        return nullptr;
    }

    RwRaster* raster = RwRasterCreate(w, h, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    if (!raster) {
        stbi_image_free(data);
        return nullptr;
    }

    RwUInt8* pixels = RwRasterLock(raster, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
    if (!pixels) {
        RwRasterDestroy(raster);
        stbi_image_free(data);
        return nullptr;
    }

    const int stride = RwRasterGetStride(raster);
    for (int y = 0; y < h; ++y) {
        unsigned char* dst = pixels + y * stride;
        const unsigned char* src = data + y * w * 4;
        for (int x = 0; x < w; ++x) {
            dst[x * 4 + 0] = src[x * 4 + 2]; // B
            dst[x * 4 + 1] = src[x * 4 + 1]; // G
            dst[x * 4 + 2] = src[x * 4 + 0]; // R
            dst[x * 4 + 3] = src[x * 4 + 3]; // A
        }
    }
    RwRasterUnlock(raster);
    stbi_image_free(data);

    RwTexture* tex = RwTextureCreate(raster);
    if (tex) {
        RwTextureSetFilterMode(tex, rwFILTERNEAREST);
        RwTextureSetAddressing(tex, rwTEXTUREADDRESSCLAMP);
    }
    return tex;
}

static void LoadWeaponIcons()
{
    if (gIconsReady || gIconsTried)
        return;
    gIconsTried = true;

    // 获取当前 ASI 模块所在的绝对目录
    char asiDir[MAX_PATH] = {};
    HMODULE hModule = NULL;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&LoadWeaponIcons),
        &hModule
    );
    GetModuleFileNameA(hModule, asiDir, MAX_PATH);
    char* slash = strrchr(asiDir, '\\');
    if (slash)
        *slash = '\0';

    int found = 0;
    for (int i = 0; i <= MAX_WEAPON_TYPE; ++i) {
        char path[MAX_PATH] = {};
        sprintf_s(path, "%s\\weapvww\\w%02d.png", asiDir, i);

        RwTexture* tex = CreateTextureFromPng(path);
        if (tex) {
            gWeaponIcons[i].m_pTexture = tex;
            ++found;
        }
    }

    gIconsReady = found > 0;
}

static void FreeWeaponIcons()
{
    for (int i = 0; i <= MAX_WEAPON_TYPE; ++i) {
        if (gWeaponIcons[i].m_pTexture) {
            RwTextureDestroy(gWeaponIcons[i].m_pTexture);
            gWeaponIcons[i].m_pTexture = nullptr;
        }
    }
    gIconsReady = false;
    gIconsTried = false;
}

static CSprite2d* GetWeaponSprite(eWeaponType type)
{
    if (static_cast<unsigned int>(type) > MAX_WEAPON_TYPE)
        return nullptr;

    if (!gIconsTried && gWheelOpen)
        LoadWeaponIcons();

    if (gWeaponIcons[type].m_pTexture)
        return &gWeaponIcons[type];
    return nullptr;
}

// ---------------------------------------------------------------------------
// RenderWare Native 2D Primitive Drawing
// ---------------------------------------------------------------------------
static inline void Set2DVertex(RwIm2DVertex& v, float x, float y, CRGBA col)
{
    RwIm2DVertexSetScreenX(&v, x);
    RwIm2DVertexSetScreenY(&v, y);
    RwIm2DVertexSetScreenZ(&v, 0.0f);
    RwIm2DVertexSetRecipCameraZ(&v, 1.0f);
    RwIm2DVertexSetIntRGBA(&v, col.r, col.g, col.b, col.a);
}

static void DrawRwCircleFan(float cx, float cy, float radius, int steps, CRGBA color)
{
    if (steps < 3 || steps > 120) return;
    std::vector<RwIm2DVertex> verts(steps + 2);

    Set2DVertex(verts[0], cx, cy, color);
    float stepRad = (2.0f * 3.14159265f) / float(steps);

    for (int i = 0; i <= steps; ++i) {
        float a = float(i) * stepRad;
        Set2DVertex(verts[i + 1], cx + radius * std::cos(a), cy + radius * std::sin(a), color);
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)nullptr);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);

    RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, verts.data(), static_cast<RwInt32>(verts.size()));
}

static void DrawRwRingSector(float cx, float cy, float r0, float r1,
    float a0Deg, float a1Deg, int steps, CRGBA color)
{
    if (steps < 2) steps = 2;
    int vertCount = (steps + 1) * 2;
    std::vector<RwIm2DVertex> verts(vertCount);

    float stepDeg = (a1Deg - a0Deg) / float(steps);

    for (int i = 0; i <= steps; ++i) {
        float deg = a0Deg + float(i) * stepDeg;
        float rad = deg * (3.14159265f / 180.0f);
        float c = std::cos(rad);
        float s = std::sin(rad);

        Set2DVertex(verts[i * 2 + 0], cx + r1 * c, cy + r1 * s, color);
        Set2DVertex(verts[i * 2 + 1], cx + r0 * c, cy + r0 * s, color);
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)nullptr);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);

    RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, verts.data(), vertCount);
}

static void DrawRwCircleOutline(float cx, float cy, float radius, float halfThick, int steps, CRGBA color)
{
    DrawRwRingSector(cx, cy, radius - halfThick, radius + halfThick, 0.0f, 360.0f, steps, color);
}

static void DrawRwSpoke(float cx, float cy, float r0, float r1,
    float angleDeg, float halfThick, CRGBA color)
{
    const float a = angleDeg * (3.14159265f / 180.0f);
    const float dx = std::cos(a);
    const float dy = std::sin(a);
    const float nx = -dy * halfThick;
    const float ny = dx * halfThick;

    RwIm2DVertex verts[4];
    Set2DVertex(verts[0], cx + r0 * dx - nx, cy + r0 * dy - ny, color);
    Set2DVertex(verts[1], cx + r1 * dx - nx, cy + r1 * dy - ny, color);
    Set2DVertex(verts[2], cx + r0 * dx + nx, cy + r0 * dy + ny, color);
    Set2DVertex(verts[3], cx + r1 * dx + nx, cy + r1 * dy + ny, color);

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)nullptr);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);

    RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, verts, 4);
}

static void BeginText()
{
    CFont::SetBackgroundOff();
    CFont::SetBackGroundOnlyTextOff();
    CFont::SetWrapx(SCREEN_WIDTH);
    CFont::SetCentreSize(SCREEN_WIDTH);
    CFont::SetAlphaFade(255.0f);
    CFont::SetPropOn();
    CFont::SetDropShadowPosition(0);
    CFont::SetSlant(0.0f);
}

static void DrawWeaponIcon(eWeaponType type, float x, float y, float size, CRGBA color)
{
    CSprite2d* sprite = GetWeaponSprite(type);
    if (!sprite)
        return;
    CRect rect(x - size * 0.5f, y - size * 0.5f, x + size * 0.5f, y + size * 0.5f);
    sprite->SetRenderState();
    sprite->Draw(rect, color);
}

static void DrawWheel()
{
    if (!gWheelOpen)
        return;

    CPlayerPed* player = FindPlayerPed();
    if (!player)
        return;

    if (!gIconsTried)
        LoadWeaponIcons();

    std::vector<SlotInfo> slots;
    CollectSlots(player, slots);
    const int count = static_cast<int>(slots.size());
    if (count == 0)
        return;
    if (gSelectedSlot >= count)
        gSelectedSlot = count - 1;

    const float cx = SCREEN_COORD_CENTER_X;
    const float cy = SCREEN_COORD_CENTER_Y;
    const float r0 = SCREEN_MULTIPLIER(gSettings.wheelInnerRadius);
    const float r1 = SCREEN_MULTIPLIER(gSettings.wheelRadius);

    // 1. 全屏背景磨砂虚化与暗角
    if (gSettings.enableBackgroundBlur) {
        CSprite2d::DrawRect(CRect(-2.0f, -2.0f, SCREEN_WIDTH + 2.0f, SCREEN_HEIGHT + 2.0f), CRGBA(5, 0, 15, 30));
        CSprite2d::DrawRect(CRect(2.0f, 2.0f, SCREEN_WIDTH - 2.0f, SCREEN_HEIGHT - 2.0f), CRGBA(0, 5, 12, 30));
        CSprite2d::DrawRect(CRect(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT), CRGBA(10, 6, 20, 45));
    }
    else {
        CSprite2d::DrawRect(CRect(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT), CRGBA(8, 4, 18, 55));
    }
    CSprite2d::SetRecipNearClip();

    const float sector = 360.0f / count;
    const float spokeHalf = SCREEN_MULTIPLIER(1.2f);

    // 2. 底盘半透明圆环
    DrawRwRingSector(cx, cy, r0, r1, 0.0f, 360.0f, 64, CRGBA(14, 10, 24, 135));

    // 3. 高亮选中扇区：亮白色高光扇区与外边缘纯白描边
    {
        float a0 = -90.0f + sector * gSelectedSlot;
        float a1 = a0 + sector;
        DrawRwRingSector(cx, cy, r0, r1, a0, a1, 32, CRGBA(255, 255, 255, 140));
        DrawRwRingSector(cx, cy, r1 - SCREEN_MULTIPLIER(2.5f), r1, a0, a1, 32, CRGBA(255, 255, 255, 255));
    }

    // 4. 中心圆深色遮罩
    DrawRwCircleFan(cx, cy, r0, 64, CRGBA(8, 6, 14, 180));

    // 5. 放射状分割线（淡粉紫：RGB 255, 175, 235）
    for (int i = 0; i < count; ++i) {
        float ang = -90.0f + sector * i;
        DrawRwSpoke(cx, cy, r0, r1, ang, spokeHalf, CRGBA(255, 175, 235, 75));
    }

    // 6. 内外双圈光环（图标框淡粉紫：RGB 255, 175, 235）
    DrawRwCircleOutline(cx, cy, r0, SCREEN_MULTIPLIER(1.6f), 64, CRGBA(255, 175, 235, 220));
    DrawRwCircleOutline(cx, cy, r1, SCREEN_MULTIPLIER(1.6f), 64, CRGBA(255, 175, 235, 230));

    // 7. 绘制武器图标（尺寸整体放大 1.2 倍）
    for (int i = 0; i < count; ++i) {
        float a0 = -90.0f + sector * i;
        float a1 = a0 + sector;
        const bool selected = (i == gSelectedSlot);
        const SlotInfo& info = slots[i];

        const float midDeg = (a0 + a1) * 0.5f;
        const float midRad = midDeg * (3.14159265f / 180.0f);
        const float midR = (r0 + r1) * 0.5f;
        const float ix = cx + midR * std::cos(midRad);
        const float iy = cy + midR * std::sin(midRad);

        CSprite2d* sprite = GetWeaponSprite(info.type);
        if (!sprite || !info.hasWeapon)
            continue;

        float iconSize = SCREEN_MULTIPLIER(selected ? 125.0f : 101.0f);
        DrawWeaponIcon(info.type, ix, iy, iconSize, CRGBA(255, 255, 255, 255));
    }

    // 8. 中心文字
    if (!slots.empty()) {
        const SlotInfo& sel = slots[gSelectedSlot];
        const wchar_t* name = GetWeaponDisplayName(sel.type);
        bool hasAmmo = gSettings.showAmmo && sel.hasWeapon && sel.type != WEAPONTYPE_UNARMED && sel.ammo > 0;

        BeginText();
        // 武器名字：纯白色
        CFont::SetFontStyle(FONT_STANDARD);
        CFont::SetCentreOn();
        CFont::SetScale(SCREEN_MULTIPLIER(0.72f), SCREEN_MULTIPLIER(1.35f));
        CFont::SetColor(CRGBA(255, 255, 255, 255));
        CFont::SetDropColor(CRGBA(10, 10, 15, 230));
        CFont::SetDropShadowPosition(2);

        float nameY = hasAmmo ? (cy - SCREEN_MULTIPLIER(20.0f)) : (cy - SCREEN_MULTIPLIER(8.0f));
        CFont::PrintString(cx, nameY, name);

        // 弹药数字：霓虹粉色（Neon Pink）
        if (hasAmmo) {
            wchar_t ammoBuf[16];
            swprintf(ammoBuf, 16, L"%u", sel.ammo);
            CFont::SetFontStyle(FONT_STANDARD);
            CFont::SetSlant(0.0f);
            CFont::SetCentreOn();
            CFont::SetScale(SCREEN_MULTIPLIER(0.60f), SCREEN_MULTIPLIER(1.15f));
            CFont::SetColor(CRGBA(255, 105, 180, 255));
            CFont::SetDropColor(CRGBA(20, 5, 15, 220));
            CFont::SetDropShadowPosition(2);
            CFont::PrintString(cx, cy + SCREEN_MULTIPLIER(14.0f), ammoBuf);
        }
    }
}

// ---------------------------------------------------------------------------
// Process
// ---------------------------------------------------------------------------
static void ProcessWheel()
{
    if (!IsGameplayAllowed()) {
        if (gWheelOpen)
            CloseWheel(false);
        gWheelHeld = false;
        return;
    }

    bool held = IsPrimaryKeyDown();

    if (held && !gWheelHeld) {
        if (IsPlayerValidForWheel())
            OpenWheel();
        else
            gWheelHeld = true;
    }
    else if (held && gWheelOpen) {
        UpdateSelection();
        SuppressCombatInput();
    }
    else if (!held && gWheelHeld) {
        CloseWheel(gWheelOpen);
    }

    if (gWheelOpen && !IsPlayerValidForWheel())
        CloseWheel(false);

    gWheelHeld = held;
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------
struct LSDCXX_WeaponWheelVC {
    LSDCXX_WeaponWheelVC() {
        Events::initRwEvent.after += [] {
            CIniReader ini("LSDCXX_WeaponWheelVC.ini");
            gSettings.primaryKey = ini.ReadInteger("configs", "PrimaryKey", 'Q');
            gSettings.secondaryKey = ini.ReadInteger("configs", "SecondaryKey", 0);
            gSettings.enableSlowMotion = ini.ReadInteger("configs", "EnableSlowMotion", 1);
            gSettings.slowMotionSpeed = ini.ReadFloat("configs", "SlowMotionSpeed", 0.15f);
            gSettings.invertMouseVertical = ini.ReadInteger("configs", "InvertMouseVertical", 0);
            gSettings.skipEmptySlots = ini.ReadInteger("configs", "SkipEmptySlots", 1);
            gSettings.allowInVehicle = ini.ReadInteger("configs", "AllowInVehicle", 0);
            gSettings.showAmmo = ini.ReadInteger("configs", "ShowAmmo", 1);
            gSettings.showWeaponName = ini.ReadInteger("configs", "ShowWeaponName", 1);
            gSettings.enableBackgroundBlur = ini.ReadInteger("configs", "EnableBackgroundBlur", 1);
            gSettings.wheelRadius = ini.ReadFloat("configs", "WheelRadius", 280.0f);
            gSettings.wheelInnerRadius = ini.ReadFloat("configs", "WheelInnerRadius", 110.0f);
            gSettings.selectDeadzone = ini.ReadFloat("configs", "SelectDeadzone", 30.0f);

            if (gSettings.slowMotionSpeed < 0.01f) gSettings.slowMotionSpeed = 0.01f;
            if (gSettings.slowMotionSpeed > 1.0f) gSettings.slowMotionSpeed = 1.0f;
            if (gSettings.wheelInnerRadius < 40.0f) gSettings.wheelInnerRadius = 40.0f;
            if (gSettings.wheelRadius <= gSettings.wheelInnerRadius)
                gSettings.wheelRadius = gSettings.wheelInnerRadius + 80.0f;

            for (int i = 0; i <= MAX_WEAPON_TYPE; ++i) {
                char key[16];
                sprintf_s(key, "Weapon%02d", i);
                std::string text = ini.ReadString("names", key, "");
                if (!text.empty()) {
                    int wlen = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, NULL, 0);
                    if (wlen > 0) {
                        std::vector<wchar_t> wbuf(wlen);
                        MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wbuf.data(), wlen);
                        gWeaponNames[i] = wbuf.data();
                    }
                    else {
                        gWeaponNames[i] = L"";
                    }
                }
                else {
                    gWeaponNames[i] = L"";
                }
            }

            InstallHooks();
            };

        Events::gameProcessEvent += [] {
            ProcessWheel();
            };

        Events::drawHudEvent += [] {
            DrawWheel();
            };

        Events::shutdownRwEvent.before += [] {
            if (gSlowMotionActive) {
                CTimer::ms_fTimeScale = gSavedTimeScale;
                gSlowMotionActive = false;
            }
            gWheelOpen = false;
            FreeWeaponIcons();
            };
    }
} gLSDCXX_WeaponWheelVC;