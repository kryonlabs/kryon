//! CSS Transform Support for Kryon
//!
//! This module provides a foundation for CSS-style transforms including
//! translations, rotations, scaling, and matrix operations.

use glam::{Mat4, Vec3, Quat, Vec4Swizzles};
use std::f32::consts::PI;

/// CSS Transform matrix and operations
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Transform {
    /// 4x4 transformation matrix
    pub matrix: Mat4,
}

impl Transform {
    /// Create an identity transform
    pub fn identity() -> Self {
        Self {
            matrix: Mat4::IDENTITY,
        }
    }

    /// Create a translation transform
    pub fn translate(x: f32, y: f32, z: f32) -> Self {
        Self {
            matrix: Mat4::from_translation(Vec3::new(x, y, z)),
        }
    }

    /// Create a 2D translation transform
    pub fn translate_2d(x: f32, y: f32) -> Self {
        Self::translate(x, y, 0.0)
    }

    /// Create a rotation transform around the Z axis (2D rotation)
    pub fn rotate_z(angle_degrees: f32) -> Self {
        let angle_radians = angle_degrees * PI / 180.0;
        Self {
            matrix: Mat4::from_rotation_z(angle_radians),
        }
    }

    /// Create a rotation transform around an arbitrary axis
    pub fn rotate_axis(axis: Vec3, angle_degrees: f32) -> Self {
        let angle_radians = angle_degrees * PI / 180.0;
        let quaternion = Quat::from_axis_angle(axis, angle_radians);
        Self {
            matrix: Mat4::from_quat(quaternion),
        }
    }

    /// Create a scale transform
    pub fn scale(x: f32, y: f32, z: f32) -> Self {
        Self {
            matrix: Mat4::from_scale(Vec3::new(x, y, z)),
        }
    }

    /// Create a 2D scale transform
    pub fn scale_2d(x: f32, y: f32) -> Self {
        Self::scale(x, y, 1.0)
    }

    /// Create a uniform scale transform
    pub fn scale_uniform(scale: f32) -> Self {
        Self::scale(scale, scale, scale)
    }

    /// Create a skew transform
    pub fn skew(x_degrees: f32, y_degrees: f32) -> Self {
        let x_radians = x_degrees * PI / 180.0;
        let y_radians = y_degrees * PI / 180.0;
        
        let skew_x = x_radians.tan();
        let skew_y = y_radians.tan();
        
        Self {
            matrix: Mat4::from_cols(
                Vec3::new(1.0, skew_y, 0.0).extend(0.0),
                Vec3::new(skew_x, 1.0, 0.0).extend(0.0),
                Vec3::new(0.0, 0.0, 1.0).extend(0.0),
                Vec3::new(0.0, 0.0, 0.0).extend(1.0),
            ),
        }
    }

    /// Create a perspective transform
    pub fn perspective(distance: f32) -> Self {
        let mut matrix = Mat4::IDENTITY;
        if distance != 0.0 {
            matrix.w_axis.z = -1.0 / distance;
        }
        Self { matrix }
    }

    /// Combine this transform with another (matrix multiplication)
    pub fn combine(&self, other: &Transform) -> Self {
        Self {
            matrix: self.matrix * other.matrix,
        }
    }

    /// Apply transform to a point
    pub fn transform_point(&self, point: Vec3) -> Vec3 {
        let point4 = self.matrix * point.extend(1.0);
        if point4.w != 0.0 {
            point4.xyz() / point4.w
        } else {
            point4.xyz()
        }
    }

    /// Apply transform to a 2D point
    pub fn transform_point_2d(&self, point: glam::Vec2) -> glam::Vec2 {
        let result = self.transform_point(point.extend(0.0));
        glam::Vec2::new(result.x, result.y)
    }

    /// Get the inverse transform
    pub fn inverse(&self) -> Option<Self> {
        let inv = self.matrix.inverse();
        if inv.is_finite() {
            Some(Self { matrix: inv })
        } else {
            None
        }
    }

    /// Check if this is an identity transform
    pub fn is_identity(&self) -> bool {
        self.matrix.abs_diff_eq(Mat4::IDENTITY, f32::EPSILON)
    }

