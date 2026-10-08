#include "rectangle_layer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

#ifndef MLN_RECTANGLE_PLUGIN_VERSION
#define MLN_RECTANGLE_PLUGIN_VERSION "0.1.0-local"
#endif

namespace {

constexpr uint32_t positionAttribute = 0;
constexpr uint32_t cornerAttribute = 1;
constexpr uint32_t widthMinimumAttribute = 2;
constexpr uint32_t widthMaximumAttribute = 3;
constexpr uint32_t heightMinimumAttribute = 4;
constexpr uint32_t heightMaximumAttribute = 5;
constexpr uint32_t colorMinimumAttribute = 6;
constexpr uint32_t colorMaximumAttribute = 7;
constexpr uint32_t strokeWidthMinimumAttribute = 8;
constexpr uint32_t strokeWidthMaximumAttribute = 9;
constexpr uint32_t strokeColorMinimumAttribute = 10;
constexpr uint32_t strokeColorMaximumAttribute = 11;
constexpr uint32_t vertexStream = 0;
constexpr uint64_t rectangleDrawable = 1;

constexpr mln_plugin_string str(const char* value, size_t size) { return {value, size}; }

template <size_t N>
constexpr mln_plugin_string str(const char (&value)[N]) {
    return str(value, N - 1);
}

struct Vertex {
    int16_t position[2];
    int16_t corner[2];
};

static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, corner) == 4);
static_assert(sizeof(Vertex) == 8);

struct Layout {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<mln_plugin_segment_v1> segments;
    std::vector<mln_plugin_feature_vertex_range_v1> featureRanges;
    std::array<mln_plugin_vertex_stream_v1, 1> streams{};
    std::array<mln_plugin_attribute_binding_v1, 2> attributes{};
    std::array<mln_plugin_drawable_descriptor_v1, 1> drawables{};
};

void startSegment(Layout& layout) {
    mln_plugin_segment_v1 segment{};
    segment.struct_size = sizeof(segment);
    segment.vertex_offset = static_cast<uint32_t>(layout.vertices.size());
    segment.index_offset = static_cast<uint32_t>(layout.indices.size());
    layout.segments.push_back(segment);
}

mln_plugin_status createLayout(const mln_plugin_layout_context_v1* context, void** instance) {
    if (!context || context->struct_size < sizeof(*context) || !instance) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto* layout = new (std::nothrow) Layout();
    if (!layout) return MLN_PLUGIN_STATUS_CALLBACK_ERROR;
    startSegment(*layout);
    *instance = layout;
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status layoutFeature(void* instance, const mln_plugin_feature_v1* feature) {
    if (!instance || !feature || feature->struct_size < sizeof(*feature) ||
        feature->geometry_type != MLN_PLUGIN_GEOMETRY_POINT || !feature->points) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto& layout = *static_cast<Layout*>(instance);
    const auto firstVertex = static_cast<uint32_t>(layout.vertices.size());

    for (size_t pointIndex = 0; pointIndex < feature->point_count; ++pointIndex) {
        auto& segment = layout.segments.back();
        if (segment.vertex_length > std::numeric_limits<uint16_t>::max() - 4u) {
            startSegment(layout);
        }
        auto& active = layout.segments.back();
        const uint16_t base = static_cast<uint16_t>(active.vertex_length);
        const auto point = feature->points[pointIndex];
        constexpr int16_t corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const auto& corner : corners) {
            Vertex vertex{};
            vertex.position[0] = point.x;
            vertex.position[1] = point.y;
            vertex.corner[0] = corner[0];
            vertex.corner[1] = corner[1];
            layout.vertices.push_back(vertex);
        }
        const uint16_t quad[] = {base, static_cast<uint16_t>(base + 1), static_cast<uint16_t>(base + 2),
                                 base, static_cast<uint16_t>(base + 2), static_cast<uint16_t>(base + 3)};
        layout.indices.insert(layout.indices.end(), std::begin(quad), std::end(quad));
        active.vertex_length += 4;
        active.index_length += 6;
    }
    if (layout.vertices.size() > firstVertex) {
        layout.featureRanges.push_back({sizeof(mln_plugin_feature_vertex_range_v1),
                                        feature->feature_index,
                                        rectangleDrawable,
                                        firstVertex,
                                        static_cast<uint32_t>(layout.vertices.size() - firstVertex)});
    }
    return MLN_PLUGIN_STATUS_OK;
}

