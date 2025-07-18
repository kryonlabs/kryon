-- =============================================================================
--  Kryon-Lua Bridge: Core API for Scripting
-- =============================================================================
-- This script sets up the API that allows Lua code to interact with the
-- Kryon UI engine. It provides a DOM-like interface for manipulating
-- elements, handling events, and managing state.
--

-- =============================================================================
--  1. Global State & Pending Changes
-- =============================================================================
-- These tables store changes made by scripts during a single frame.
-- They are collected by the Rust runtime after each event handler runs to
-- update the UI state efficiently.

_pending_style_changes      = {} -- { [element_id] = new_style_id }
_pending_state_changes      = {} -- { [element_id] = is_checked (boolean) }
_pending_text_changes       = {} -- { [element_id] = new_text (string) }
_pending_visibility_changes = {} -- { [element_id] = is_visible (boolean) }

-- Event listener system state
_event_listeners            = {} -- { [event_type] = {callback1, callback2, ...} }
_ready_callbacks            = {} -- Callbacks to run when the UI is fully loaded
_is_ready                   = false


-- =============================================================================
--  2. Core DOM-like API
-- =============================================================================

---
-- Retrieves an element by its string ID and returns a proxy object
-- that can be used to manipulate it.
---@param element_id string The string ID of the element to retrieve.
---@return table|nil A proxy object for the element, or nil if not found.
--
function getElementById(element_id)
    -- _element_ids is a table provided by the Rust runtime mapping string IDs to numeric IDs.
    local numeric_id = _element_ids[element_id]

    if numeric_id then
        -- Return a proxy table with methods to manipulate the element.
        -- This avoids exposing the raw element data directly to the script.
        return {
            id = element_id,
            numeric_id = numeric_id,

            -- Queues a style change for the element.
            setStyle = function(self, style_name)
                local style_id = _style_ids[style_name]
                if style_id then
                    _pending_style_changes[self.numeric_id] = style_id
                    print("Queuing style change for element '" .. self.id .. "' to style '" .. style_name .. "' (ID: " .. style_id .. ")")
                else
                    print("Error: Style '" .. style_name .. "' not found.")
                end
            end,

            -- Queues a checked state change (for checkboxes, radio buttons, etc.).
            setChecked = function(self, checked)
                _pending_state_changes[self.numeric_id] = checked
                print("Queuing checked state change for element '" .. self.id .. "' to " .. tostring(checked))
            end,

            -- Queues a text content change.
            setText = function(self, text)
                _pending_text_changes[self.numeric_id] = tostring(text)
                print("Queuing text change for element '" .. self.id .. "' to: " .. tostring(text))
            end,

            -- Gets the current text of the element, considering pending changes first.
            getText = function(self)
                if _pending_text_changes[self.numeric_id] ~= nil then
                    return _pending_text_changes[self.numeric_id]
                end
                -- _elements_data is provided by the Rust runtime.
                if _elements_data and _elements_data[self.numeric_id] then
                    return _elements_data[self.numeric_id].text or ""
                end
                return ""
            end,

            -- Queues a visibility change.
            setVisible = function(self, visible)
                _pending_visibility_changes[self.numeric_id] = visible
                print("Queuing visibility change for element '" .. self.id .. "' to " .. tostring(visible))
            end,

            -- Gets the current visibility of the element, considering pending changes.
            getVisible = function(self)
                if _pending_visibility_changes[self.numeric_id] ~= nil then
                    return _pending_visibility_changes[self.numeric_id]
                end
                if _elements_data and _elements_data[self.numeric_id] then
                    return _elements_data[self.numeric_id].visible
                end
                return true -- Default to visible if not found
            end,

            -- Gets the current checked state of the element, considering pending changes.
            getChecked = function(self)
                if _pending_state_changes[self.numeric_id] ~= nil then
                    return _pending_state_changes[self.numeric_id]
                end
                if _elements_data and _elements_data[self.numeric_id] then
                    return _elements_data[self.numeric_id].is_checked or false
                end
                return false -- Default to unchecked if not found
            end,

            -- DOM Traversal Methods
            getParent = function(self) return _get_parent_element(self.numeric_id) end,
            getChildren = function(self) return _get_children_elements(self.numeric_id) end,
            getFirstChild = function(self)
                local children = _get_children_elements(self.numeric_id)
                return children[1] or nil
            end,
            getLastChild = function(self)
                local children = _get_children_elements(self.numeric_id)
                return children[#children] or nil
            end,
            getNextSibling = function(self) return _get_next_sibling(self.numeric_id) end,
            getPreviousSibling = function(self) return _get_previous_sibling(self.numeric_id) end
        }
    else
        print("Error: Element with ID '" .. tostring(element_id) .. "' not found.")
        return nil
    end
end

-- Alias for backward compatibility.
get_element = getElementById

---
-- Retrieves a table of custom properties for a given component/element.
---@param element_id string|number The ID of the element.
---@return table A table of properties, or an empty table if none.
--
function getComponentProperties(element_id)
    local props = _component_properties[element_id]
    if not props and type(element_id) == "string" then
        local numeric_id = _element_ids[element_id]
        if numeric_id then
            props = _component_properties[numeric_id]
        end
    end
    return props or {}
end

---
-- Retrieves a single custom property value for a given component/element.
---@param element_id string|number The ID of the element.
---@param property_name string The name of the property to retrieve.
---@return any The property value, or nil if not found.
--
function getComponentProperty(element_id, property_name)
    local props = getComponentProperties(element_id)
    return props[property_name]
end


-- =============================================================================
--  3. DOM Traversal & Querying (Internal Helpers)
-- =============================================================================
-- These internal functions are called by the public API and query the
-- _elements_data table provided by the Rust runtime.

---
-- Internal helper to create a Lua-side proxy for a raw element.
---@param element_id number The numeric ID of the element.
---@return table|nil The proxy object.
--
function _create_element_proxy(element_id)
    local element_data = _elements_data[element_id]
    if not element_data then return nil end
    
    -- This proxy object is read-only and used for traversal.
    -- To modify an element, one must first get it via `getElementById`.
    return {
        id = element_data.id,
        numeric_id = element_id,
        element_type = element_data.element_type,
        visible = element_data.visible,
        text = element_data.text
    }
end

function _get_parent_element(element_id)
    local element_data = _elements_data[element_id]
    if element_data and element_data.parent_id then
        return _create_element_proxy(element_data.parent_id)
    end
    return nil
end

function _get_children_elements(element_id)
    local result = {}
    local element_data = _elements_data[element_id]
    if element_data and element_data.children then
        for _, child_id in ipairs(element_data.children) do
            local child_proxy = _create_element_proxy(child_id)
            if child_proxy then table.insert(result, child_proxy) end
        end
    end
    return result
end

function _get_next_sibling(element_id)
    local element_data = _elements_data[element_id]
    if element_data and element_data.parent_id then
        local parent_data = _elements_data[element_data.parent_id]
        if parent_data and parent_data.children then
            for i, child_id in ipairs(parent_data.children) do
                if child_id == element_id and i < #parent_data.children then
                    return _create_element_proxy(parent_data.children[i + 1])
                end
            end
        end
    end
    return nil
end

function _get_previous_sibling(element_id)
    local element_data = _elements_data[element_id]
    if element_data and element_data.parent_id then
        local parent_data = _elements_data[element_data.parent_id]
        if parent_data and parent_data.children then
            for i, child_id in ipairs(parent_data.children) do
                if child_id == element_id and i > 1 then
                    return _create_element_proxy(parent_data.children[i - 1])
                end
            end
        end
    end
    return nil
end

function _get_elements_by_tag(tag_name)
    local result = {}
    for element_id, element_data in pairs(_elements_data) do
        if element_data.element_type:lower() == tag_name:lower() then
            local proxy = _create_element_proxy(element_id)
            if proxy then table.insert(result, proxy) end
        end
    end
    return result
end

function _get_elements_by_class(class_name)
    local result = {}
    for element_id, element_data in pairs(_elements_data) do
        local style_data = _styles_data[element_data.style_id]
        if style_data and style_data.name == class_name then
            local proxy = _create_element_proxy(element_id)
            if proxy then table.insert(result, proxy) end
        end
    end
    return result
end

function _query_selector_all(selector)
    local result = {}
    if selector:sub(1, 1) == '#' then
        local id = selector:sub(2)
        local element = getElementById(id)
        if element then table.insert(result, element) end
    elseif selector:sub(1, 1) == '.' then
        result = _get_elements_by_class(selector:sub(2))
    else
        result = _get_elements_by_tag(selector)
    end
    return result
end

function querySelector(selector)
    local results = _query_selector_all(selector)
    return results[1] or nil
end

function getRootElement()
    return _get_root_element()
end


-- =============================================================================
--  4. Event System
-- =============================================================================

---
-- Registers a callback to be executed when the UI is fully loaded and ready,
-- similar to DOMContentLoaded in web development.
---@param callback function The function to execute.
--
function onReady(callback)
    if type(callback) ~= "function" then
        print("Error: onReady(callback) - callback must be a function.")
        return
    end

    if _is_ready then
        -- If UI is already ready, execute immediately.
        local success, error = pcall(callback)
        if not success then
            print("Error in immediate onReady callback: " .. tostring(error))
        end
    else
        -- Otherwise, queue for later execution.
        table.insert(_ready_callbacks, callback)
    end
end

-- Internal function called by the Rust runtime to signal readiness.
function _mark_ready()
    _is_ready = true
end

---
-- Adds a global event listener for document-level events like 'keydown'.
---@param event_type string The type of event to listen for (e.g., "keydown").
---@param callback function The function to call when the event occurs.
--
function addEventListener(event_type, callback)
    if type(event_type) ~= "string" or type(callback) ~= "function" then
        print("Error: addEventListener(event_type, callback) requires a string and a function.")
        return
    end

    if not _event_listeners[event_type] then
        _event_listeners[event_type] = {}
    end

    table.insert(_event_listeners[event_type], callback)
    print("Added event listener for '" .. event_type .. "'.")
end

-- Internal function called by the Rust runtime to trigger an event.
function _trigger_event(event_type, event_data)
    if _event_listeners[event_type] then
        for _, callback in ipairs(_event_listeners[event_type]) do
            pcall(callback, event_data or {})
        end
    end
end

-- Convenience wrappers for common events
function onKeyDown(callback) addEventListener('keydown', callback) end
function onKeyUp(callback) addEventListener('keyup', callback) end
function onResize(callback) addEventListener('resize', callback) end
function onLoad(callback) addEventListener('load', callback) end


-- =============================================================================
--  5. Internal Getter Functions for the Rust Runtime
-- =============================================================================
-- These functions are called by the Rust runtime to retrieve the state
-- changes that scripts have queued up.

---
-- Helper function to clear a table in-place. This is crucial because
-- re-assigning a global variable (e.g., `_my_table = {}`) can trigger
-- the __newindex metamethod, which can cause issues. Modifying the
-- table's contents directly avoids this.
---@param t table The table to clear.
--
function _clear_table_in_place(t)
    for k in pairs(t) do
        t[k] = nil
    end
end

---
-- Creates a copy of a table.
---@param t table The table to copy.
---@return table A new table with the same key-value pairs.
--
function _copy_table(t)
    local copy = {}
    for k, v in pairs(t) do
        copy[k] = v
    end
    return copy
end

-- The following functions get changes without clearing them.
-- Changes are cleared separately after both DOM and template variable changes are applied.

function _get_pending_style_changes()
    return _copy_table(_pending_style_changes)
end

function _get_pending_state_changes()
    return _copy_table(_pending_state_changes)
end

function _get_pending_text_changes()
    return _copy_table(_pending_text_changes)
end

function _get_pending_visibility_changes()
    return _copy_table(_pending_visibility_changes)
end

-- This getter is part of the reactive variable system.
-- Get template variable changes without clearing them.
function _get_reactive_template_variable_changes()
    return _copy_table(_template_variable_changes)
end

-- Clear all DOM changes without returning them
function _clear_dom_changes()
    _clear_table_in_place(_pending_style_changes)
    _clear_table_in_place(_pending_state_changes)
    _clear_table_in_place(_pending_text_changes)
    _clear_table_in_place(_pending_visibility_changes)
end

-- Clear template variable changes without returning them
function _clear_template_variable_changes()
    _clear_table_in_place(_template_variable_changes)
end


-- =============================================================================
--  6. Canvas Drawing API
-- =============================================================================
-- Canvas drawing functions for script execution
-- These functions are called from within Canvas draw_script functions

-- Global Canvas drawing context
_canvas_commands = {} -- Commands to be executed for current canvas

-- Clear canvas commands (called by runtime for each canvas)
function _clear_canvas_commands()
    _clear_table_in_place(_canvas_commands)
end

-- Get canvas commands (called by runtime to retrieve drawing commands)
function _get_canvas_commands()
    return _copy_table(_canvas_commands)
end

-- Helper function to add a canvas command
function _add_canvas_command(command)
    table.insert(_canvas_commands, command)
end

---
-- Draws a line on the canvas
---@param x1 number Start X coordinate
---@param y1 number Start Y coordinate
---@param x2 number End X coordinate
---@param y2 number End Y coordinate
---@param color table Color as {r, g, b, a} values (0-1)
---@param width number Line width (optional, default 1.0)
--
function drawLine(x1, y1, x2, y2, color, width)
    _add_canvas_command({
        type = "DrawCanvasLine",
        start = {x = x1, y = y1},
        ["end"] = {x = x2, y = y2},
        color = color or {r = 1.0, g = 1.0, b = 1.0, a = 1.0},
        width = width or 1.0
    })
end

---
-- Draws a rectangle on the canvas
---@param x number X coordinate
---@param y number Y coordinate
---@param width number Width of rectangle
---@param height number Height of rectangle
---@param fill_color table Fill color as {r, g, b, a} (optional)
---@param stroke_color table Stroke color as {r, g, b, a} (optional)
---@param stroke_width number Stroke width (optional, default 1.0)
--
function drawRect(x, y, width, height, fill_color, stroke_color, stroke_width)
    _add_canvas_command({
        type = "DrawCanvasRect",
        position = {x = x, y = y},
        size = {x = width, y = height},
        fill_color = fill_color,
        stroke_color = stroke_color,
        stroke_width = stroke_width or 1.0
    })
end

---
-- Draws a circle on the canvas
---@param x number Center X coordinate
---@param y number Center Y coordinate
---@param radius number Circle radius
---@param fill_color table Fill color as {r, g, b, a} (optional)
---@param stroke_color table Stroke color as {r, g, b, a} (optional)
---@param stroke_width number Stroke width (optional, default 1.0)
--
function drawCircle(x, y, radius, fill_color, stroke_color, stroke_width)
    _add_canvas_command({
        type = "DrawCanvasCircle",
        center = {x = x, y = y},
        radius = radius,
        fill_color = fill_color,
        stroke_color = stroke_color,
        stroke_width = stroke_width or 1.0
    })
end

---
-- Draws text on the canvas
---@param text string Text to draw
---@param x number X coordinate
---@param y number Y coordinate
---@param font_size number Font size (optional, default 16)
---@param color table Color as {r, g, b, a} (optional, default white)
---@param font_family string Font family (optional)
---@param alignment string Text alignment: "Left", "Center", "Right" (optional, default "Left")
--
function drawText(text, x, y, font_size, color, font_family, alignment)
    _add_canvas_command({
        type = "DrawCanvasText",
        text = tostring(text),
        position = {x = x, y = y},
        font_size = font_size or 16,
        color = color or {r = 1.0, g = 1.0, b = 1.0, a = 1.0},
        font_family = font_family,
        alignment = alignment or "Left"
    })
end

---
-- Draws an ellipse on the canvas
---@param x number Center X coordinate
---@param y number Center Y coordinate
---@param rx number X radius
---@param ry number Y radius
---@param fill_color table Fill color as {r, g, b, a} (optional)
---@param stroke_color table Stroke color as {r, g, b, a} (optional)
---@param stroke_width number Stroke width (optional, default 1.0)
--
function drawEllipse(x, y, rx, ry, fill_color, stroke_color, stroke_width)
    _add_canvas_command({
        type = "DrawCanvasEllipse",
        center = {x = x, y = y},
        rx = rx,
        ry = ry,
        fill_color = fill_color,
        stroke_color = stroke_color,
        stroke_width = stroke_width or 1.0
    })
end

---
-- Draws a polygon on the canvas
---@param points table Array of points as {{x, y}, {x, y}, ...}
---@param fill_color table Fill color as {r, g, b, a} (optional)
---@param stroke_color table Stroke color as {r, g, b, a} (optional)
---@param stroke_width number Stroke width (optional, default 1.0)
--
function drawPolygon(points, fill_color, stroke_color, stroke_width)
    _add_canvas_command({
        type = "DrawCanvasPolygon",
        points = points,
        fill_color = fill_color,
        stroke_color = stroke_color,
        stroke_width = stroke_width or 1.0
    })
end

---
-- Draws an image on the canvas
---@param source string Image source/path
---@param x number X coordinate
---@param y number Y coordinate
---@param width number Width to draw
---@param height number Height to draw
---@param opacity number Opacity (0-1, optional, default 1.0)
--
function drawImage(source, x, y, width, height, opacity)
    _add_canvas_command({
        type = "DrawCanvasImage",
        source = source,
        position = {x = x, y = y},
        size = {x = width, y = height},
        opacity = opacity or 1.0
    })
end

-- =============================================================================
--  3D Canvas Functions
-- =============================================================================

---
-- Begins 3D rendering mode with camera configuration
---@param camera_pos table Camera position as {x, y, z} (optional, default {0, 0, 10})
---@param camera_target table Camera target as {x, y, z} (optional, default {0, 0, 0})
---@param camera_up table Camera up vector as {x, y, z} (optional, default {0, 1, 0})
---@param fov number Field of view in degrees (optional, default 45.0)
---@param near_plane number Near clipping plane (optional, default 0.1)
---@param far_plane number Far clipping plane (optional, default 100.0)
--
function beginCanvas3D(camera_pos, camera_target, camera_up, fov, near_plane, far_plane)
    _add_canvas_command({
        type = "BeginCanvas3D",
        camera = {
            position = camera_pos or {x = 0, y = 0, z = 10},
            target = camera_target or {x = 0, y = 0, z = 0},
            up = camera_up or {x = 0, y = 1, z = 0},
            fov = fov or 45.0,
            near_plane = near_plane or 0.1,
            far_plane = far_plane or 100.0
        }
    })
end

---
-- Ends 3D rendering mode
--
function endCanvas3D()
    _add_canvas_command({
        type = "EndCanvas3D"
    })
end

---
-- Draws a 3D cube
---@param x number X position
---@param y number Y position
---@param z number Z position
---@param width number Width
---@param height number Height
---@param depth number Depth
---@param color table Color as {r, g, b, a} (optional, default white)
---@param wireframe boolean Draw as wireframe (optional, default false)
--
function drawCube3D(x, y, z, width, height, depth, color, wireframe)
    _add_canvas_command({
        type = "DrawCanvas3DCube",
        position = {x = x, y = y, z = z},
        size = {x = width, y = height, z = depth},
        color = color or {r = 1.0, g = 1.0, b = 1.0, a = 1.0},
        wireframe = wireframe or false
    })
end

---
-- Draws a 3D sphere
---@param x number X position
---@param y number Y position
---@param z number Z position
---@param radius number Sphere radius
---@param color table Color as {r, g, b, a} (optional, default white)
---@param wireframe boolean Draw as wireframe (optional, default false)
--
function drawSphere3D(x, y, z, radius, color, wireframe)
    _add_canvas_command({
        type = "DrawCanvas3DSphere",
        center = {x = x, y = y, z = z},
        radius = radius,
        color = color or {r = 1.0, g = 1.0, b = 1.0, a = 1.0},
        wireframe = wireframe or false
    })
end

---
-- Draws a 3D plane
---@param x number X position
---@param y number Y position
---@param z number Z position
---@param width number Width
---@param height number Height
---@param normal_x number Normal X component (optional, default 0)
---@param normal_y number Normal Y component (optional, default 1)
---@param normal_z number Normal Z component (optional, default 0)
---@param color table Color as {r, g, b, a} (optional, default white)
--
function drawPlane3D(x, y, z, width, height, normal_x, normal_y, normal_z, color)
    _add_canvas_command({
        type = "DrawCanvas3DPlane",
        center = {x = x, y = y, z = z},
        size = {x = width, y = height},
        normal = {x = normal_x or 0, y = normal_y or 1, z = normal_z or 0},
        color = color or {r = 1.0, g = 1.0, b = 1.0, a = 1.0}
    })
end

---
-- Draws a 3D mesh from vertices
---@param vertices table Array of vertex positions as {{x, y, z}, ...}
---@param indices table Array of triangle indices (optional)
---@param normals table Array of vertex normals (optional)
---@param uvs table Array of texture coordinates (optional)
---@param color table Color as {r, g, b, a} (optional, default white)
---@param texture string Texture source (optional)
--
function drawMesh3D(vertices, indices, normals, uvs, color, texture)
    _add_canvas_command({
        type = "DrawCanvas3DMesh",
        vertices = vertices,
        indices = indices or {},
        normals = normals or {},
        uvs = uvs or {},
        color = color or {r = 1.0, g = 1.0, b = 1.0, a = 1.0},
        texture = texture
    })
end

---
-- Sets 3D lighting configuration
---@param ambient_color table Ambient light color as {r, g, b, a} (optional, default dark gray)
---@param directional_light table Directional light as {direction = {x, y, z}, color = {r, g, b, a}, intensity = number} (optional)
---@param point_lights table Array of point lights as {{position = {x, y, z}, color = {r, g, b, a}, intensity = number, range = number}, ...} (optional)
--
function setLighting3D(ambient_color, directional_light, point_lights)
    _add_canvas_command({
        type = "SetCanvas3DLighting",
        ambient_color = ambient_color or {r = 0.1, g = 0.1, b = 0.1, a = 1.0},
        directional_light = directional_light,
        point_lights = point_lights or {}
    })
end

---
-- Applies a 3D transformation matrix
---@param transform_matrix table Transform matrix (simplified as translation, rotation, scale)
--
function applyTransform3D(transform_matrix)
    _add_canvas_command({
        type = "ApplyCanvas3DTransform",
        transform = transform_matrix
    })
end

---
-- Helper function to create transform matrices
---@param translation table Translation as {x, y, z} (optional, default {0, 0, 0})
---@param rotation table Rotation as {x, y, z} in degrees (optional, default {0, 0, 0})
---@param scale table Scale as {x, y, z} (optional, default {1, 1, 1})
---@return table Transform matrix
--
function createTransformMatrix(translation, rotation, scale)
    return {
        translation = translation or {x = 0, y = 0, z = 0},
        rotation = rotation or {x = 0, y = 0, z = 0},
        scale = scale or {x = 1, y = 1, z = 1}
    }
end

---
-- Draws a path on the canvas using SVG-like path data
---@param path_data string SVG path data (e.g., "M10,10 L20,20 Z")
---@param fill_color table Fill color as {r, g, b, a} (optional)
---@param stroke_color table Stroke color as {r, g, b, a} (optional)
---@param stroke_width number Stroke width (optional, default 1.0)
--
function drawPath(path_data, fill_color, stroke_color, stroke_width)
    _add_canvas_command({
        type = "DrawCanvasPath",
        path_data = path_data,
        fill_color = fill_color,
        stroke_color = stroke_color,
        stroke_width = stroke_width or 1.0
    })
end
