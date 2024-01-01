// Parameters:
// - p_LIGHTING_MODEL: u32 => whether to use Blinn-Phong or PBR
// - p_INSTANCE_COUNT: u32 => the maximum instance count drawn
// - p_DIRECTIONAL_LIGHT_COUNT: u32 => the maximum directional lights drawn
// - p_POINT_LIGHT_COUNT: u32 => the maximum point lights drawn
// - p_DIR_LIGHT_CASCADE_COUNT: u32 => the number of cascades for cascaded shadow maps for all directional lights
// - p_DIR_LIGHT_SHADOW_RADIUS: f32 => what '1.0' in shadow map distance corresponds to

const PI: f32 = 3.14159265;

struct Rectf {
  min: vec2f,
  max: vec2f
}
struct LightUniformCore {
  directional_light_dir_array: array<vec4<f32>, p_DIRECTIONAL_LIGHT_COUNT>,
  directional_light_color_array: array<vec4<f32>, p_DIRECTIONAL_LIGHT_COUNT>,
  point_light_pos_array: array<vec4<f32>, p_POINT_LIGHT_COUNT>,
  point_light_color_array: array<vec4<f32>, p_POINT_LIGHT_COUNT>,
  directional_light_count: u32,
  point_light_count: u32,
  ambient_glow: f32,
  _rsv0: u32,
}
struct LightUniformShadow {
  dir_csm_proj_view_mats: array<array<mat4x4f, p_DIR_LIGHT_CASCADE_COUNT>, p_DIRECTIONAL_LIGHT_COUNT>,
  dir_csm_xy_bounds: array<array<Rectf, p_DIR_LIGHT_CASCADE_COUNT>, p_DIRECTIONAL_LIGHT_COUNT>,
}
struct LightUniform {
  core: LightUniformCore,
  shadow: LightUniformShadow,
}
struct CameraUniform {
  view_matrix: mat4x4<f32>,
  world_position: vec4<f32>,
  camera_cot_half_fovy: f32,
  camera_aspect_inv: f32,
  camera_zmin: f32,
  camera_zmax: f32,
  camera_logarithmic_z_scale: f32,
  hdr_exposure_bias: f32,
  rsv00: u32,
  rsv01: u32,
  rsv02: u32,
  rsv03: u32,
  rsv04: u32,
  rsv05: u32,
}
struct MaterialUniform {
  albedo_uv_offset: vec2<f32>,
  albedo_uv_size: vec2<f32>,
  normal_uv_offset: vec2<f32>,
  normal_uv_size: vec2<f32>,
  roughness_uv_offset: vec2<f32>,
  roughness_uv_size: vec2<f32>,
  metalness_uv_offset: vec2<f32>,
  metalness_uv_size: vec2<f32>,
  pbr_fresnel0: vec4<f32>,
  blinn_phong_shininess: f32,
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
  @location(0) raw_position: vec3<i32>,
  @location(1) raw_normal: vec4<f32>,
  @location(2) raw_tangent: vec4<f32>,
  @location(3) raw_uv: vec2<f32>,
};
struct FragmentInput {
  @builtin(position) clip_position: vec4<f32>,
  @location(0) @interpolate(linear) world_position: vec4<f32>,
  @location(1) @interpolate(linear) world_normal: vec4<f32>,
  @location(2) @interpolate(linear) world_tangent: vec4<f32>,
  @location(3) @interpolate(linear) world_bitangent: vec4<f32>,
  @location(4) @interpolate(linear) uv: vec2<f32>,
  @location(5) @interpolate(flat) instance_index: u32,
};

@group(0) @binding(0) var<uniform> u_camera: CameraUniform;
@group(0) @binding(1) var<uniform> u_light: LightUniform;
@group(0) @binding(2) var<uniform> u_model_mats: array<mat4x4<f32>, p_INSTANCE_COUNT>;
@group(0) @binding(3) var dir_light_csm_texture_array: texture_2d_array<f32>;
@group(0) @binding(4) var dir_light_csm_texture_sampler: sampler;

@group(1) @binding(0) var<uniform> u_material: MaterialUniform;
@group(1) @binding(1) var albedo_texture: texture_2d<f32>;
@group(1) @binding(2) var normal_texture: texture_2d<f32>;
@group(1) @binding(3) var metalness_texture: texture_2d<f32>;
@group(1) @binding(4) var roughness_texture: texture_2d<f32>;
@group(1) @binding(5) var albedo_texture_sampler: sampler;
@group(1) @binding(6) var normal_texture_sampler: sampler;
@group(1) @binding(7) var metalness_texture_sampler: sampler;
@group(1) @binding(8) var roughness_texture_sampler: sampler;