mln_plugin_status finishLayout(void* instance, mln_plugin_bucket_v1* output) {
    if (!instance || !output || output->struct_size < sizeof(*output)) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    auto& layout = *static_cast<Layout*>(instance);
    layout.segments.erase(std::remove_if(layout.segments.begin(),
                                         layout.segments.end(),
                                         [](const auto& segment) { return segment.index_length == 0; }),
                          layout.segments.end());
    layout.streams[0] = {sizeof(mln_plugin_vertex_stream_v1),
                         vertexStream,
                         reinterpret_cast<const uint8_t*>(layout.vertices.data()),
                         layout.vertices.size() * sizeof(Vertex),
                         static_cast<uint32_t>(layout.vertices.size()),
                         sizeof(Vertex)};
    layout.attributes = {{
        {sizeof(mln_plugin_attribute_binding_v1), positionAttribute, vertexStream, offsetof(Vertex, position)},
        {sizeof(mln_plugin_attribute_binding_v1), cornerAttribute, vertexStream, offsetof(Vertex, corner)},
    }};
    auto& drawable = layout.drawables[0];
    drawable.struct_size = sizeof(drawable);
    drawable.drawable_key = rectangleDrawable;
    drawable.shader_id = str("rectangle");
    // Indexed triangles in the translucent pass with premultiplied-alpha blending.
    // Point ownership is half-open; rectangles may extend across their tile edge.
    drawable.depth_mode = MLN_PLUGIN_DRAWABLE_DEPTH_READ_ONLY;
    drawable.attributes = layout.attributes.data();
    drawable.attribute_count = layout.attributes.size();
    drawable.segments = layout.segments.data();
    drawable.segment_count = layout.segments.size();

    output->vertex_streams = layout.vertices.empty() ? nullptr : layout.streams.data();
    output->vertex_stream_count = layout.vertices.empty() ? 0 : layout.streams.size();
    output->indices = layout.indices.data();
    output->index_count = layout.indices.size();
    output->drawables = layout.indices.empty() ? nullptr : layout.drawables.data();
    output->drawable_count = layout.indices.empty() ? 0 : layout.drawables.size();
    output->query_radius = 0.0f;
    output->feature_vertex_ranges = layout.featureRanges.data();
    output->feature_vertex_range_count = layout.featureRanges.size();
    return MLN_PLUGIN_STATUS_OK;
}

void destroyLayout(void* instance) { delete static_cast<Layout*>(instance); }

bool pointInPolygon(double x, double y, const mln_plugin_tile_point_v1* polygon, size_t count) {
    if (count < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        const double xi = polygon[i].x;
        const double yi = polygon[i].y;
        const double xj = polygon[j].x;
        const double yj = polygon[j].y;
        if (((yi > y) != (yj > y)) &&
            x < (xj - xi) * (y - yi) / ((yj - yi) == 0.0 ? 1.0 : (yj - yi)) + xi) {
            inside = !inside;
        }
    }
    return inside;
}

bool segmentIntersectsRectangle(double x0,
                                double y0,
                                double x1,
                                double y1,
                                double minX,
                                double minY,
                                double maxX,
                                double maxY) {
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double p[] = {-dx, dx, -dy, dy};
    const double q[] = {x0 - minX, maxX - x0, y0 - minY, maxY - y0};
    double lower = 0.0;
    double upper = 1.0;
    for (size_t i = 0; i < 4; ++i) {
        if (p[i] == 0.0) {
            if (q[i] < 0.0) return false;
            continue;
        }
        const double ratio = q[i] / p[i];
        if (p[i] < 0.0) {
            lower = std::max(lower, ratio);
        } else {
            upper = std::min(upper, ratio);
        }
        if (lower > upper) return false;
    }
    return true;
}

