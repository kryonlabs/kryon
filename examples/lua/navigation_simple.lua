-- Simple Navigation Demo
-- Demonstrates basic navigation patterns in Lua

local kryon = require("../../../bindings/lua/init")

-- Navigation state
local currentPage = "home"
local pages = {
    home = {
        title = "Home",
        content = "Welcome to the home page! This is the main landing page of our application."
    },
    about = {
        title = "About",
        content = "This is the about page. Learn more about our application and its features."
    },
    contact = {
        title = "Contact",
        content = "Contact us page. You can reach out through various channels listed below."
    }
}

-- Navigation handlers
local function navigateTo(pageName)
    currentPage = pageName
    print("Navigated to: " .. pages[pageName].title)
    -- TODO: Trigger UI update when reactive system is implemented
end

local function showHome()
    navigateTo("home")
end

local function showAbout()
    navigateTo("about")
end

local function showContact()
    navigateTo("contact")
end

-- Build navigation bar
local function buildNavigationBar()
    return kryon.container({
        background = kryon.rgb(52, 58, 64),
        height = 60,
        padding = 20,

        children = {
            kryon.row({
                gap = 30,

                children = {
                    -- App title
                    kryon.text({
                        text = "Kryon App",
                        color = kryon.rgb(255, 255, 255),
                        font_size = 20
                    }),

                    -- Navigation buttons
                    kryon.row({
                        gap = 15,

                        children = {
                            kryon.button({
                                text = "Home",
                                background = currentPage == "home" and kryon.rgb(0, 123, 255) or kryon.rgb(108, 117, 125),
                                color = kryon.rgb(255, 255, 255),
                                width = 80,
                                height = 35,
                                on_click = showHome
                            }),

                            kryon.button({
                                text = "About",
                                background = currentPage == "about" and kryon.rgb(0, 123, 255) or kryon.rgb(108, 117, 125),
                                color = kryon.rgb(255, 255, 255),
                                width = 80,
                                height = 35,
                                on_click = showAbout
                            }),

                            kryon.button({
                                text = "Contact",
                                background = currentPage == "contact" and kryon.rgb(0, 123, 255) or kryon.rgb(108, 117, 125),
                                color = kryon.rgb(255, 255, 255),
                                width = 80,
                                height = 35,
                                on_click = showContact
                            })
                        }
                    })
                }
            })
        }
    })
end

-- Build page content
local function buildPageContent()
    local page = pages[currentPage]

    return kryon.column({
        gap = 30,
        padding = 40,

        children = {
            -- Page title
            kryon.text({
                text = page.title,
                color = kryon.rgb(33, 37, 41),
                font_size = 36
            }),

            -- Page content
            kryon.text({
                text = page.content,
                color = kryon.rgb(108, 117, 125),
                font_size = 18
            }),

            -- Page-specific content
            (function()
                if currentPage == "home" then
                    return kryon.column({
                        gap = 20,

                        children = {
                            kryon.text({
                                text = "Quick Links:",
                                color = kryon.rgb(33, 37, 41),
                                font_size = 20
                            }),

                            kryon.row({
                                gap = 15,

                                children = {
                                    kryon.button({
                                        text = "Learn More",
                                        background = kryon.rgb(23, 162, 184),
                                        color = kryon.rgb(255, 255, 255),
                                        width = 120,
                                        height = 40,
                                        on_click = showAbout
                                    }),

                                    kryon.button({
                                        text = "Get in Touch",
                                        background = kryon.rgb(40, 167, 69),
                                        color = kryon.rgb(255, 255, 255),
                                        width = 120,
                                        height = 40,
                                        on_click = showContact
                                    })
                                }
                            })
                        }
                    })
                elseif currentPage == "about" then
                    return kryon.column({
                        gap = 20,

                        children = {
                            kryon.text({
                                text = "Application Features:",
                                color = kryon.rgb(33, 37, 41),
                                font_size = 20
                            }),

                            kryon.column({
                                gap = 10,

                                children = {
                                    kryon.text({
                                        text = "• Cross-platform UI framework",
                                        color = kryon.rgb(108, 117, 125),
                                        font_size = 16
                                    }),

                                    kryon.text({
                                        text = "• Multiple frontend support (Nim, Lua)",
                                        color = kryon.rgb(108, 117, 125),
                                        font_size = 16
                                    }),

                                    kryon.text({
                                        text = "• C core for performance",
                                        color = kryon.rgb(108, 117, 125),
                                        font_size = 16
                                    })
                                }
                            })
                        }
                    })
                elseif currentPage == "contact" then
                    return kryon.column({
                        gap = 20,

                        children = {
                            kryon.text({
                                text = "Contact Information:",
                                color = kryon.rgb(33, 37, 41),
                                font_size = 20
                            }),

                            kryon.column({
                                gap = 10,

                                children = {
                                    kryon.text({
                                        text = "Email: info@kryon.dev",
                                        color = kryon.rgb(108, 117, 125),
                                        font_size = 16
                                    }),

                                    kryon.text({
                                        text = "GitHub: github.com/kryon",
                                        color = kryon.rgb(108, 117, 125),
                                        font_size = 16
                                    }),

                                    kryon.text({
                                        text = "Documentation: docs.kryon.dev",
                                        color = kryon.rgb(108, 117, 125),
                                        font_size = 16
                                    })
                                }
                            }),

                            kryon.input({
                                placeholder = "Enter your message...",
                                width = 300,
                                height = 40,
                                background = kryon.rgb(255, 255, 255),
                                border = { color = kryon.rgb(206, 212, 218), width = 1 },
                                padding = 10
                            })
                        }
                    })
                end
            end)()
        }
    })
end

-- Define the UI
local app = kryon.app({
    width = 800,
    height = 600,
    title = "Simple Navigation Demo",

    body = kryon.column({
        background = kryon.rgb(248, 249, 250),

        children = {
            -- Navigation bar
            buildNavigationBar(),

            -- Page content area
            kryon.container({
                background = kryon.rgb(255, 255, 255),
                height = 540,

                children = {
                    buildPageContent()
                }
            })
        }
    })
})

-- TODO: Run the application when runtime is implemented
-- app:run()

print("Simple Navigation Lua example created successfully!")
print("Window: 800x600, Title: 'Simple Navigation Demo'")
print("Features: Navigation bar with Home, About, Contact pages")
print("Current page: " .. pages[currentPage].title)
print("Navigation state: currentPage = '" .. currentPage .. "'")
print("Pages available: " .. table.concat({"home", "about", "contact"}, ", "))