@vertex
fn vertexShaderMain(vertex_input: VertexInput) -> FragmentInput {
  
  let position = unpackPosition(vertex_input.raw_position);
  let normal = 2.0 * vertex_input.raw_normal.xyz - 1.0;
  let tangent = 2.0 * vertex_input.raw_tangent.xyz - 1.0;

  let model_matrix = u_model_mats[vertex_input.instance_index];

  // NOTE: to evaluate 'world_normal', we must recall that 'model_matrix' may translate as well as perform a linear
  // transform. We can get the linear transformation to the normal by subtracting out the image of (0, 0, 0, 1): by
  // figuring out where the model matrix transforms (0, 0, 0), we can eliminate all 'translation' effects. This is
  // equivalent to finding the image of (0, 0, 0, 1), i.e. the 'w' basis vector: hence, model_matrix[3].
  // A more efficient way to do this is to simply set the normal as vec4(normal, 0.0), thereby ignoring all translation.
  let world_position = (model_matrix * vec4(position, 1.0)).xyz;
  let world_normal = (model_matrix * vec4(normal, 0.0)).xyz;
  let world_tangent = (model_matrix * vec4(tangent, 0.0)).xyz;
  let world_bitangent = cross(world_normal, world_tangent);

  let cam_position = u_camera.view_matrix * vec4(world_position, 1.0);

  var in: FragmentInput;
  in.clip_position = perspectiveProjection(cam_position);
  in.world_position = vec4(world_position, 1.0);
  in.world_normal = vec4(world_normal, 1.0);
  in.world_tangent = vec4(world_tangent, 1.0);
  in.world_bitangent = vec4(world_bitangent, 1.0);
  in.instance_index = vertex_input.instance_index;
  return in;
}
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

@fragment
fn fragmentShaderMain(in: FragmentInput) -> @location(0) vec4f {
  // return vec4(computeNormal(in.world_normal.xyz, in.world_tangent.xyz, in.world_bitangent.xyz, in.uv), 1.0);
  switch (p_LIGHTING_MODEL) {
    case 1: { return blinnPhongMain(in); }
    case 2: { return pbrMain(in); }
    default: { return vec4<f32>(1.0, 0.0, 1.0, 1.0); }
  }
}

fn albedoUv(uv: vec2<f32>) -> vec2<f32> {
  return computeVirtualUv(uv, u_material.albedo_uv_offset, u_material.albedo_uv_size);
}
fn normalUv(uv: vec2<f32>) -> vec2<f32> {
  return computeVirtualUv(uv, u_material.normal_uv_offset, u_material.normal_uv_size);
}
fn metalnessUv(uv: vec2<f32>) -> vec2<f32> {
  return computeVirtualUv(uv, u_material.metalness_uv_offset, u_material.metalness_uv_size);
}
fn roughnessUv(uv: vec2<f32>) -> vec2<f32> {
  return computeVirtualUv(uv, u_material.roughness_uv_offset, u_material.roughness_uv_size);
}
fn computeVirtualUv(uv: vec2<f32>, offset: vec2<f32>, size: vec2<f32>) -> vec2<f32> {
  return offset + uv * size;
}