uint8_t queryFeature(const mln_plugin_feature_v1* feature,
                     const mln_plugin_tile_point_v1* query,
                     size_t queryCount,
                     const mln_plugin_query_context_v1* context,
                     const mln_plugin_property_value_v1* properties,
                     size_t propertyCount) {
    if (!feature || !feature->points || !query || !queryCount || !context) return 0;
    const auto pixelsToTileUnits = context->pixels_to_tile_units;
    float width = 10.0f;
    float height = 10.0f;
    float strokeWidth = 0.0f;
    for (size_t i = 0; i < propertyCount; ++i) {
        const auto& candidate = properties[i];
        if (!candidate.name.data || candidate.value.type != MLN_PLUGIN_VALUE_FLOAT) continue;
        const std::string name(candidate.name.data, candidate.name.size);
        if (name == "rectangle-width") width = candidate.value.data.float_value;
        if (name == "rectangle-height") height = candidate.value.data.float_value;
        if (name == "rectangle-stroke-width") strokeWidth = candidate.value.data.float_value;
    }
    const auto stroke = std::max(0.0f, strokeWidth);
    const double halfWidth = (std::max(0.0f, width) * 0.5 + stroke) * pixelsToTileUnits;
    const double halfHeight = (std::max(0.0f, height) * 0.5 + stroke) * pixelsToTileUnits;
    for (size_t pointIndex = 0; pointIndex < feature->point_count; ++pointIndex) {
        const auto point = feature->points[pointIndex];
        const double minX = point.x - halfWidth;
        const double maxX = point.x + halfWidth;
        const double minY = point.y - halfHeight;
        const double maxY = point.y + halfHeight;
        for (size_t queryIndex = 0; queryIndex < queryCount; ++queryIndex) {
            const auto& current = query[queryIndex];
            if ((current.x >= minX && current.x <= maxX && current.y >= minY && current.y <= maxY) ||
                segmentIntersectsRectangle(current.x,
                                           current.y,
                                           query[(queryIndex + 1) % queryCount].x,
                                           query[(queryIndex + 1) % queryCount].y,
                                           minX,
                                           minY,
                                           maxX,
                                           maxY)) {
                return 1;
            }
        }
        if (pointInPolygon(point.x, point.y, query, queryCount) ||
            pointInPolygon(minX, minY, query, queryCount) ||
            pointInPolygon(maxX, minY, query, queryCount) ||
            pointInPolygon(maxX, maxY, query, queryCount) ||
            pointInPolygon(minX, maxY, query, queryCount)) {
            return 1;
        }
    }
    return 0;
}

float queryRadius(const mln_plugin_property_statistics_v1* statistics,
                  size_t statisticsCount,
                  const mln_plugin_property_value_v1* cameraProperties,
                  size_t cameraPropertyCount) {
    const auto maximum = [&](const char* propertyName, float fallback) {
        const auto length = std::strlen(propertyName);
        for (size_t i = 0; i < statisticsCount; ++i) {
            const auto& candidate = statistics[i];
            if (candidate.property_name.data && candidate.property_name.size == length &&
                std::memcmp(candidate.property_name.data, propertyName, length) == 0 &&
                candidate.maximum.type == MLN_PLUGIN_VALUE_FLOAT) {
                return candidate.maximum.data.float_value;
            }
        }
        for (size_t i = 0; i < cameraPropertyCount; ++i) {
            const auto& candidate = cameraProperties[i];
            if (candidate.name.data && candidate.name.size == length &&
                std::memcmp(candidate.name.data, propertyName, length) == 0 &&
                candidate.value.type == MLN_PLUGIN_VALUE_FLOAT) {
                return candidate.value.data.float_value;
            }
        }
        return fallback;
    };
    const auto width = std::max(0.0f, maximum("rectangle-width", 10.0f));
    const auto height = std::max(0.0f, maximum("rectangle-height", 10.0f));
    const auto stroke = std::max(0.0f, maximum("rectangle-stroke-width", 0.0f));
    return 0.5f * std::max(width, height) + stroke;
}

