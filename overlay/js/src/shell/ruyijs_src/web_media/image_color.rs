//! Native WebMedia color adapter; qcms owns ICC parsing and pixel conversion.
extern crate qcms;
use qcms::{DataType, Intent, Profile, Transform};
#[no_mangle]
pub unsafe extern "C" fn ruyijs_image_srgb_to_profile(
    pixels: *mut u8, size: usize, profile: *const u8, profile_size: usize,
) -> bool {
    if pixels.is_null() || size % 4 != 0 || profile.is_null() || profile_size == 0 {
        return false;
    }
    let input = Profile::new_sRGB();
    let Some(mut output) = Profile::new_from_slice(std::slice::from_raw_parts(profile, profile_size), false) else {
        return false;
    };
    output.precache_output_transform();
    let Some(transform) = Transform::new(&input, &output, DataType::BGRA8, Intent::Perceptual) else {
        return false;
    };
    transform.apply(std::slice::from_raw_parts_mut(pixels, size));
    true
}
