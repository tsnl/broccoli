override INSTANCE_COUNT: u32 = 1024;

struct CameraUniform {
  view_matrix: mat4x4<f32>,
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
  rsv07: u32,
  rsv08: u32,
  rsv09: u32,
  rsv10: u32,
}
struct VertexInput {
  @builtin(vertex_index) vertex_index: u32,
  @builtin(instance_index) instance_index: u32,
  @location(0) raw_position: vec4<f32>,
  @location(1) color: vec4<f32>,
  @location(2) raw_normal: vec4<f32>,
};
struct FragmentInput {
  @builtin(position) clip_position: vec4<f32>,
  @location(0) @interpolate(linear) world_position: vec4<f32>,
  @location(1) @interpolate(linear) color: vec4<f32>,
  @location(2) @interpolate(linear) cam_normal: vec4<f32>,
  @location(3) @interpolate(linear) world_normal: vec4<f32>,
  @location(4) @interpolate(flat) instance_index: u32,
};

@group(0) @binding(0) var<uniform> u_camera: CameraUniform;
@group(0) @binding(1) var<uniform> u_model_mats: array<mat4x4<f32>, INSTANCE_COUNT>;

@vertex
fn vs_main(vertex_input: VertexInput) -> FragmentInput {
  let position = vertex_input.raw_position.xyz / 1024.0;
  let normal = 2.0 * vertex_input.raw_normal.xyz - vec3(1.0);

  let model_matrix = u_model_mats[vertex_input.instance_index];

  let world_position = model_matrix * vec4<f32>(position, 1.0);
  let world_normal = normalize(model_matrix * vec4<f32>(normal, 1.0)).xyz;

  let cam_position = u_camera.view_matrix * world_position;
  let cam_normal = u_camera.view_matrix * world_normal;

  var fi: FragmentInput;
  fi.clip_position = perspective_projection(cam_position);
  fi.world_position = world_position;
  fi.color = vertex_input.color;
  fi.cam_normal = vec4<f32>(cam_normal, 1.0f);
  fi.world_normal = vec4<f32>(world_normal, 1.0f);
  fi.instance_index = in.instance_index;
  return fi;
}

@fragment
fn fs_main(fragment_input: FragmentInput) -> @location(0) vec4f {
  return fragment_input.color;
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
    +in.x * u_camera.camera_cot_half_fovy * u_camera.camera_aspect_inv,
    +in.y * u_camera.camera_cot_half_fovy,
    -in.z * log2(c * (-in.z - u_camera.camera_zmin) + 1.0f) / log2(c * (u_camera.camera_zmax - u_camera.camera_zmin) + 1.0f),
    -in.z
  );
}
