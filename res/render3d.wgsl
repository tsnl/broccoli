const PI: f32 = 3.14159265;
const INSTANCE_COUNT: u32 = 1024;
const DIRECTIONAL_LIGHT_COUNT: u32 = 4;
const POINT_LIGHT_COUNT: u32 = 16;

struct LightUniform {
  directional_light_dir_array: array<vec4<f32>, DIRECTIONAL_LIGHT_COUNT>,
  directional_light_color_array: array<vec4<f32>, DIRECTIONAL_LIGHT_COUNT>,
  point_light_pos_array: array<vec4<f32>, POINT_LIGHT_COUNT>,
  point_light_color_array: array<vec4<f32>, POINT_LIGHT_COUNT>,
  directional_light_count: u32,
  point_light_count: u32,
  ambient_glow: f32,
  rsv0: u32,
  rsv1: array<vec4<u32>, 23>,
}
struct CameraUniform {
  view_matrix: mat4x4<f32>,
  world_position: vec4<f32>,
  camera_cot_half_fovy: f32,
  camera_aspect_inv: f32,
  camera_zmin: f32,
  camera_zmax: f32,
  camera_logarithmic_z_scale: f32,
  rsv00: u32,
  rsv01: u32,
  rsv02: u32,
  rsv03: u32,
  rsv04: u32,
  rsv05: u32,
  rsv06: u32,
}
struct VertexInput {
  @builtin(vertex_index) vertex_index: u32,
  @builtin(instance_index) instance_index: u32,
  @location(0) raw_position: vec4<i32>,
  @location(1) raw_color: vec4<u32>,
  @location(2) raw_normal: vec4<f32>,
};
struct FragmentInput {
  @builtin(position) clip_position: vec4<f32>,
  @location(0) @interpolate(linear) world_position: vec4<f32>,
  @location(1) @interpolate(linear) color: vec4<f32>,
  @location(2) @interpolate(linear) world_normal: vec4<f32>,
  @location(3) @interpolate(flat) instance_index: u32,
};

@group(0) @binding(0) var<uniform> u_camera: CameraUniform;
@group(0) @binding(1) var<uniform> u_light: LightUniform;
@group(0) @binding(2) var<uniform> u_model_mats: array<mat4x4<f32>, INSTANCE_COUNT>;

@vertex
fn vs_main(vertex_input: VertexInput) -> FragmentInput {
  let position = vec3<f32>(vertex_input.raw_position.xyz) / 16.0;
  let color = vec3<f32>(vertex_input.raw_color.xyz) / 255.0;
  let shininess = f32(vertex_input.raw_color.w) / 255.0;
  let normal = 2.0 * vertex_input.raw_normal.xyz - 1.0;

  let model_matrix = u_model_mats[vertex_input.instance_index];

  // NOTE: to evaluate 'world_normal', we must recall that 'model_matrix' may translate as well as perform a linear
  // transform. We can get the linear transformation to the normal by subtracting out the image of (0, 0, 0, 1): by
  // figuring out where the model matrix transforms (0, 0, 0), we can eliminate all 'translation' effects. This is
  // equivalent to finding the image of (0, 0, 0, 1), i.e. the 'w' basis vector: hence, model_matrix[3].
  let world_position = model_matrix * vec4(position, 1.0);
  let world_normal = normalize((model_matrix * vec4(normal, 1.0) - model_matrix[3]).xyz);

  let cam_position = u_camera.view_matrix * world_position;

  var fragment_input: FragmentInput;
  fragment_input.clip_position = perspective_projection(cam_position);
  fragment_input.world_position = world_position;
  fragment_input.color = vec4(color, shininess);
  fragment_input.world_normal = vec4(world_normal, 1.0f);
  fragment_input.instance_index = vertex_input.instance_index;
  return fragment_input;
}

@fragment
fn fs_main(fragment_input: FragmentInput) -> @location(0) vec4f {
  var result = blinn_phong_ambient(fragment_input, u_light.ambient_glow).xyz;
  for (var i = 0u; i < u_light.directional_light_count; i += 1) {
    let light_dir = u_light.directional_light_dir_array[i].xyz;
    let light_color = u_light.directional_light_color_array[i].xyz;
    result = clamp(result + blinn_phong_dir(fragment_input, light_dir, light_color), vec3(0.0), vec3(1.0));
  }
  for (var i = 0u; i < u_light.point_light_count; i += 1) {
    let light_pos = u_light.point_light_pos_array[i].xyz;
    let light_color = u_light.point_light_color_array[i].xyz;
    result = clamp(result + blinn_phong_pt(fragment_input, light_pos, light_color), vec3(0.0), vec3(1.0));
  }
  return vec4(result, 1.0);
}