    /// Decompose the transform into translation, rotation, and scale
    pub fn decompose(&self) -> (Vec3, Quat, Vec3) {
        let (scale, rotation, translation) = self.matrix.to_scale_rotation_translation();
        (translation, rotation, scale)
    }

    /// Get the translation component
    pub fn translation(&self) -> Vec3 {
        self.matrix.w_axis.xyz()
    }

    /// Get the 2D translation component
    pub fn translation_2d(&self) -> glam::Vec2 {
        let translation = self.translation();
        glam::Vec2::new(translation.x, translation.y)
    }
}

impl Default for Transform {
    fn default() -> Self {
        Self::identity()
    }
}

/// CSS Transform chain builder
#[derive(Debug, Clone)]
pub struct TransformChain {
    transforms: Vec<Transform>,
}

impl TransformChain {
    /// Create a new transform chain
    pub fn new() -> Self {
        Self {
            transforms: Vec::new(),
        }
    }

    /// Add a translation to the chain
    pub fn translate(mut self, x: f32, y: f32, z: f32) -> Self {
        self.transforms.push(Transform::translate(x, y, z));
        self
    }

    /// Add a 2D translation to the chain
    pub fn translate_2d(mut self, x: f32, y: f32) -> Self {
        self.transforms.push(Transform::translate_2d(x, y));
        self
    }

    /// Add a rotation to the chain
    pub fn rotate(mut self, angle_degrees: f32) -> Self {
        self.transforms.push(Transform::rotate_z(angle_degrees));
        self
    }

    /// Add a scale to the chain
    pub fn scale(mut self, x: f32, y: f32, z: f32) -> Self {
        self.transforms.push(Transform::scale(x, y, z));
        self
    }

    /// Add a 2D scale to the chain
    pub fn scale_2d(mut self, x: f32, y: f32) -> Self {
        self.transforms.push(Transform::scale_2d(x, y));
        self
    }

    /// Add a uniform scale to the chain
    pub fn scale_uniform(mut self, scale: f32) -> Self {
        self.transforms.push(Transform::scale_uniform(scale));
        self
    }

    /// Add a skew to the chain
    pub fn skew(mut self, x_degrees: f32, y_degrees: f32) -> Self {
        self.transforms.push(Transform::skew(x_degrees, y_degrees));
        self
    }

    /// Add a custom transform to the chain
    pub fn transform(mut self, transform: Transform) -> Self {
        self.transforms.push(transform);
        self
    }

    /// Build the final combined transform
    pub fn build(self) -> Transform {
        self.transforms.into_iter().fold(Transform::identity(), |acc, transform| {
            acc.combine(&transform)
        })
    }
}

impl Default for TransformChain {
    fn default() -> Self {
        Self::new()
    }
}

/// CSS Transform origin point
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum TransformOrigin {
    /// Pixel coordinates
    Pixels(f32, f32),
    /// Percentage of element size (0.0 to 1.0)
    Percentage(f32, f32),
    /// Predefined origins
    Predefined(PredefinedOrigin),
}

/// Predefined transform origins
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum PredefinedOrigin {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
}

impl TransformOrigin {
    /// Convert to pixel coordinates given element size
    pub fn to_pixels(&self, element_size: glam::Vec2) -> glam::Vec2 {
        match self {
            TransformOrigin::Pixels(x, y) => glam::Vec2::new(*x, *y),
            TransformOrigin::Percentage(x, y) => glam::Vec2::new(
                element_size.x * x,
                element_size.y * y,
            ),
            TransformOrigin::Predefined(origin) => match origin {
                PredefinedOrigin::TopLeft => glam::Vec2::new(0.0, 0.0),
                PredefinedOrigin::TopCenter => glam::Vec2::new(element_size.x * 0.5, 0.0),
                PredefinedOrigin::TopRight => glam::Vec2::new(element_size.x, 0.0),
                PredefinedOrigin::CenterLeft => glam::Vec2::new(0.0, element_size.y * 0.5),
                PredefinedOrigin::Center => glam::Vec2::new(element_size.x * 0.5, element_size.y * 0.5),
                PredefinedOrigin::CenterRight => glam::Vec2::new(element_size.x, element_size.y * 0.5),
                PredefinedOrigin::BottomLeft => glam::Vec2::new(0.0, element_size.y),
                PredefinedOrigin::BottomCenter => glam::Vec2::new(element_size.x * 0.5, element_size.y),
                PredefinedOrigin::BottomRight => glam::Vec2::new(element_size.x, element_size.y),
            },
        }
    }
}