constexpr char glVertex[] = R"SHADER(
layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_corner;
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_WIDTH_IS_UNIFORM
layout (location = 2) in float a_width_min;
layout (location = 3) in float a_width_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_HEIGHT_IS_UNIFORM
layout (location = 4) in float a_height_min;
layout (location = 5) in float a_height_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_COLOR_IS_UNIFORM
layout (location = 6) in vec4 a_color_min;
layout (location = 7) in vec4 a_color_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_WIDTH_IS_UNIFORM
layout (location = 8) in float a_stroke_width_min;
layout (location = 9) in float a_stroke_width_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_COLOR_IS_UNIFORM
layout (location = 10) in vec4 a_stroke_color_min;
layout (location = 11) in vec4 a_stroke_color_max;
#endif

layout (std140) uniform PluginDrawableUBO {
    mat4 u_matrix;
    vec2 u_extrude_scale;
    vec2 u_pad;
    float u_rectangle_width;
    float u_rectangle_height;
    float u_rectangle_stroke_width;
    float u_property_pad;
    vec4 u_rectangle_color;
    vec4 u_rectangle_stroke_color;
    vec4 u_interpolation;
    vec4 u_interpolation2;
};

out vec2 v_corner;
out vec2 v_size;
out vec4 v_color;
out float v_stroke_width;
out vec4 v_stroke_color;

void main() {
    float width = u_rectangle_width;
    float height = u_rectangle_height;
    vec4 color = u_rectangle_color;
    float stroke_width = u_rectangle_stroke_width;
    vec4 stroke_color = u_rectangle_stroke_color;
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_WIDTH_IS_UNIFORM
    width = mix(a_width_min, a_width_max, u_interpolation.x);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_HEIGHT_IS_UNIFORM
    height = mix(a_height_min, a_height_max, u_interpolation.y);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_COLOR_IS_UNIFORM
    color = mix(a_color_min, a_color_max, u_interpolation.z);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_WIDTH_IS_UNIFORM
    stroke_width = mix(a_stroke_width_min, a_stroke_width_max, u_interpolation.w);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_COLOR_IS_UNIFORM
    stroke_color = mix(a_stroke_color_min, a_stroke_color_max, u_interpolation2.x);
#endif
    vec2 size = max(vec2(0.0), vec2(width, height));
    stroke_width = clamp(stroke_width, 0.0, 0.5 * min(size.x, size.y));
    gl_Position = u_matrix * vec4(a_position, 0.0, 1.0);
    gl_Position.xy += a_corner * size * 0.5 * u_extrude_scale * gl_Position.w;
    v_corner = a_corner;
    v_size = size;
    v_color = color;
    v_stroke_width = stroke_width;
    v_stroke_color = stroke_color;
}
)SHADER";

constexpr char glFragment[] = R"SHADER(
in vec2 v_corner;
in vec2 v_size;
in vec4 v_color;
in float v_stroke_width;
in vec4 v_stroke_color;

void main() {
    vec2 edge_distance = (vec2(1.0) - abs(v_corner)) * v_size * 0.5;
    fragColor = min(edge_distance.x, edge_distance.y) < v_stroke_width ? v_stroke_color : v_color;
}
)SHADER";

constexpr char vkVertex[] = R"SHADER(
layout(location = 0) in ivec2 a_position;
layout(location = 1) in ivec2 a_corner;
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_WIDTH_IS_UNIFORM
layout(location = 2) in float a_width_min;
layout(location = 3) in float a_width_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_HEIGHT_IS_UNIFORM
layout(location = 4) in float a_height_min;
layout(location = 5) in float a_height_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_COLOR_IS_UNIFORM
layout(location = 6) in vec4 a_color_min;
layout(location = 7) in vec4 a_color_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_WIDTH_IS_UNIFORM
layout(location = 8) in float a_stroke_width_min;
layout(location = 9) in float a_stroke_width_max;
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_COLOR_IS_UNIFORM
layout(location = 10) in vec4 a_stroke_color_min;
layout(location = 11) in vec4 a_stroke_color_max;
#endif

layout(std140, set = DRAWABLE_UBO_SET_INDEX, binding = MLN_PLUGIN_UNIFORM_0_BINDING) uniform PluginDrawableUBO {
    mat4 matrix;
    vec2 extrude_scale;
    vec2 pad;
    float rectangle_width;
    float rectangle_height;
    float rectangle_stroke_width;
    float property_pad;
    vec4 rectangle_color;
    vec4 rectangle_stroke_color;
    vec4 interpolation;
    vec4 interpolation2;
} drawable;

