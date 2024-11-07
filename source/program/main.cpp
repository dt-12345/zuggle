#include "lib.hpp"

#include "nn.hpp"
#include "utils.hpp"
#include "structs.hpp"
#include "binaryoffsethelper.h"

u32 version;

// Pouch.Weapon.EquipIndex
const u32 weapon_index = 0xb80e3dbb;
// Pouch.Bow.EquipIndex
const u32 bow_index = 0x2cf18d2b;
// Pouch.Shield.EquipIndex
const u32 shield_index = 0x350ad90e;

using Dtor = void (ActorLink*);
Dtor* ActorLinkDtor = nullptr;

using SetInt = void (void*, int, u32);
SetInt* setInt = nullptr; // ideally use gmd::GameDataMgr::setIntArray and check the current pouch index in game::pouchcontent::PouchMgr

using SetPendingDrop = void (DynamicEquipmentComponent**);
SetPendingDrop* setPendingDrop = nullptr; // tbh, I could just implement this function myself bc it's literally setting a single value

using GetCounter = CoreCounter* (void*);
GetCounter* getCoreCounter = nullptr;

void** s_GmdMgrPtr = nullptr;

void** s_VFRMgrPtr = nullptr;

// TODO: port these offsets to other versions
static constexpr u64 s_WhistleOffsets[] = {
    0x01d2e1dc, 0, 0, 0, 0, 0x01d8fecc,
};
static constexpr u64 s_DtorOffsets[] = {
    0x009665d0, 0, 0, 0, 0, 0x0076eb88,
};
static constexpr u64 s_MgrOffsets[] = {
    0x04646c60, 0, 0, 0, 0, 0x04721b98,
};
static constexpr u64 s_SetOffsets[] = {
    0x00b90968, 0, 0, 0, 0, 0x00b51b08,
};
static constexpr u64 s_SetDropOffsets[] = {
    0x0153e984, 0, 0, 0, 0, 0x0156ac04,
};
static constexpr u64 s_CoreCountOffsets[] = {
    0x00850d7c, 0, 0, 0, 0, 0x0065a508,
};
static constexpr u64 s_VFRMgrOffsets[] = {
    0x04661598, 0, 0, 0, 0, 0x04725bb8,
};

void set_pending_drop(GameActor* actor) {
    if (actor == nullptr) return;
    if (version == 0) {
        // cursed but it's the laziest solution to the dependency ring buffer being smaller on 1.0.0 than other versions
        // we don't access any members before the ring buffer so it works in this case
        actor = reinterpret_cast<GameActor*>(reinterpret_cast<uintptr_t>(actor) - 0x8);
    }
    DynamicEquipmentComponent* cmp = actor->get_dynamic_equipment_component();
    setPendingDrop(&cmp);
    // sets the flag that marks the equipment as waiting to be dropped (meaning it ignores all state changes except dropping)
}

// function is inlined so we have to recreate it
void setEquipedDrawn(DynamicEquipmentComponent* cmp) {
    float delta_frame = 1.f;
    if (s_VFRMgrPtr && *s_VFRMgrPtr) {
        CoreCounter* counter = getCoreCounter(*s_VFRMgrPtr);
        delta_frame = counter->raw_delta_frame;
    }

    if (cmp->state_controller.current_state) {
        cmp->state_controller.current_state->_14 = 0;
        cmp->state_controller.current_state->f09(&delta_frame);
        cmp->state_controller.previous_state = cmp->state_controller.state;
        cmp->state_controller.flags |= 2;
    }
    if (cmp->state_controller.states[EquipedDrawn]) {
        cmp->state_controller.states[EquipedDrawn]->f07(&delta_frame);
        cmp->state_controller.states[EquipedDrawn]->timer.time = 0.f;
        cmp->state_controller.states[EquipedDrawn]->timer.last_time = 0.f;
        cmp->state_controller.states[EquipedDrawn]->timer.rate = 1.f;
        cmp->state_controller.state = EquipedDrawn;
        cmp->state_controller.flags |= 1;
    }
};

