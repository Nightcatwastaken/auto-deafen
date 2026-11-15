#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

// ---------- storage ----------

// Trigger percents and the on/off switch are saved per level.
static std::string triggersKey(int levelId) {
    return fmt::format("triggers-{}", levelId);
}

static std::string enabledKey(int levelId) {
    return fmt::format("enabled-{}", levelId);
}

// The bind is shared by every level.
static std::string keyKey() {
    return "global-key";
}

// "25, 50 75" -> {25, 50, 75}. Keeps values 0-100, sorted, no duplicates.
static std::vector<int> parseTriggers(std::string raw) {
    std::replace(raw.begin(), raw.end(), ',', ' ');
    std::istringstream ss(raw);
    std::set<int> vals;
    int v;
    while (ss >> v) {
        if (v >= 0 && v <= 100) vals.insert(v);
    }
    return std::vector<int>(vals.begin(), vals.end());
}

// ---------- input ----------

static std::string keyName(int vk) {
    switch (vk) {
        case VK_LBUTTON:  return "Mouse Left";
        case VK_RBUTTON:  return "Mouse Right";
        case VK_MBUTTON:  return "Mouse Middle";
        case VK_XBUTTON1: return "Mouse Side 1";
        case VK_XBUTTON2: return "Mouse Side 2";
        default: break;
    }
    UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    char buf[64] = {};
    if (scan && GetKeyNameTextA(scan << 16, buf, sizeof(buf)) > 0) return buf;
    return fmt::format("0x{:02X}", vk);
}

// Presses and releases a key or mouse button. Mouse buttons need mouse events.
static void pressBind(int vk) {
    switch (vk) {
        case VK_LBUTTON:
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            return;
        case VK_RBUTTON:
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
            return;
        case VK_MBUTTON:
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, 0);
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);
            return;
        case VK_XBUTTON1:
            mouse_event(MOUSEEVENTF_XDOWN, 0, 0, XBUTTON1, 0);
            mouse_event(MOUSEEVENTF_XUP, 0, 0, XBUTTON1, 0);
            return;
        case VK_XBUTTON2:
            mouse_event(MOUSEEVENTF_XDOWN, 0, 0, XBUTTON2, 0);
            mouse_event(MOUSEEVENTF_XUP, 0, 0, XBUTTON2, 0);
            return;
        default:
            keybd_event(static_cast<BYTE>(vk), 0, 0, 0);
            keybd_event(static_cast<BYTE>(vk), 0, KEYEVENTF_KEYUP, 0);
            return;
    }
}

static bool isDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
// ---------- settings popup ----------

class AutoDeafenPopup : public Popup {
    int m_levelId = 0;
    int m_key = 0;
    bool m_enabled = true;
    TextInput* m_triggerInput = nullptr;
    CCLabelBMFont* m_keyLabel = nullptr;
    ButtonSprite* m_toggleSprite = nullptr;

