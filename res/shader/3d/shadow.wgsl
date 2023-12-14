// Parameters:
// - p_INSTANCE_COUNT: u32 => the maximum instance count drawn

struct ShadowUniform {
  proj_view_matrix: mat4x4<f32>,
}
struct VertexInput {
  @builtin(vertex_index) vertex_index: u32,
  @builtin(instance_index) instance_index: u32,
  @location(0) raw_position: vec3<i32>,
};
struct FragmentInput {
  @builtin(position) clip_position: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_model_mats: array<mat4x4<f32>, p_INSTANCE_COUNT>;
@group(1) @binding(0) var<uniform> u_shadow: ShadowUniform;

@vertex
fn vertexShaderMain(vertex_input: VertexInput) -> FragmentInput {
  let position = unpackPosition(vertex_input.raw_position);
  let model_matrix = u_model_mats[vertex_input.instance_index];
  let world_position = (model_matrix * vec4(position, 1.0)).xyz;
  let clip_position = u_shadow.proj_view_matrix * vec4(world_position, 1.0);
  var fi: FragmentInput;
  fi.clip_position = clip_position;
  return fi;
}

@fragment
fn fragmentShaderMain(in: FragmentInput) -> @location(0) vec4<f32> {
  return vec4(0.0);
}

//
// Utility:
//

// FIXME: this is copy-pasted from 'ubershader'

fn unpackPosition(raw_position: vec3<i32>) -> vec3<f32> {
  let position_lo = vec3<f32>(
    f32(raw_position.x & 0xFFFFF) / 1048576.0,
    f32(raw_position.y & 0xFFFFF) / 1048576.0,
    f32(raw_position.z & 0xFFFFF) / 1048576.0,
  );
  let position_hi = vec3<f32>(
    f32(raw_position.x >> 20),
    f32(raw_position.y >> 20),
    f32(raw_position.z >> 20),
  );
  return position_hi + position_lo;
}