void equip_and_draw(GameActor* actor) {
    if (actor == nullptr) return;
    if (version == 0) {
        actor = reinterpret_cast<GameActor*>(reinterpret_cast<uintptr_t>(actor) - 0x8);
    }
    DynamicEquipmentComponent* cmp = actor->get_dynamic_equipment_component();
    if (!cmp) return;
    setEquipedDrawn(cmp);
}

HOOK_DEFINE_INLINE(Whistle) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        GameActor* actor = reinterpret_cast<GameActor*>(ctx->X[0]);
        if (version == 0) {
            actor = reinterpret_cast<GameActor*>(ctx->X[0] - 0x8); // cursed but whatever
        }
        if (!actor) return;
        EquipmentUserComponent* cmp = actor->get_equipment_user_component();
        if (!cmp) return;

        // remove these three lines to get a normal zuggle but in the equiped + sheathed state
        equip_and_draw(cmp->dynamic_equipment[0].get_actor()); // weapon
        equip_and_draw(cmp->dynamic_equipment[3].get_actor()); // bow
        equip_and_draw(cmp->dynamic_equipment[4].get_actor()); // shield

        // remove these three lines to get a dynamic zuggle
        set_pending_drop(cmp->dynamic_equipment[0].get_actor()); // weapon
        set_pending_drop(cmp->dynamic_equipment[3].get_actor()); // bow
        set_pending_drop(cmp->dynamic_equipment[4].get_actor()); // shield

        // these three lines are what actually trigger the zuggle
        ActorLinkDtor(&cmp->dynamic_equipment[0]); // weapon
        ActorLinkDtor(&cmp->dynamic_equipment[3]); // bow
        ActorLinkDtor(&cmp->dynamic_equipment[4]); // shield
        
        if (!s_GmdMgrPtr || !*s_GmdMgrPtr) return;
        // using this is kinda hacky since we're unconditionally setting index 0 instead of checking the current pouch index
        // ideally you just check PouchMgr for the current index and use setIntArray instead but I'm lazy so screw proving grounds
        // we do this so the inventory + equipment aren't desynced
        // remove these three lines if you want the equipment to still be equipped in the inventory
        setInt(*s_GmdMgrPtr, -1, weapon_index);
        setInt(*s_GmdMgrPtr, -1, bow_index);
        setInt(*s_GmdMgrPtr, -1, shield_index);
    }
};

extern "C" void exl_main(void* x0, void* x1) {

    char buf[500];

    /* Setup hooking enviroment */
    exl::hook::Initialize();

    PRINT("Getting app version...");
    version = InitializeAppVersion();
    if (version == 0xffffffff || version > 5) {
        PRINT("Error getting version");
        return;
    }
    PRINT("Version index %d", version);

    ActorLinkDtor = reinterpret_cast<Dtor*>(exl::util::modules::GetTargetOffset(s_DtorOffsets[version]));
    setInt = reinterpret_cast<SetInt*>(exl::util::modules::GetTargetOffset(s_SetOffsets[version]));
    setPendingDrop = reinterpret_cast<SetPendingDrop*>(exl::util::modules::GetTargetOffset(s_SetDropOffsets[version]));
    getCoreCounter = reinterpret_cast<GetCounter*>(exl::util::modules::GetTargetOffset(s_CoreCountOffsets[version]));
    s_GmdMgrPtr = reinterpret_cast<void**>(exl::util::modules::GetTargetOffset(s_MgrOffsets[version]));
    s_VFRMgrPtr = reinterpret_cast<void**>(exl::util::modules::GetTargetOffset(s_VFRMgrOffsets[version]));
    
    Whistle::InstallAtOffset(s_WhistleOffsets[version]);
    
    return;
}

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT(0x420);
}