fn computeNormal(normal: vec3<f32>, tangent: vec3<f32>, bitangent: vec3<f32>, uv: vec2<f32>) -> vec3<f32> {
  // FIXME: this transform should also take into account if the transform scale is non-uniform, see LearnOpenGL for the 
  // "normal matrix".
  // NOTE: because WebGPU uses the top-left corner for UV coordinates, this means the TNB matrix space is left-handed
  // if +Z points outward. For now, we use Collada TNB matrix space, converting UV lookups in the shader. This means the
  // texture coordinates (UV) used to define the tangent and bitangent are different than those used to query textures:
  // the V axis is inverted.
  let is_tangent_zero = dot(tangent, tangent) <= 1e-5f;
  let sample = textureSampleLevel(normal_texture, normal_texture_sampler, normalUv(uv), 0.0).xyz;
  if is_tangent_zero {
    return normal;
  } else {
    let tbn = mat3x3(tangent, bitangent, normal);
    return normalize(tbn * (2.0 * sample - 1.0));
  }
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
fn perspectiveProjection(in: vec4<f32>) -> vec4<f32> {
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
fn blinnPhongMain(in: FragmentInput) -> vec4<f32> {
  let albedo = textureSampleLevel(albedo_texture, albedo_texture_sampler, albedoUv(in.uv), 0.0).xyz;
  let normal = computeNormal(in.world_normal.xyz, in.world_tangent.xyz, in.world_bitangent.xyz, in.uv);
  let position = in.world_position.xyz;
  var result = blinnPhongAmbient(albedo, u_light.core.ambient_glow).xyz;
  for (var i = 0u; i < u_light.core.directional_light_count; i += 1) {
    let light_dir = u_light.core.directional_light_dir_array[i].xyz;
    let light_color = u_light.core.directional_light_color_array[i].xyz;
    result = clamp(
      result + blinnPhongDir(albedo, normal, position, light_dir, light_color),
      vec3(0.0), vec3(1.0)
    );
  }
  for (var i = 0u; i < u_light.core.point_light_count; i += 1) {
    let light_pos = u_light.core.point_light_pos_array[i].xyz;
    let light_color = u_light.core.point_light_color_array[i].xyz;
    result = clamp(
      result + blinnPhongPt(albedo, normal, position, light_pos, light_color),
      vec3(0.0), vec3(1.0)
    );
  }
  return vec4(result, 1.0);
}
fn blinnPhongDir(
  albedo: vec3<f32>, world_normal: vec3<f32>, world_position: vec3<f32>,
  light_dir: vec3<f32>, light_color: vec3<f32>
) -> vec3<f32> {
  return
    blinnPhongSpecularDir(world_normal, world_position, light_dir, light_color) +
    blinnPhongDiffuseDir(albedo, world_normal, light_dir, light_color) +
    vec3<f32>(0.0);
}
fn blinnPhongSpecularDir(
  world_normal: vec3<f32>, world_position: vec3<f32>,
  light_dir: vec3<f32>, light_color: vec3<f32>
) -> vec3<f32> {
  let shininess = u_material.blinn_phong_shininess;
  let normal = world_normal;
  let view_pos = u_camera.world_position.xyz;
  let frag_pos = world_position;
  let view_dir: vec3<f32> = normalize(view_pos - frag_pos);
  let halfway_dir: vec3<f32> = normalize(-light_dir + view_dir);
  let k_energy_conservation = (8.0 + shininess) / (8.0 * PI);
  let spec: f32 = k_energy_conservation * pow(max(dot(normal, halfway_dir), 0.0), shininess);
  return light_color * spec;
}
fn blinnPhongDiffuseDir(
  albedo: vec3<f32>, world_normal: vec3<f32>,
  light_dir: vec3<f32>, light_color: vec3<f32>
) -> vec3<f32> {
  let base_color = albedo;
  let normal = world_normal.xyz;
  let strength = clamp(dot(normal, -light_dir), 0.0f, 1.0f);
  return strength * base_color;
}
fn blinnPhongPt(
  albedo: vec3<f32>, world_normal: vec3<f32>, world_position: vec3<f32>,
  light_pos: vec3<f32>, light_color: vec3<f32>
) -> vec3<f32> {
  return
    blinnPhongSpecularPt(world_normal, world_position, light_pos, light_color) +
    blinnPhongDiffusePt(albedo, world_normal, world_position, light_pos, light_color) +
    vec3<f32>(0.0);
}
fn blinnPhongSpecularPt(
  world_normal: vec3<f32>, world_position: vec3<f32>,
  light_pos: vec3<f32>, light_color: vec3<f32>
) -> vec3<f32> {
  let shininess = u_material.blinn_phong_shininess;
  let normal = world_normal;
  let view_pos = u_camera.world_position.xyz;
  let frag_pos = world_position;
  let light_dir: vec3<f32> = normalize(light_pos - frag_pos);
  let view_dir: vec3<f32> = normalize(view_pos - frag_pos);
  let halfway_dir: vec3<f32> = normalize(light_dir + view_dir);
  let k_energy_conservation = (8.0 + shininess) / (8.0 * PI);
  let spec: f32 = k_energy_conservation * pow(max(dot(normal, halfway_dir), 0.0), shininess);
  return light_color * spec;
}
fn blinnPhongDiffusePt(
  albedo: vec3<f32>, world_normal: vec3<f32>, world_position: vec3<f32>,
  light_pos: vec3<f32>, light_color: vec3<f32>
) -> vec3<f32> {
  let frag_pos = world_position;
  let light_dir: vec3<f32> = normalize(light_pos - frag_pos);
  return blinnPhongDiffuseDir(albedo, world_normal, light_color, light_pos);
}
fn blinnPhongAmbient(albedo: vec3<f32>, ambient_glow: f32) -> vec3<f32> {
  return ambient_glow * albedo;
}

// see: https://learnopengl.com/PBR/Lighting
fn pbrMain(in: FragmentInput) -> vec4<f32> {
  let exposure_bias = u_camera.hdr_exposure_bias;
  let pbr_ao = 1.0f;
  let fresnel0 = u_material.pbr_fresnel0.xyz;
  let albedo = textureSampleLevel(albedo_texture, albedo_texture_sampler, albedoUv(in.uv), 0.0).xyz;
  let metalness = textureSampleLevel(metalness_texture, metalness_texture_sampler, metalnessUv(in.uv), 0.0).x;
  let roughness = textureSampleLevel(roughness_texture, roughness_texture_sampler, roughnessUv(in.uv), 0.0).x;
  let normal = computeNormal(in.world_normal.xyz, in.world_tangent.xyz, in.world_bitangent.xyz, in.uv);
  let position = in.world_position.xyz;

  var lo = vec3<f32>(0.0);
  var i: u32;
  for (i = 0u; i < u_light.core.directional_light_count; i++) {
    // let is_lit = dirLightShadowMapSample(i, position);
    // if (is_lit == 0) {
    //   continue;
    // }

    // DEBUG:
    return dirLightShadowMapSample(i, position);
    
    let light_dir = -u_light.core.directional_light_dir_array[i].xyz;
    let light_color = u_light.core.directional_light_color_array[i].xyz;
    lo += pbrDir(position, fresnel0, albedo, normal, metalness, roughness, light_dir, light_color, 1.0);
  }
  for (i = 0u; i < u_light.core.point_light_count; i++) {
    let light_pos = u_light.core.point_light_pos_array[i].xyz;
    let light_color = u_light.core.point_light_color_array[i].xyz;
    lo += pbrPt(position, fresnel0, albedo, normal, metalness, roughness, light_pos, light_color);
  }
  let ambient = vec3<f32>(u_light.core.ambient_glow) * albedo * pbr_ao;
  let color_hdr = ambient + lo;
  return vec4<f32>(naughtyDogTonemap(color_hdr.xyz, exposure_bias), 1.0f);
}
fn pbrPt(
  world_position: vec3<f32>,
  fresnel0: vec3<f32>,
  albedo: vec3<f32>, normal: vec3<f32>, metalness: f32, roughness: f32,
  light_pos: vec3<f32>, light_color: vec3<f32>
) -> vec3<f32> {
  let frag_pos = world_position;
  let light_dir: vec3<f32> = normalize(light_pos - frag_pos);
  let distance = length(light_pos - frag_pos);
  let attenuation = 1.0f / (distance * distance);
  return pbrDir(world_position, fresnel0, albedo, normal, metalness, roughness, light_dir, light_color, attenuation);
}
fn pbrDir(
  world_position: vec3<f32>,
  fresnel0: vec3<f32>,
  albedo: vec3<f32>, normal: vec3<f32>, metalness: f32, roughness: f32,
  light_dir: vec3<f32>, light_color: vec3<f32>,
  attenuation: f32
) -> vec3<f32> {
  let view_pos = u_camera.world_position.xyz;
  let frag_pos = world_position;
  let view_dir: vec3<f32> = normalize(view_pos - frag_pos);
  let halfway_dir: vec3<f32> = normalize(light_dir + view_dir);
  
  let f0 = mix(fresnel0, albedo, metalness); 

  let radiance = light_color * attenuation; 

  let ndf = distributionGGX(normal, halfway_dir, roughness);       
  let g = geometrySmith(normal, view_dir, light_dir, roughness);
  let f = fresnelSchlick(max(dot(halfway_dir, view_dir), 0.0), f0);

  let ks = f;
  let kd = (vec3<f32>(1.0f) - ks) * (1.0f - metalness);
  
  let numerator: vec3<f32> = ndf * g * f;
  let denominator: f32 = 4.0f * max(dot(normal, view_dir), 0.0f) * max(dot(normal, light_dir), 0.0f) + 1e-4f;
  let specular = numerator / denominator;

  let nl = max(dot(normal, light_dir), 0.0f);
  return (kd * albedo / PI + specular) * radiance * nl;
}
fn geometrySmith(n: vec3<f32>, v: vec3<f32>, l: vec3<f32>, roughness: f32) -> f32 {
  let nv = max(dot(n, v), 0.0);
  let nl = max(dot(n, l), 0.0);
  let ggx1 = geometrySchlickGGX(nl, roughness);
  let ggx2 = geometrySchlickGGX(nv, roughness);
  return ggx1 * ggx2;
}
fn geometrySchlickGGX(nv: f32, roughness: f32) -> f32 {
  let r = roughness + 1.0f;
  let k = (r * r) / 8.0f;
  let num = nv;
  let denom = nv * (1.0 - k) + k;
  return num / denom;
}
fn distributionGGX(n: vec3<f32>, h: vec3<f32>, roughness: f32) -> f32 {
  let a = roughness * roughness;
  let a_squared = a * a;
  let nh = max(dot(n, h), 0.0);
  let nh_squared = nh * nh;
  let numerator = a_squared;
  let denominator_term = nh_squared * (a_squared - 1.0f) + 1.0f;
  let denominator = PI * denominator_term * denominator_term;
  return numerator / denominator;
}
fn fresnelSchlick(cos_theta: f32, f0: vec3<f32>) -> vec3<f32> {
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}
fn reinhardTonemap(color0: vec3<f32>) -> vec3<f32> {
  let color1 = color0 / (color0 + vec3(1.0));
  let color2 = pow(color1, vec3(1.0/2.2));
  return color2;
}
fn naughtyDogTonemap(x: vec3<f32>, exposure_bias: f32) -> vec3<f32> {
  // Naughty Dog filmic tonemapping operator.
  // See: http://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/
  // See: http://duikerresearch.com/2015/09/filmic-tonemapping-for-real-time-rendering/
  // 'exposure_bias' is an exposure quantity used to control exposure time.
  const w = 11.20f;
  let curr = exposure_bias * naughtyDogTonemapHelper(x);
  let white_scale = 1.0f / naughtyDogTonemapHelper(vec3<f32>(w));
  let color = curr * white_scale;
  return pow(color, vec3<f32>(1.0f/2.2f));
}
fn naughtyDogTonemapHelper(x: vec3<f32>) -> vec3<f32> {
  const a = 0.15f;
  const b = 0.50f;
  const c = 0.10f;
  const d = 0.20f;
  const e = 0.02f;
  const f = 0.30f;
  return (x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f) - (e / f);
}

/// returns '1' if fragment is lit by this light, else '0'.
fn dirLightShadowMapSample(light_idx: u32, frag_world_pos: vec3f) -> vec4f {
  let frag_world_pos_v4 = vec4f(frag_world_pos, 1.0);
  let array_offset = light_idx * p_DIR_LIGHT_CASCADE_COUNT;
  for (var i = 0u; i < p_DIR_LIGHT_CASCADE_COUNT; i++) {
    let pv = u_light.shadow.dir_csm_proj_view_mats[light_idx][i];
    let xy_v4 = pv * frag_world_pos_v4;
    let xy_distance = xy_v4.w;
    let xy = xy_v4.xy;
    let bounds = u_light.shadow.dir_csm_xy_bounds[light_idx][i];
    let x_uv = 0.0 + (xy.x - bounds.min.x) / (bounds.max.x - bounds.min.x);
    let y_uv = 1.0 - (xy.y - bounds.min.y) / (bounds.max.y - bounds.min.y);
    if (0.0 <= x_uv && x_uv <= 1.0 && 0.0 <= y_uv && y_uv <= 1.0) {
      let sample_v4 = textureSampleLevel(
        dir_light_csm_texture_array, 
        dir_light_csm_texture_sampler, 
        vec2f(x_uv, y_uv),
        array_offset + i,
        0.0
      );

      // DEBUG:
      // return vec4f(sample_v4.r * 0.5 + 0.5, f32(i) * 64.0, 0.0, 1.0);
      // return vec4f(1.0, f32(i) * 64.0, 0.0, 1.0);
      if (i == 0) {
        return vec4f(1.0, 0.0, 0.0, 1.0);
      }
      if (i == 1) {
        return vec4f(0.0, 1.0, 0.0, 1.0);
      }
      if (i == 2) {
        return vec4f(0.0, 0.0, 1.0, 1.0);
      }
      if (i == 3) {
        return vec4f(1.0);
      }

      let sample = sample_v4.r;
      if (sample <= xy_distance) {
        return vec4f(1.0);
      } else {
        return vec4f(0.0);
      }
    }
  }
  
  // DEFAULT: leave the fragment lit if outside CSM bounds:
  // return 1;
  
  // DEBUG:
  return vec4f(0.0, 0.0, 0.0, 8.0);
}