layout(location = 0) out vec2 v_corner;
layout(location = 1) out vec2 v_size;
layout(location = 2) out vec4 v_color;
layout(location = 3) out float v_stroke_width;
layout(location = 4) out vec4 v_stroke_color;

void main() {
    float width = drawable.rectangle_width;
    float height = drawable.rectangle_height;
    vec4 color = drawable.rectangle_color;
    float stroke_width = drawable.rectangle_stroke_width;
    vec4 stroke_color = drawable.rectangle_stroke_color;
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_WIDTH_IS_UNIFORM
    width = mix(a_width_min, a_width_max, drawable.interpolation.x);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_HEIGHT_IS_UNIFORM
    height = mix(a_height_min, a_height_max, drawable.interpolation.y);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_COLOR_IS_UNIFORM
    color = mix(a_color_min, a_color_max, drawable.interpolation.z);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_WIDTH_IS_UNIFORM
    stroke_width = mix(a_stroke_width_min, a_stroke_width_max, drawable.interpolation.w);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_COLOR_IS_UNIFORM
    stroke_color = mix(a_stroke_color_min, a_stroke_color_max, drawable.interpolation2.x);
#endif
    vec2 size = max(vec2(0.0), vec2(width, height));
    stroke_width = clamp(stroke_width, 0.0, 0.5 * min(size.x, size.y));
    gl_Position = drawable.matrix * vec4(a_position, 0.0, 1.0);
    gl_Position.xy += vec2(a_corner) * size * 0.5 * drawable.extrude_scale * gl_Position.w;
    applySurfaceTransform();
    v_corner = vec2(a_corner);
    v_size = size;
    v_color = color;
    v_stroke_width = stroke_width;
    v_stroke_color = stroke_color;
}
)SHADER";

constexpr char vkFragment[] = R"SHADER(
layout(location = 0) in vec2 v_corner;
layout(location = 1) in vec2 v_size;
layout(location = 2) in vec4 v_color;
layout(location = 3) in float v_stroke_width;
layout(location = 4) in vec4 v_stroke_color;
layout(location = 0) out vec4 fragColor;

void main() {
    vec2 edge_distance = (vec2(1.0) - abs(v_corner)) * v_size * 0.5;
    fragColor = min(edge_distance.x, edge_distance.y) < v_stroke_width ? v_stroke_color : v_color;
}
)SHADER";

constexpr char metalSource[] = R"SHADER(
struct alignas(16) PluginDrawableUBO {
    float4x4 matrix;
    float2 extrude_scale;
    float2 pad;
    float rectangle_width;
    float rectangle_height;
    float rectangle_stroke_width;
    float property_pad;
    float4 rectangle_color;
    float4 rectangle_stroke_color;
    float4 interpolation;
    float4 interpolation2;
};

struct RectangleVertex {
    short2 position [[attribute(0)]];
    short2 corner [[attribute(1)]];
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_WIDTH_IS_UNIFORM
    float width_min [[attribute(2)]];
    float width_max [[attribute(3)]];
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_HEIGHT_IS_UNIFORM
    float height_min [[attribute(4)]];
    float height_max [[attribute(5)]];
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_COLOR_IS_UNIFORM
    float4 color_min [[attribute(6)]];
    float4 color_max [[attribute(7)]];
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_WIDTH_IS_UNIFORM
    float stroke_width_min [[attribute(8)]];
    float stroke_width_max [[attribute(9)]];
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_COLOR_IS_UNIFORM
    float4 stroke_color_min [[attribute(10)]];
    float4 stroke_color_max [[attribute(11)]];
#endif
};

struct RectangleFragment {
    float4 position [[position, invariant]];
    float2 corner;
    float2 size;
    float4 color;
    float stroke_width;
    float4 stroke_color;
};