    // Listening state for the "Set Key" button.
    bool m_listening = false;
    bool m_armed = false;   // false until every button is released, so the click that started listening doesn't count
    float m_listenFor = 0.f;

public:
    static AutoDeafenPopup* create(int levelId) {
        auto ret = new AutoDeafenPopup();
        if (ret && ret->init(levelId)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init(int levelId) {
        if (!Popup::init(320.f, 220.f)) return false;
        m_levelId = levelId;
        m_key = Mod::get()->getSavedValue<int>(keyKey(), 0);
        m_enabled = Mod::get()->getSavedValue<bool>(enabledKey(levelId), true);
        this->setTitle("Auto Deafen");

        auto size = m_mainLayer->getContentSize();
        auto cx = size.width / 2;
        auto cy = size.height / 2;

        // Percentage label and trigger box
        auto percentLabel = CCLabelBMFont::create("Percentage", "goldFont.fnt");
        percentLabel->setScale(0.5f);
        percentLabel->setPosition({cx, cy + 72});
        m_mainLayer->addChild(percentLabel);

        m_triggerInput = TextInput::create(240.f, "e.g. 25, 50, 75");
        m_triggerInput->setFilter("0123456789, ");
        m_triggerInput->setString(Mod::get()->getSavedValue<std::string>(triggersKey(levelId), ""));
        m_triggerInput->setPosition({cx, cy + 40});
        m_mainLayer->addChild(m_triggerInput);

        // Current bind
        m_keyLabel = CCLabelBMFont::create(m_key ? keyName(m_key).c_str() : "No key set", "bigFont.fnt");
        m_keyLabel->setScale(0.45f);
        m_keyLabel->setPosition({cx, cy + 10});
        m_mainLayer->addChild(m_keyLabel);

        // Button row: Set Key, Test, On/Off
        auto row = CCMenu::create();
        row->setPosition({cx, cy - 25});

        auto setBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Set Key"), this, menu_selector(AutoDeafenPopup::onListen));
        setBtn->getNormalImage()->setScale(0.7f);
        setBtn->setPosition({-95.f, 0.f});
        row->addChild(setBtn);

        auto testBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Test"), this, menu_selector(AutoDeafenPopup::onTest));
        testBtn->getNormalImage()->setScale(0.7f);
        testBtn->setPosition({0.f, 0.f});
        row->addChild(testBtn);

        m_toggleSprite = ButtonSprite::create(m_enabled ? "On" : "Off");
        auto toggleBtn = CCMenuItemSpriteExtra::create(
            m_toggleSprite, this, menu_selector(AutoDeafenPopup::onToggle));
        toggleBtn->getNormalImage()->setScale(0.7f);
        toggleBtn->setPosition({95.f, 0.f});
        row->addChild(toggleBtn);

        m_mainLayer->addChild(row);

        // Save, centered at the bottom
        auto saveMenu = CCMenu::create();
        auto saveBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Save"), this, menu_selector(AutoDeafenPopup::onSave));
        saveMenu->addChild(saveBtn);
        saveMenu->setPosition({cx, cy - 70});
        m_mainLayer->addChild(saveMenu);

        return true;
    }

    void onToggle(CCObject*) {
        m_enabled = !m_enabled;
        m_toggleSprite->setString(m_enabled ? "On" : "Off");
    }

    void onTest(CCObject*) {
        if (m_key == 0) {
            FLAlertLayer::create("Auto Deafen", "Click \"Set Key\" first.", "OK")->show();
            return;
        }
        pressBind(m_key);
    }

    // ----- Set Key -----

    void onListen(CCObject*) {
        if (m_listening) {
            cancelListen();
            return;
        }
        m_listening = true;
        m_armed = false;
        m_listenFor = 0.f;
        m_keyLabel->setString("Press a key...");
        this->schedule(schedule_selector(AutoDeafenPopup::pollBind));
    }

    void cancelListen() {
        m_listening = false;
        this->unschedule(schedule_selector(AutoDeafenPopup::pollBind));
        m_keyLabel->setString(m_key ? keyName(m_key).c_str() : "No key set");
    }

    void pollBind(float dt) {
        if (!m_listening) return;

        m_listenFor += dt;
        if (m_listenFor > 5.f) {
            cancelListen();
            return;
        }

        int pressed = 0;
        bool anyDown = false;
        bool leftOrRight = false;
        for (int vk = 1; vk <= 0xFE; ++vk) {
            short state = GetAsyncKeyState(vk);
            bool down = (state & 0x8000) != 0;   // held right now
            bool tapped = (state & 0x0001) != 0; // pressed since the last check, even if already released
            if (!down && !tapped) continue;
            if (down) anyDown = true;
            if (vk == VK_LBUTTON || vk == VK_RBUTTON) {
                if (down) leftOrRight = true;
                continue;
            }
            if (!pressed) pressed = vk;
        }

        if (!m_armed) {
            if (!anyDown) m_armed = true;
            return;
        }

        // A left or right click means the user is pressing a button, so stop listening and let it act.
        if (leftOrRight) {
            cancelListen();
            return;
        }
        if (!pressed) return;

        if (pressed != VK_ESCAPE) m_key = pressed;
        cancelListen();
    }

    // ----- Save -----

    void onSave(CCObject*) {
        auto triggers = parseTriggers(std::string(m_triggerInput->getString()));

        if (triggers.empty()) {
            FLAlertLayer::create("Auto Deafen", "Enter at least one percent between 0 and 100.", "OK")->show();
            return;
        }
        if (m_key == 0) {
            FLAlertLayer::create("Auto Deafen", "Click \"Set Key\" and press a key or mouse button first.", "OK")->show();
            return;
        }

        std::string joined;
        for (size_t i = 0; i < triggers.size(); ++i) {
            if (i) joined += ",";
            joined += std::to_string(triggers[i]);
        }
        Mod::get()->setSavedValue<std::string>(triggersKey(m_levelId), joined);
        Mod::get()->setSavedValue<int>(keyKey(), m_key);
        Mod::get()->setSavedValue<bool>(enabledKey(m_levelId), m_enabled);

        log::info("Auto Deafen: level {} triggers [{}] key 0x{:02X} enabled {}", m_levelId, joined, m_key, m_enabled);
        this->onClose(nullptr);
    }
};
// ---------- pause menu button ----------

// Icon position, measured from the right and top edges of the window.
static constexpr float ICON_FROM_RIGHT = 36.f;
static constexpr float ICON_FROM_TOP = 73.f;

static CCNode* autoDeafenIcon() {
    auto spr = CCSprite::create("icon-button.png"_spr);
    spr->setScale(30.f / spr->getContentSize().width);
    return spr;
}

class $modify(AutoDeafenPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto btn = CCMenuItemSpriteExtra::create(
            autoDeafenIcon(), this, menu_selector(AutoDeafenPauseLayer::onAutoDeafen));
        auto menu = CCMenu::create();
        menu->addChild(btn);
        auto win = CCDirector::sharedDirector()->getWinSize();
        menu->setPosition({win.width - ICON_FROM_RIGHT, win.height - ICON_FROM_TOP});
        this->addChild(menu);
    }

    void onAutoDeafen(CCObject*) {
        auto pl = PlayLayer::get();
        if (!pl || !pl->m_level) return;
        AutoDeafenPopup::create(pl->m_level->m_levelID.value())->show();
    }
};

// ---------- gameplay ----------

class $modify(AutoDeafenPlayLayer, PlayLayer) {
    struct Fields {
        std::set<int> fired;          // triggers already used this attempt
        std::vector<int> triggers;
        int key = 0;
        bool enabled = false;
        bool loaded = false;
        bool deafened = false;        // true after an odd number of presses
    };

