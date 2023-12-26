// Parameters:
// - p_SAMPLE_MODE: u32 => whether to broadcast a monochrome sample (1) or reproduce color (2)

struct VertexInput {
  @location(0) normalized_offset: vec2f,
}
struct FragmentInput {
  @builtin(position) clip_position: vec4f,
  @location(0) @interpolate(linear) uv: vec2f,
}

struct OverlayUniform {
  screen_size: vec2i,
  rect_size: vec2i,
  rect_center: vec2i,
}

@group(0) @binding(0) var<uniform> u_overlay: OverlayUniform;
@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;

@vertex
fn vertexShaderMain(vi: VertexInput) -> FragmentInput {
  // TODO: consider the case where 'screen_size' contains an odd number of pixels.
  let half_rect_size = (vec2f(u_overlay.rect_size) / 2.0) / (vec2f(u_overlay.screen_size) / 2.0);
  let rect_center = mapPixelCoordsToNdc(vec2f(u_overlay.rect_center));
  var fi: FragmentInput;
  fi.clip_position = vec4f(rect_center + vec2f(half_rect_size) * vi.normalized_offset, 0.0, 1.0);
  fi.uv = vec2f((vi.normalized_offset.x + 1.0) / 2.0, 1.0 - (vi.normalized_offset.y + 1.0) / 2.0);
  return fi;
}

@fragment
fn fragmentShaderMain(fi: FragmentInput) -> @location(0) vec4f {
  switch (p_SAMPLE_MODE) {
    case 1:
    {
      let sample = textureSampleLevel(u_texture, u_sampler, fi.uv, 0.0).r;
      return vec4(sample, sample, sample, 1.0);
    }
    case 4:
    {
      return textureSampleLevel(u_texture, u_sampler, fi.uv, 0.0);
    }
    default:
    {
      return vec4(1.0, 0.0, 1.0, 1.0);
    }
  }
}

fn mapPixelCoordsToNdc(xy: vec2f) -> vec2f {
  // In all below examples, assume screen size is (800, 600).
  // - xy=(400, 300) -> (   0,    0)
  // - xy=(400, 150) -> (   0, +150)
  // - xy=(400, 450) -> (   0, -150)
  // - xy=(200, 300) -> (-200,    0)
  // - xy=(600, 300) -> (+200,    0)
  let half_screen_size = vec2f(u_overlay.screen_size) / 2;
  return (xy - half_screen_size) * vec2f(1.0, -1.0);
}