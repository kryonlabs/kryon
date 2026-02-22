-- Login Form Controller
-- Demonstrates form validation and submission

local txt_username = kryon.widget("txt_username")
local txt_password = kryon.widget("txt_password")
local chk_remember = kryon.widget("chk_remember")
local lbl_title = kryon.widget("lbl_title")

-- Login button handler
kryon.widget("btn_login").on_click(function()
    local username = txt_username.value
    local password = txt_password.value
    local remember = chk_remember.checked

    -- Validate inputs
    if username == "" or password == "" then
        lbl_title.text = "Please enter all fields!"
        return
    end

    -- Attempt login
    if login(username, password, remember) then
        lbl_title.text = "Login successful!"
        -- Navigate to main screen
        navigate_to("main_screen")
    else
        lbl_title.text = "Invalid credentials!"
    end
end)

-- Cancel button handler
kryon.widget("btn_cancel").on_click(function()
    txt_username.value = ""
    txt_password.value = ""
    chk_remember.checked = false
    lbl_title.text = "Welcome Back!"
end)

-- Start event loop
kryon.run()