    void loadSettings() {
        int id = m_level->m_levelID.value();
        m_fields->triggers = parseTriggers(Mod::get()->getSavedValue<std::string>(triggersKey(id), ""));
        m_fields->key = Mod::get()->getSavedValue<int>(keyKey(), 0);
        bool levelOn = Mod::get()->getSavedValue<bool>(enabledKey(id), true);
        bool masterOn = Mod::get()->getSettingValue<bool>("master-enabled");
        m_fields->enabled = levelOn && masterOn;
        m_fields->loaded = true;
    }

    // Presses the key once if we are currently deafened.
    void undeafenIfNeeded(char const* why) {
        if (m_fields->deafened && m_fields->key != 0) {
            pressBind(m_fields->key);
            m_fields->deafened = false;
            log::info("Auto Deafen: undeafened on {}", why);
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        undeafenIfNeeded("death");
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        undeafenIfNeeded("completion");
    }

    void pauseGame(bool unfocused) {
        // Alt-tabbing (unfocused) doesn't undeafen; only the pause menu does.
        if (!unfocused) undeafenIfNeeded("pause");
        PlayLayer::pauseGame(unfocused);
    }

    void onQuit() {
        undeafenIfNeeded("leaving level");
        PlayLayer::onQuit();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->fired.clear();
        m_fields->loaded = false; // re-read in case settings changed in the pause menu
    }

    void updateProgressbar() {
        PlayLayer::updateProgressbar();
        if (!m_level) return;
        if (!m_fields->loaded) loadSettings();
        if (!m_fields->enabled || m_fields->triggers.empty() || m_fields->key == 0) return;
        if (m_isPracticeMode) return; // never auto-deafen in practice mode

        int pct = this->getCurrentPercentInt();
        if (pct < 0 || pct > 100) return;

        for (int trig : m_fields->triggers) {
            if (pct >= trig && !m_fields->fired.contains(trig)) {
                m_fields->fired.insert(trig);
                pressBind(m_fields->key);
                m_fields->deafened = !m_fields->deafened;
                log::info("Auto Deafen: {}% -> pressed {} (trigger {}%)", pct, keyName(m_fields->key), trig);
            }
        }
    }
};