#include "sim_entity_state.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void SimEntityState::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_id"), &SimEntityState::get_id);
    ClassDB::bind_method(D_METHOD("set_id", "id"), &SimEntityState::set_id);
    ClassDB::bind_method(D_METHOD("get_position"), &SimEntityState::get_position);
    ClassDB::bind_method(D_METHOD("set_position", "position"), &SimEntityState::set_position);
    ClassDB::bind_method(D_METHOD("get_velocity"), &SimEntityState::get_velocity);
    ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &SimEntityState::set_velocity);
    ClassDB::bind_method(D_METHOD("get_species_id"), &SimEntityState::get_species_id);
    ClassDB::bind_method(D_METHOD("set_species_id", "species_id"), &SimEntityState::set_species_id);
    ClassDB::bind_method(D_METHOD("get_kind"), &SimEntityState::get_kind);
    ClassDB::bind_method(D_METHOD("set_kind", "kind"), &SimEntityState::set_kind);
    ClassDB::bind_method(D_METHOD("get_intent"), &SimEntityState::get_intent);
    ClassDB::bind_method(D_METHOD("set_intent", "intent"), &SimEntityState::set_intent);
    ClassDB::bind_method(D_METHOD("get_target_position"), &SimEntityState::get_target_position);
    ClassDB::bind_method(D_METHOD("set_target_position", "target_position"),
                         &SimEntityState::set_target_position);
    ClassDB::bind_method(D_METHOD("get_target_id"), &SimEntityState::get_target_id);
    ClassDB::bind_method(D_METHOD("set_target_id", "target_id"), &SimEntityState::set_target_id);
    ClassDB::bind_method(D_METHOD("get_biomass"), &SimEntityState::get_biomass);
    ClassDB::bind_method(D_METHOD("set_biomass", "biomass"), &SimEntityState::set_biomass);
    ClassDB::bind_method(D_METHOD("get_energy"), &SimEntityState::get_energy);
    ClassDB::bind_method(D_METHOD("set_energy", "energy"), &SimEntityState::set_energy);
    ClassDB::bind_method(D_METHOD("get_hydration"), &SimEntityState::get_hydration);
    ClassDB::bind_method(D_METHOD("set_hydration", "hydration"), &SimEntityState::set_hydration);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "id"), "set_id", "get_id");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "position"), "set_position", "get_position");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity"), "set_velocity", "get_velocity");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "species_id"), "set_species_id", "get_species_id");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "kind"), "set_kind", "get_kind");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "intent"), "set_intent", "get_intent");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "target_position"), "set_target_position",
                 "get_target_position");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "target_id"), "set_target_id", "get_target_id");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "biomass"), "set_biomass", "get_biomass");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "energy"), "set_energy", "get_energy");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hydration"), "set_hydration", "get_hydration");
}

void SimEntityState::set_id(int64_t id) {
    id_ = id;
}

int64_t SimEntityState::get_id() const {
    return id_;
}

void SimEntityState::set_position(Vector3 position) {
    position_ = position;
}

Vector3 SimEntityState::get_position() const {
    return position_;
}

void SimEntityState::set_velocity(Vector3 velocity) {
    velocity_ = velocity;
}

Vector3 SimEntityState::get_velocity() const {
    return velocity_;
}

void SimEntityState::set_species_id(int64_t species_id) {
    species_id_ = species_id;
}

int64_t SimEntityState::get_species_id() const {
    return species_id_;
}

void SimEntityState::set_kind(int32_t kind) {
    kind_ = kind;
}

int32_t SimEntityState::get_kind() const {
    return kind_;
}

void SimEntityState::set_intent(int32_t intent) {
    intent_ = intent;
}

int32_t SimEntityState::get_intent() const {
    return intent_;
}

void SimEntityState::set_target_position(Vector3 target_position) {
    target_position_ = target_position;
}

Vector3 SimEntityState::get_target_position() const {
    return target_position_;
}

void SimEntityState::set_target_id(int64_t target_id) {
    target_id_ = target_id;
}

int64_t SimEntityState::get_target_id() const {
    return target_id_;
}

void SimEntityState::set_biomass(double biomass) {
    biomass_ = biomass;
}

double SimEntityState::get_biomass() const {
    return biomass_;
}

void SimEntityState::set_energy(double energy) {
    energy_ = energy;
}

double SimEntityState::get_energy() const {
    return energy_;
}

void SimEntityState::set_hydration(double hydration) {
    hydration_ = hydration;
}

double SimEntityState::get_hydration() const {
    return hydration_;
}