RectangleFragment vertex rectangleVertex(
    thread const RectangleVertex vertx [[stage_in]],
    device const PluginDrawableUBO& drawable [[buffer(MLN_PLUGIN_UNIFORM_0_BINDING)]]) {
    float width = drawable.rectangle_width;
    float height = drawable.rectangle_height;
    float4 color = drawable.rectangle_color;
    float strokeWidth = drawable.rectangle_stroke_width;
    float4 strokeColor = drawable.rectangle_stroke_color;
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_WIDTH_IS_UNIFORM
    width = mix(vertx.width_min, vertx.width_max, drawable.interpolation.x);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_HEIGHT_IS_UNIFORM
    height = mix(vertx.height_min, vertx.height_max, drawable.interpolation.y);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_COLOR_IS_UNIFORM
    color = mix(vertx.color_min, vertx.color_max, drawable.interpolation.z);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_WIDTH_IS_UNIFORM
    strokeWidth = mix(vertx.stroke_width_min, vertx.stroke_width_max, drawable.interpolation.w);
#endif
#if !MLN_PLUGIN_PROPERTY_RECTANGLE_STROKE_COLOR_IS_UNIFORM
    strokeColor = mix(vertx.stroke_color_min, vertx.stroke_color_max, drawable.interpolation2.x);
#endif
    float2 size = max(float2(0.0), float2(width, height));
    strokeWidth = clamp(strokeWidth, 0.0, 0.5 * min(size.x, size.y));
    float4 position = drawable.matrix * float4(float2(vertx.position), 0.0, 1.0);
    position.xy += float2(vertx.corner) * size * 0.5 * drawable.extrude_scale * position.w;
    return {position, float2(vertx.corner), size, color, strokeWidth, strokeColor};
}

half4 fragment rectangleFragment(RectangleFragment in [[stage_in]]) {
    float2 edgeDistance = (float2(1.0) - abs(in.corner)) * in.size * 0.5;
    return half4(min(edgeDistance.x, edgeDistance.y) < in.stroke_width ? in.stroke_color : in.color);
}
)SHADER";

struct alignas(16) DrawableUBO {
    float matrix[16];
    float extrudeScale[2];
    float padding[2];
    float rectangleWidth;
    float rectangleHeight;
    float rectangleStrokeWidth;
    float propertyPadding;
    float rectangleColor[4];
    float rectangleStrokeColor[4];
    float interpolation[4];
    float interpolation2[4];
};
static_assert(offsetof(DrawableUBO, rectangleWidth) == 80);
static_assert(offsetof(DrawableUBO, rectangleColor) == 96);
static_assert(offsetof(DrawableUBO, rectangleStrokeColor) == 112);
static_assert(offsetof(DrawableUBO, interpolation) == 128);
static_assert(offsetof(DrawableUBO, interpolation2) == 144);
static_assert(sizeof(DrawableUBO) == 160);

mln_plugin_status updateUniform(const mln_plugin_uniform_context_v1* context,
                                uint32_t uniformID,
                                uint8_t* output,
                                size_t outputSize) {
    if (!context || context->struct_size < sizeof(*context) || !output || uniformID != 0 ||
        outputSize != sizeof(DrawableUBO)) {
        return MLN_PLUGIN_STATUS_INVALID_ARGUMENT;
    }
    DrawableUBO value{};
    std::copy_n(context->tile_matrix, 16, value.matrix);
    value.extrudeScale[0] = context->pixels_to_gl_units[0];
    value.extrudeScale[1] = context->pixels_to_gl_units[1];
    std::memcpy(output, &value, sizeof(value));
    return MLN_PLUGIN_STATUS_OK;
}

constexpr mln_plugin_value makeFloat(float value) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_FLOAT;
    result.data.float_value = value;
    return result;
}

constexpr mln_plugin_value makeColor(float r, float g, float b, float a) {
    mln_plugin_value result{};
    result.struct_size = sizeof(result);
    result.type = MLN_PLUGIN_VALUE_COLOR;
    result.data.color_value = {r, g, b, a};
    return result;
}

