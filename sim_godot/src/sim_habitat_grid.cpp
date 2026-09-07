#include "sim_habitat_grid.hpp"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void SimHabitatGrid::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_width"), &SimHabitatGrid::get_width);
    ClassDB::bind_method(D_METHOD("set_width", "width"), &SimHabitatGrid::set_width);
    ClassDB::bind_method(D_METHOD("get_height"), &SimHabitatGrid::get_height);
    ClassDB::bind_method(D_METHOD("set_height", "height"), &SimHabitatGrid::set_height);
    ClassDB::bind_method(D_METHOD("get_cell_size"), &SimHabitatGrid::get_cell_size);
    ClassDB::bind_method(D_METHOD("set_cell_size", "cell_size"), &SimHabitatGrid::set_cell_size);
    ClassDB::bind_method(D_METHOD("get_origin"), &SimHabitatGrid::get_origin);
    ClassDB::bind_method(D_METHOD("set_origin", "origin"), &SimHabitatGrid::set_origin);
    ClassDB::bind_method(D_METHOD("get_elevation"), &SimHabitatGrid::get_elevation);
    ClassDB::bind_method(D_METHOD("set_elevation", "elevation"), &SimHabitatGrid::set_elevation);
    ClassDB::bind_method(D_METHOD("get_moisture"), &SimHabitatGrid::get_moisture);
    ClassDB::bind_method(D_METHOD("set_moisture", "moisture"), &SimHabitatGrid::set_moisture);
    ClassDB::bind_method(D_METHOD("get_nutrients"), &SimHabitatGrid::get_nutrients);
    ClassDB::bind_method(D_METHOD("set_nutrients", "nutrients"), &SimHabitatGrid::set_nutrients);
    ClassDB::bind_method(D_METHOD("get_temperature"), &SimHabitatGrid::get_temperature);
    ClassDB::bind_method(D_METHOD("set_temperature", "temperature"),
                         &SimHabitatGrid::set_temperature);
    ClassDB::bind_method(D_METHOD("get_canopy"), &SimHabitatGrid::get_canopy);
    ClassDB::bind_method(D_METHOD("set_canopy", "canopy"), &SimHabitatGrid::set_canopy);
    ClassDB::bind_method(D_METHOD("get_light"), &SimHabitatGrid::get_light);
    ClassDB::bind_method(D_METHOD("set_light", "light"), &SimHabitatGrid::set_light);
    ClassDB::bind_method(D_METHOD("get_organic"), &SimHabitatGrid::get_organic);
    ClassDB::bind_method(D_METHOD("set_organic", "organic"), &SimHabitatGrid::set_organic);
    ClassDB::bind_method(D_METHOD("get_pollination"), &SimHabitatGrid::get_pollination);
    ClassDB::bind_method(D_METHOD("set_pollination", "pollination"),
                         &SimHabitatGrid::set_pollination);
    ClassDB::bind_method(D_METHOD("get_surface"), &SimHabitatGrid::get_surface);
    ClassDB::bind_method(D_METHOD("set_surface", "surface"), &SimHabitatGrid::set_surface);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "width"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "height"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size"), "set_cell_size", "get_cell_size");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "origin"), "set_origin", "get_origin");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "elevation"), "set_elevation",
                 "get_elevation");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "moisture"), "set_moisture",
                 "get_moisture");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "nutrients"), "set_nutrients",
                 "get_nutrients");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "temperature"), "set_temperature",
                 "get_temperature");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "canopy"), "set_canopy", "get_canopy");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "light"), "set_light", "get_light");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "organic"), "set_organic",
                 "get_organic");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "pollination"), "set_pollination",
                 "get_pollination");
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "surface"), "set_surface", "get_surface");
}

void SimHabitatGrid::set_width(int32_t width) {
    width_ = width;
}

int32_t SimHabitatGrid::get_width() const {
    return width_;
}

void SimHabitatGrid::set_height(int32_t height) {
    height_ = height;
}

int32_t SimHabitatGrid::get_height() const {
    return height_;
}

void SimHabitatGrid::set_cell_size(double cell_size) {
    cell_size_ = cell_size;
}

double SimHabitatGrid::get_cell_size() const {
    return cell_size_;
}

void SimHabitatGrid::set_origin(Vector3 origin) {
    origin_ = origin;
}

Vector3 SimHabitatGrid::get_origin() const {
    return origin_;
}

void SimHabitatGrid::set_elevation(const PackedFloat32Array& elevation) {
    elevation_ = elevation;
}

PackedFloat32Array SimHabitatGrid::get_elevation() const {
    return elevation_;
}

void SimHabitatGrid::set_moisture(const PackedFloat32Array& moisture) {
    moisture_ = moisture;
}

PackedFloat32Array SimHabitatGrid::get_moisture() const {
    return moisture_;
}

void SimHabitatGrid::set_nutrients(const PackedFloat32Array& nutrients) {
    nutrients_ = nutrients;
}

PackedFloat32Array SimHabitatGrid::get_nutrients() const {
    return nutrients_;
}

void SimHabitatGrid::set_temperature(const PackedFloat32Array& temperature) {
    temperature_ = temperature;
}

PackedFloat32Array SimHabitatGrid::get_temperature() const {
    return temperature_;
}

void SimHabitatGrid::set_canopy(const PackedFloat32Array& canopy) {
    canopy_ = canopy;
}

PackedFloat32Array SimHabitatGrid::get_canopy() const {
    return canopy_;
}

void SimHabitatGrid::set_light(const PackedFloat32Array& light) {
    light_ = light;
}

PackedFloat32Array SimHabitatGrid::get_light() const {
    return light_;
}

void SimHabitatGrid::set_organic(const PackedFloat32Array& organic) {
    organic_ = organic;
}

PackedFloat32Array SimHabitatGrid::get_organic() const {
    return organic_;
}

void SimHabitatGrid::set_pollination(const PackedFloat32Array& pollination) {
    pollination_ = pollination;
}

PackedFloat32Array SimHabitatGrid::get_pollination() const {
    return pollination_;
}

void SimHabitatGrid::set_surface(const PackedByteArray& surface) {
    surface_ = surface;
}

PackedByteArray SimHabitatGrid::get_surface() const {
    return surface_;
}
