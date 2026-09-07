#include "sim_snapshot.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void SimSnapshot::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_tick"), &SimSnapshot::get_tick);
    ClassDB::bind_method(D_METHOD("set_tick", "tick"), &SimSnapshot::set_tick);
    ClassDB::bind_method(D_METHOD("get_alpha"), &SimSnapshot::get_alpha);
    ClassDB::bind_method(D_METHOD("set_alpha", "alpha"), &SimSnapshot::set_alpha);
    ClassDB::bind_method(D_METHOD("get_tick_dt"), &SimSnapshot::get_tick_dt);
    ClassDB::bind_method(D_METHOD("set_tick_dt", "tick_dt"), &SimSnapshot::set_tick_dt);
    ClassDB::bind_method(D_METHOD("is_paused"), &SimSnapshot::is_paused);
    ClassDB::bind_method(D_METHOD("set_paused", "paused"), &SimSnapshot::set_paused);
    ClassDB::bind_method(D_METHOD("get_simulated_hours"), &SimSnapshot::get_simulated_hours);
    ClassDB::bind_method(D_METHOD("set_simulated_hours", "hours"),
                         &SimSnapshot::set_simulated_hours);
    ClassDB::bind_method(D_METHOD("get_mean_moisture"), &SimSnapshot::get_mean_moisture);
    ClassDB::bind_method(D_METHOD("set_mean_moisture", "moisture"),
                         &SimSnapshot::set_mean_moisture);
    ClassDB::bind_method(D_METHOD("get_mean_temperature"), &SimSnapshot::get_mean_temperature);
    ClassDB::bind_method(D_METHOD("set_mean_temperature", "temperature"),
                         &SimSnapshot::set_mean_temperature);
    ClassDB::bind_method(D_METHOD("get_mean_canopy"), &SimSnapshot::get_mean_canopy);
    ClassDB::bind_method(D_METHOD("set_mean_canopy", "canopy"), &SimSnapshot::set_mean_canopy);
    ClassDB::bind_method(D_METHOD("get_mean_light"), &SimSnapshot::get_mean_light);
    ClassDB::bind_method(D_METHOD("set_mean_light", "light"), &SimSnapshot::set_mean_light);
    ClassDB::bind_method(D_METHOD("get_mean_organic"), &SimSnapshot::get_mean_organic);
    ClassDB::bind_method(D_METHOD("set_mean_organic", "organic"), &SimSnapshot::set_mean_organic);
    ClassDB::bind_method(D_METHOD("get_mean_pollination"), &SimSnapshot::get_mean_pollination);
    ClassDB::bind_method(D_METHOD("set_mean_pollination", "pollination"),
                         &SimSnapshot::set_mean_pollination);
    ClassDB::bind_method(D_METHOD("get_year_phase"), &SimSnapshot::get_year_phase);
    ClassDB::bind_method(D_METHOD("set_year_phase", "phase"), &SimSnapshot::set_year_phase);
    ClassDB::bind_method(D_METHOD("get_hour_of_day"), &SimSnapshot::get_hour_of_day);
    ClassDB::bind_method(D_METHOD("set_hour_of_day", "hour"), &SimSnapshot::set_hour_of_day);
    ClassDB::bind_method(D_METHOD("get_entities"), &SimSnapshot::get_entities);
    ClassDB::bind_method(D_METHOD("set_entities", "entities"), &SimSnapshot::set_entities);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "tick"), "set_tick", "get_tick");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "alpha"), "set_alpha", "get_alpha");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tick_dt"), "set_tick_dt", "get_tick_dt");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "is_paused");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "simulated_hours"), "set_simulated_hours",
                 "get_simulated_hours");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mean_moisture"), "set_mean_moisture",
                 "get_mean_moisture");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mean_temperature"), "set_mean_temperature",
                 "get_mean_temperature");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mean_canopy"), "set_mean_canopy",
                 "get_mean_canopy");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mean_light"), "set_mean_light", "get_mean_light");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mean_organic"), "set_mean_organic",
                 "get_mean_organic");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mean_pollination"), "set_mean_pollination",
                 "get_mean_pollination");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "year_phase"), "set_year_phase", "get_year_phase");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hour_of_day"), "set_hour_of_day",
                 "get_hour_of_day");
    ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "entities", PROPERTY_HINT_ARRAY_TYPE, "SimEntityState"),
                 "set_entities", "get_entities");
}

void SimSnapshot::set_tick(int64_t tick) {
    tick_ = tick;
}

int64_t SimSnapshot::get_tick() const {
    return tick_;
}

void SimSnapshot::set_alpha(double alpha) {
    alpha_ = alpha;
}

double SimSnapshot::get_alpha() const {
    return alpha_;
}

void SimSnapshot::set_tick_dt(double tick_dt) {
    tick_dt_ = tick_dt;
}

double SimSnapshot::get_tick_dt() const {
    return tick_dt_;
}

void SimSnapshot::set_paused(bool paused) {
    paused_ = paused;
}

bool SimSnapshot::is_paused() const {
    return paused_;
}

void SimSnapshot::set_simulated_hours(double simulated_hours) {
    simulated_hours_ = simulated_hours;
}

double SimSnapshot::get_simulated_hours() const {
    return simulated_hours_;
}

void SimSnapshot::set_mean_moisture(double mean_moisture) {
    mean_moisture_ = mean_moisture;
}

double SimSnapshot::get_mean_moisture() const {
    return mean_moisture_;
}

void SimSnapshot::set_mean_temperature(double mean_temperature) {
    mean_temperature_ = mean_temperature;
}

double SimSnapshot::get_mean_temperature() const {
    return mean_temperature_;
}

void SimSnapshot::set_mean_canopy(double mean_canopy) {
    mean_canopy_ = mean_canopy;
}

double SimSnapshot::get_mean_canopy() const {
    return mean_canopy_;
}

void SimSnapshot::set_mean_light(double mean_light) {
    mean_light_ = mean_light;
}

double SimSnapshot::get_mean_light() const {
    return mean_light_;
}

void SimSnapshot::set_mean_organic(double mean_organic) {
    mean_organic_ = mean_organic;
}

double SimSnapshot::get_mean_organic() const {
    return mean_organic_;
}

void SimSnapshot::set_mean_pollination(double mean_pollination) {
    mean_pollination_ = mean_pollination;
}

double SimSnapshot::get_mean_pollination() const {
    return mean_pollination_;
}

void SimSnapshot::set_year_phase(double year_phase) {
    year_phase_ = year_phase;
}

double SimSnapshot::get_year_phase() const {
    return year_phase_;
}

void SimSnapshot::set_hour_of_day(double hour_of_day) {
    hour_of_day_ = hour_of_day;
}

double SimSnapshot::get_hour_of_day() const {
    return hour_of_day_;
}

void SimSnapshot::set_entities(const TypedArray<SimEntityState>& entities) {
    entities_ = entities;
}

TypedArray<SimEntityState> SimSnapshot::get_entities() const {
    return entities_;
}
