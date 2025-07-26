//! Layout engine tests for Kryon renderer

use kryon_core::{Element, ElementId, ElementType};
use kryon_layout::{LayoutEngine, LayoutDimension, TaffyLayoutEngine};
use kryon_render::Vec2;
use std::collections::HashMap;

fn create_test_element(id: &str, element_type: ElementType) -> Element {
    let mut element = Element::new(element_type);
    element.id = id.to_string();
    element
}

#[test]
fn test_flexbox_column_alignment() {
    let mut layout_engine = TaffyLayoutEngine::new();
    let mut elements = HashMap::new();
    
    // Create container with column layout
    let mut container = create_test_element("container", ElementType::Container);
    container.layout_size = Vec2::new(200.0, 300.0);
    container.custom_properties.insert("display".to_string(), "flex".into());
    container.custom_properties.insert("flex_direction".to_string(), "column".into());
    container.custom_properties.insert("align_items".to_string(), "end".into());
    
    let container_id = ElementId(1);
    elements.insert(container_id, container);
    
    // Create child elements
    let mut child1 = create_test_element("child1", ElementType::Container);
    child1.layout_size = Vec2::new(50.0, 30.0);
    child1.parent = Some(container_id);
    
    let mut child2 = create_test_element("child2", ElementType::Container);
    child2.layout_size = Vec2::new(80.0, 30.0);
    child2.parent = Some(container_id);
    
    let child1_id = ElementId(2);
    let child2_id = ElementId(3);
    
    elements.insert(child1_id, child1);
    elements.insert(child2_id, child2);
    
    // Add children to container
    elements.get_mut(&container_id).unwrap().children.push(child1_id);
    elements.get_mut(&container_id).unwrap().children.push(child2_id);
    
    // Compute layout
    layout_engine.compute_layout(&mut elements, container_id, Vec2::new(800.0, 600.0));
    
    // Verify alignment
    let child1 = &elements[&child1_id];
    let child2 = &elements[&child2_id];
    
    // Children should be aligned to the right (end) of the container
    assert_eq!(child1.computed_position.x, 150.0, "Child 1 should be right-aligned");
    assert_eq!(child2.computed_position.x, 120.0, "Child 2 should be right-aligned");
}

#[test]
fn test_flexbox_row_justify_end() {
    let mut layout_engine = TaffyLayoutEngine::new();
    let mut elements = HashMap::new();
    
    // Create container with row layout
    let mut container = create_test_element("container", ElementType::Container);
    container.layout_size = Vec2::new(400.0, 100.0);
    container.custom_properties.insert("display".to_string(), "flex".into());
    container.custom_properties.insert("flex_direction".to_string(), "row".into());
    container.custom_properties.insert("justify_content".to_string(), "end".into());
    
    let container_id = ElementId(1);
    elements.insert(container_id, container);
    
    // Create child elements
    let mut child1 = create_test_element("child1", ElementType::Container);
    child1.layout_size = Vec2::new(60.0, 40.0);
    child1.parent = Some(container_id);
    
    let mut child2 = create_test_element("child2", ElementType::Container);
    child2.layout_size = Vec2::new(80.0, 40.0);
    child2.parent = Some(container_id);
    
    let child1_id = ElementId(2);
    let child2_id = ElementId(3);
    
    elements.insert(child1_id, child1);
    elements.insert(child2_id, child2);
    
    // Add children to container
    elements.get_mut(&container_id).unwrap().children.push(child1_id);
    elements.get_mut(&container_id).unwrap().children.push(child2_id);
    
    // Compute layout
    layout_engine.compute_layout(&mut elements, container_id, Vec2::new(800.0, 600.0));
    
    // Verify justification
    let child1 = &elements[&child1_id];
    let child2 = &elements[&child2_id];
    
    // Children should be at the end (right) of the container
    assert!(child2.computed_position.x > 300.0, "Child 2 should be near right edge");
    assert_eq!(child1.computed_position.x + 60.0 + /* gap */ 0.0, child2.computed_position.x, 
        "Children should be adjacent");
}

