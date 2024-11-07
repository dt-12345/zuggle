#pragma once

// I have a lot more of these classes mapped out so feel free to ask if you want more info
// I'm just too lazy to fill it all in here

struct PtrArray {
    int count;
    int max;
    void** array;

    void* at(size_t index) {
        if (index >= count) {
            return array[0];
        }
        return array[index];
    }
};

struct EquipmentUserComponent;
struct DynamicEquipmentComponent;

struct GameActor {
    char _[0x218];
    char* name;
    PtrArray components;

    EquipmentUserComponent* get_equipment_user_component() {
        return reinterpret_cast<EquipmentUserComponent*>(components.at(70));
    }
    DynamicEquipmentComponent* get_dynamic_equipment_component() {
        return reinterpret_cast<DynamicEquipmentComponent*>(components.at(65));
    }
};

struct LinkData {
    char _00[0x40];
    GameActor* actor;
    int actor_id;
    u64 refs;
    u8 _058;
};
static_assert(offsetof(LinkData, refs) == 0x50);
static_assert(sizeof(LinkData) == 0x60);

struct ActorLink {
    void* vtable;
    LinkData* link_data;
    int actor_id;
    u8 stuff[4];

    bool is_valid() const {
        if (!link_data || !(link_data->actor)) return false;
        return true;
    }

    GameActor* get_actor() {
        if (is_valid()) return link_data->actor;
        return nullptr;
    }
};
static_assert(sizeof(ActorLink) == 0x18);

struct EquipmentUserComponent {
    char _00[0x18];
    GameActor* actor;
    ActorLink dynamic_equipment[8];
    ActorLink static_equipment[12];
    ActorLink _200;
    char _218[0x4588 - 0x218];
    ActorLink weapon;
    ActorLink shield;
    ActorLink bow;
    char _45d0[0x4750 - 0x45d0];
};
static_assert(offsetof(EquipmentUserComponent, _200) == 0x200);
static_assert(offsetof(EquipmentUserComponent, weapon) == 0x4588);
static_assert(sizeof(EquipmentUserComponent) == 0x4750);

enum DynamicEquipmentState : u32 {
    Unequiped = 0,
    EquipedDrawn = 1,
    EquipedSheathed = 2,
    Thrown = 3,
};

struct VFRCounter {
    float time;
    float last_time;
    float rate;
};

// too lazy to fill this in for equipment, I have the one for shootables kinda though
struct StateBase {
    virtual StateBase* checkDerivedRuntimeTypeInfo(void*);
    virtual void* getRuntimeTypeInfo();
    virtual ~StateBase();
    virtual void f04();
    virtual void f05();
    virtual void f06();
    virtual void f07(float*);
    virtual void f08();
    virtual void f09(float*);

    VFRCounter timer;
    u8 _14;
    GameActor* actor;
};
static_assert(sizeof(StateBase) == 0x20);

struct EquipmentStateController {
    u8 flags;
    DynamicEquipmentState state;
    DynamicEquipmentState previous_state;
    StateBase* current_state;
    StateBase* states[4]; // one for each state
};

// this is also the base class for WeaponComponent, ShieldComponent, BowComponent, etc.
// dynamic equipment has dynamic state (see the enum above), static equipment does not (e.g. armor)
struct DynamicEquipmentComponent {
    char _00[0x3d0];
    EquipmentStateController state_controller;
    // cannot be bothered to fill in this
};

struct CoreCounter {
    float raw_delta_frame;
    float raw_delta_time;
    // time mult index ring buffer but I'm lazy
};