impl Default for TransformOrigin {
    fn default() -> Self {
        TransformOrigin::Predefined(PredefinedOrigin::Center)
    }
}

/// Apply a transform around a specific origin point
pub fn transform_around_origin(
    transform: &Transform,
    origin: glam::Vec2,
) -> Transform {
    let translate_to_origin = Transform::translate_2d(-origin.x, -origin.y);
    let translate_back = Transform::translate_2d(origin.x, origin.y);
    
    translate_back.combine(transform).combine(&translate_to_origin)
}

#[cfg(test)]
mod tests {
    use super::*;
    use glam::Vec2;

    #[test]
    fn test_identity_transform() {
        let transform = Transform::identity();
        let point = Vec2::new(10.0, 20.0);
        let result = transform.transform_point_2d(point);
        assert!((result - point).length() < f32::EPSILON);
    }

    #[test]
    fn test_translation() {
        let transform = Transform::translate_2d(5.0, 10.0);
        let point = Vec2::new(0.0, 0.0);
        let result = transform.transform_point_2d(point);
        assert!((result - Vec2::new(5.0, 10.0)).length() < f32::EPSILON);
    }

    #[test]
    fn test_rotation() {
        let transform = Transform::rotate_z(90.0);
        let point = Vec2::new(1.0, 0.0);
        let result = transform.transform_point_2d(point);
        assert!((result - Vec2::new(0.0, 1.0)).length() < 0.001);
    }

    #[test]
    fn test_scale() {
        let transform = Transform::scale_2d(2.0, 3.0);
        let point = Vec2::new(1.0, 1.0);
        let result = transform.transform_point_2d(point);
        assert!((result - Vec2::new(2.0, 3.0)).length() < f32::EPSILON);
    }

    #[test]
    fn test_transform_chain() {
        let transform = TransformChain::new()
            .translate_2d(10.0, 20.0)
            .rotate(45.0)
            .scale_2d(2.0, 2.0)
            .build();
        
        let point = Vec2::new(0.0, 0.0);
        let result = transform.transform_point_2d(point);
        
        // Should be translated by (10, 20), then rotated 45 degrees, then scaled by 2
        assert!(!result.is_nan());
    }

    #[test]
    fn test_transform_origin() {
        let origin = TransformOrigin::Percentage(0.5, 0.5);
        let element_size = Vec2::new(100.0, 200.0);
        let result = origin.to_pixels(element_size);
        assert!((result - Vec2::new(50.0, 100.0)).length() < f32::EPSILON);
    }

    #[test]
    fn test_transform_around_origin() {
        let transform = Transform::scale_2d(2.0, 2.0);
        let origin = Vec2::new(50.0, 50.0);
        let transform_with_origin = transform_around_origin(&transform, origin);
        
        // Point at origin should stay at origin
        let result = transform_with_origin.transform_point_2d(origin);
        assert!((result - origin).length() < 0.001);
    }

    #[test]
    fn test_transform_inverse() {
        let transform = Transform::translate_2d(10.0, 20.0);
        let inverse = transform.inverse().unwrap();
        let combined = transform.combine(&inverse);
        
        assert!(combined.is_identity());
    }

    #[test]
    fn test_transform_decompose() {
        let transform = TransformChain::new()
            .translate_2d(10.0, 20.0)
            .rotate(45.0)
            .scale_2d(2.0, 3.0)
            .build();
        
        let (translation, _rotation, scale) = transform.decompose();
        assert!((translation.x - 10.0).abs() < 0.001);
        assert!((translation.y - 20.0).abs() < 0.001);
        assert!((scale.x - 2.0).abs() < 0.001);
        assert!((scale.y - 3.0).abs() < 0.001);
    }
}