/// A perspective projection designed to produce...
/// - projected coordinates in X and Y coordinates
/// - a logarithmic Z distribution for Z to reduce Z-fighting
/// - 1.0f in the W slot.
/// Furthermore, the output Z axis value is always positive, which ensures we conform to WebGPU's
/// 'left-handed' clip-space coordinate system, assuming +Z points away from the screen instead of 
/// towards as is assumed in a right-handed system. 
/// I got fed up with the perspective projection matrix, so derived this myself.
/// See: https://www.gamedeveloper.com/programming/logarithmic-depth-buffer
/// NOTE: affine texture warping is an issue that relies on the W component holding distance to automatically correct
/// for texture warping. TLDR, when interpolating UVs, texcoords are parallel to triangle edges, causing 'skew' in 
/// opposite directions.
/// See: https://danielilett.com/2021-11-06-tut5-21-ps1-affine-textures/
fn perspective_projection(in: vec4<f32>) -> vec4<f32> {
  let c = u_camera.camera_logarithmic_z_scale;
  return vec4<f32>(
    in.x * u_camera.camera_cot_half_fovy * u_camera.camera_aspect_inv,
    in.y * u_camera.camera_cot_half_fovy,
    -in.z * log2(c * (-in.z - u_camera.camera_zmin) + 1.0f) / log2(c * (u_camera.camera_zmax - u_camera.camera_zmin) + 1.0f),
    -in.z
  );
}

// Blinn-Phong
// see: https://learnopengl.com/Advanced-Lighting/Advanced-Lighting
// see: https://www.rorydriscoll.com/2009/01/25/energy-conservation-in-games/
fn blinn_phong_dir(in: FragmentInput, light_dir: vec3<f32>, light_color: vec3<f32>) -> vec3<f32> {
  return
    blinn_phong_specular_dir(in, light_dir, light_color) +
    blinn_phong_diffuse_dir(in, light_dir, light_color) +
    vec3<f32>(0.0);
}
fn blinn_phong_pt(in: FragmentInput, light_pos: vec3<f32>, light_color: vec3<f32>) -> vec3<f32> {
  return
    blinn_phong_specular_pt(in, light_pos, light_color) +
    blinn_phong_diffuse_pt(in, light_pos, light_color) +
    vec3<f32>(0.0);
}
fn blinn_phong_specular_dir(in: FragmentInput, light_dir: vec3<f32>, light_color: vec3<f32>) -> vec3<f32> {
  let shininess = in.color.w;
  let normal = in.world_normal.xyz;
  let view_pos = u_camera.world_position.xyz;
  let frag_pos = in.world_position.xyz;
  let view_dir: vec3<f32> = normalize(view_pos - frag_pos);
  let halfway_dir: vec3<f32> = normalize(-light_dir + view_dir);
  let k_energy_conservation = (8.0 + shininess) / (8.0 * PI);
  let spec: f32 = k_energy_conservation * pow(max(dot(normal, halfway_dir), 0.0), shininess);
  return light_color * spec;
}
fn blinn_phong_specular_pt(in: FragmentInput, light_pos: vec3<f32>, light_color: vec3<f32>) -> vec3<f32> {
  let shininess = in.color.w;
  let normal = in.world_normal.xyz;
  let view_pos = u_camera.world_position.xyz;
  let frag_pos = in.world_position.xyz;
  let light_dir: vec3<f32> = normalize(light_pos - frag_pos);
  let view_dir: vec3<f32> = normalize(view_pos - frag_pos);
  let halfway_dir: vec3<f32> = normalize(light_dir + view_dir);
  let k_energy_conservation = (8.0 + shininess) / (8.0 * PI);
  let spec: f32 = k_energy_conservation * pow(max(dot(normal, halfway_dir), 0.0), shininess);
  return light_color * spec;
}
fn blinn_phong_diffuse_dir(in: FragmentInput, light_dir: vec3<f32>, light_color: vec3<f32>) -> vec3<f32> {
  let base_color = in.color.xyz;
  let normal = in.world_normal.xyz;
  let strength = clamp(dot(normal, -light_dir), 0.0f, 1.0f);
  return strength * base_color;
}
fn blinn_phong_diffuse_pt(in: FragmentInput, light_pos: vec3<f32>, light_color: vec3<f32>) -> vec3<f32> {
  let frag_pos = in.world_position.xyz;
  let light_dir: vec3<f32> = normalize(light_pos - frag_pos);
  return blinn_phong_diffuse_dir(in, light_color, light_pos);
}
fn blinn_phong_ambient(in: FragmentInput, ambient_glow: f32) -> vec3<f32> {
  return ambient_glow * in.color.xyz;
}