const std::array<mln_plugin_property_descriptor_v1, 5> properties = {{
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-color"), MLN_PLUGIN_VALUE_COLOR, makeColor(0, 0, 0, 1), MLN_PLUGIN_EXPRESSION_CAMERA | MLN_PLUGIN_EXPRESSION_FEATURE | MLN_PLUGIN_EXPRESSION_COMPOSITE | MLN_PLUGIN_EXPRESSION_FEATURE_STATE, 1, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-width"), MLN_PLUGIN_VALUE_FLOAT, makeFloat(10), MLN_PLUGIN_EXPRESSION_CAMERA | MLN_PLUGIN_EXPRESSION_FEATURE | MLN_PLUGIN_EXPRESSION_COMPOSITE | MLN_PLUGIN_EXPRESSION_FEATURE_STATE, 1, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-height"), MLN_PLUGIN_VALUE_FLOAT, makeFloat(10), MLN_PLUGIN_EXPRESSION_CAMERA | MLN_PLUGIN_EXPRESSION_FEATURE | MLN_PLUGIN_EXPRESSION_COMPOSITE | MLN_PLUGIN_EXPRESSION_FEATURE_STATE, 1, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-stroke-width"), MLN_PLUGIN_VALUE_FLOAT, makeFloat(0), MLN_PLUGIN_EXPRESSION_CAMERA | MLN_PLUGIN_EXPRESSION_FEATURE | MLN_PLUGIN_EXPRESSION_COMPOSITE | MLN_PLUGIN_EXPRESSION_FEATURE_STATE, 1, 0, 0, 0, 0, nullptr, 0},
    {sizeof(mln_plugin_property_descriptor_v1), str("rectangle-stroke-color"), MLN_PLUGIN_VALUE_COLOR, makeColor(0, 0, 0, 1), MLN_PLUGIN_EXPRESSION_CAMERA | MLN_PLUGIN_EXPRESSION_FEATURE | MLN_PLUGIN_EXPRESSION_COMPOSITE | MLN_PLUGIN_EXPRESSION_FEATURE_STATE, 1, 0, 0, 0, 0, nullptr, 0},
}};

const std::array<mln_plugin_shader_attribute_v1, 12> shaderAttributes = {{
    {sizeof(mln_plugin_shader_attribute_v1), positionAttribute, 0, str("a_position"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), cornerAttribute, 1, str("a_corner"), MLN_PLUGIN_VERTEX_INT16_X2},
    {sizeof(mln_plugin_shader_attribute_v1), widthMinimumAttribute, 2, str("a_width_min"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), widthMaximumAttribute, 3, str("a_width_max"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), heightMinimumAttribute, 4, str("a_height_min"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), heightMaximumAttribute, 5, str("a_height_max"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), colorMinimumAttribute, 6, str("a_color_min"), MLN_PLUGIN_VERTEX_FLOAT_X4},
    {sizeof(mln_plugin_shader_attribute_v1), colorMaximumAttribute, 7, str("a_color_max"), MLN_PLUGIN_VERTEX_FLOAT_X4},
    {sizeof(mln_plugin_shader_attribute_v1), strokeWidthMinimumAttribute, 8, str("a_stroke_width_min"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), strokeWidthMaximumAttribute, 9, str("a_stroke_width_max"), MLN_PLUGIN_VERTEX_FLOAT},
    {sizeof(mln_plugin_shader_attribute_v1), strokeColorMinimumAttribute, 10, str("a_stroke_color_min"), MLN_PLUGIN_VERTEX_FLOAT_X4},
    {sizeof(mln_plugin_shader_attribute_v1), strokeColorMaximumAttribute, 11, str("a_stroke_color_max"), MLN_PLUGIN_VERTEX_FLOAT_X4},
}};

const std::array<mln_plugin_shader_source_v1, 3> shaderSources = {{
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_OPENGL, str(glVertex), str(glFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_VULKAN, str(vkVertex), str(vkFragment), {}, {}},
    {sizeof(mln_plugin_shader_source_v1), MLN_PLUGIN_BACKEND_METAL, str(metalSource), {}, str("rectangleVertex"), str("rectangleFragment")},
}};

const std::array<mln_plugin_uniform_block_descriptor_v1, 1> shaderUniforms = {{
    {sizeof(mln_plugin_uniform_block_descriptor_v1),
     0,
     str("PluginDrawableUBO"),
     sizeof(DrawableUBO),
     MLN_PLUGIN_SHADER_STAGE_VERTEX,
     MLN_PLUGIN_UNIFORM_DRAWABLE},
}};