#[test]
fn test_nested_flex_layouts() {
    let mut layout_engine = TaffyLayoutEngine::new();
    let mut elements = HashMap::new();
    
    // Create root container
    let mut root = create_test_element("root", ElementType::Container);
    root.layout_size = Vec2::new(600.0, 400.0);
    root.custom_properties.insert("display".to_string(), "flex".into());
    root.custom_properties.insert("flex_direction".to_string(), "row".into());
    
    let root_id = ElementId(1);
    elements.insert(root_id, root);
    
    // Create sidebar (fixed width)
    let mut sidebar = create_test_element("sidebar", ElementType::Container);
    sidebar.layout_size = Vec2::new(200.0, 0.0); // Height will be stretched
    sidebar.parent = Some(root_id);
    sidebar.custom_properties.insert("display".to_string(), "flex".into());
    sidebar.custom_properties.insert("flex_direction".to_string(), "column".into());
    
    let sidebar_id = ElementId(2);
    elements.insert(sidebar_id, sidebar);
    
    // Create main content (flex: 1)
    let mut main = create_test_element("main", ElementType::Container);
    main.parent = Some(root_id);
    main.custom_properties.insert("display".to_string(), "flex".into());
    main.custom_properties.insert("flex".to_string(), "1".into());
    
    let main_id = ElementId(3);
    elements.insert(main_id, main);
    
    // Add children to root
    elements.get_mut(&root_id).unwrap().children.push(sidebar_id);
    elements.get_mut(&root_id).unwrap().children.push(main_id);
    
    // Compute layout
    layout_engine.compute_layout(&mut elements, root_id, Vec2::new(800.0, 600.0));
    
    // Verify layout
    let sidebar = &elements[&sidebar_id];
    let main = &elements[&main_id];
    
    assert_eq!(sidebar.computed_size.x, 200.0, "Sidebar should maintain fixed width");
    assert_eq!(sidebar.computed_size.y, 400.0, "Sidebar should stretch to full height");
    assert_eq!(main.computed_position.x, 200.0, "Main should start after sidebar");
    assert_eq!(main.computed_size.x, 400.0, "Main should fill remaining width");
}

#[test]
fn test_gap_property() {
    let mut layout_engine = TaffyLayoutEngine::new();
    let mut elements = HashMap::new();
    
    // Create container with gap
    let mut container = create_test_element("container", ElementType::Container);
    container.layout_size = Vec2::new(300.0, 100.0);
    container.custom_properties.insert("display".to_string(), "flex".into());
    container.custom_properties.insert("flex_direction".to_string(), "row".into());
    container.custom_properties.insert("gap".to_string(), "20".into());
    
    let container_id = ElementId(1);
    elements.insert(container_id, container);
    
    // Create three children
    for i in 0..3 {
        let mut child = create_test_element(&format!("child{}", i), ElementType::Container);
        child.layout_size = Vec2::new(50.0, 50.0);
        child.parent = Some(container_id);
        
        let child_id = ElementId(i + 2);
        elements.insert(child_id, child);
        elements.get_mut(&container_id).unwrap().children.push(child_id);
    }
    
    // Compute layout
    layout_engine.compute_layout(&mut elements, container_id, Vec2::new(800.0, 600.0));
    
    // Verify gaps
    let child1 = &elements[&ElementId(2)];
    let child2 = &elements[&ElementId(3)];
    let child3 = &elements[&ElementId(4)];
    
    assert_eq!(child2.computed_position.x - (child1.computed_position.x + 50.0), 20.0, 
        "Gap between child1 and child2 should be 20px");
    assert_eq!(child3.computed_position.x - (child2.computed_position.x + 50.0), 20.0,
        "Gap between child2 and child3 should be 20px");
}