const std::array<mln_plugin_shader_property_binding_v1, 5> propertyBindings = {{
    {sizeof(mln_plugin_shader_property_binding_v1), str("rectangle-width"), MLN_PLUGIN_PROPERTY_ENCODING_FLOAT, 0, offsetof(DrawableUBO, rectangleWidth), widthMinimumAttribute, widthMaximumAttribute, 0, offsetof(DrawableUBO, interpolation) + 0 * sizeof(float)},
    {sizeof(mln_plugin_shader_property_binding_v1), str("rectangle-height"), MLN_PLUGIN_PROPERTY_ENCODING_FLOAT, 0, offsetof(DrawableUBO, rectangleHeight), heightMinimumAttribute, heightMaximumAttribute, 0, offsetof(DrawableUBO, interpolation) + 1 * sizeof(float)},
    {sizeof(mln_plugin_shader_property_binding_v1), str("rectangle-color"), MLN_PLUGIN_PROPERTY_ENCODING_COLOR, 0, offsetof(DrawableUBO, rectangleColor), colorMinimumAttribute, colorMaximumAttribute, 0, offsetof(DrawableUBO, interpolation) + 2 * sizeof(float)},
    {sizeof(mln_plugin_shader_property_binding_v1), str("rectangle-stroke-width"), MLN_PLUGIN_PROPERTY_ENCODING_FLOAT, 0, offsetof(DrawableUBO, rectangleStrokeWidth), strokeWidthMinimumAttribute, strokeWidthMaximumAttribute, 0, offsetof(DrawableUBO, interpolation) + 3 * sizeof(float)},
    {sizeof(mln_plugin_shader_property_binding_v1), str("rectangle-stroke-color"), MLN_PLUGIN_PROPERTY_ENCODING_COLOR, 0, offsetof(DrawableUBO, rectangleStrokeColor), strokeColorMinimumAttribute, strokeColorMaximumAttribute, 0, offsetof(DrawableUBO, interpolation2)},
}};

const std::array<mln_plugin_shader_descriptor_v1, 1> shaders = {{
    {sizeof(mln_plugin_shader_descriptor_v1),
     str("rectangle"),
     shaderSources.data(),
     shaderSources.size(),
     shaderAttributes.data(),
     shaderAttributes.size(),
     shaderUniforms.data(),
     shaderUniforms.size(),
     propertyBindings.data(),
     propertyBindings.size()},
}};

const mln_plugin_layer_type_v1 layerType = [] {
    mln_plugin_layer_type_v1 value{};
    value.struct_size = sizeof(value);
    value.layer_type = str("rectangle");
    value.backend_mask = MLN_PLUGIN_BACKEND_OPENGL | MLN_PLUGIN_BACKEND_VULKAN | MLN_PLUGIN_BACKEND_METAL;
    value.properties = properties.data();
    value.property_count = properties.size();
    value.geometry_type_mask = MLN_PLUGIN_GEOMETRY_POINT;
    value.shaders = shaders.data();
    value.shader_count = shaders.size();
    value.create_layout = createLayout;
    value.layout_feature = layoutFeature;
    value.finish_layout = finishLayout;
    value.destroy_layout = destroyLayout;
    value.query_feature = queryFeature;
    value.update_uniform_block = updateUniform;
    value.get_query_radius = queryRadius;
    return value;
}();

const mln_plugin_descriptor_v1 descriptor = {
    sizeof(mln_plugin_descriptor_v1),
    MLN_PLUGIN_ABI_VERSION_1,
    str("org.maplibre.rectangle-layer"),
    str(MLN_RECTANGLE_PLUGIN_VERSION, sizeof(MLN_RECTANGLE_PLUGIN_VERSION) - 1),
    MLN_PLUGIN_ABI_VERSION_1,
    MLN_PLUGIN_ABI_VERSION_1,
    &layerType,
    1,
};

} // namespace

extern "C" mln_plugin_status mln_rectangle_layer_register(mln_plugin_register_function_v1 registerPlugin,
                                                            char* errorMessage,
                                                            size_t errorMessageCapacity) {
    if (!registerPlugin) return MLN_PLUGIN_STATUS_NOT_FOUND;
    return registerPlugin(&descriptor, errorMessage, errorMessageCapacity);
}
