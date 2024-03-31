workspace "Vulkan"
    architecture "x86_64"
    configurations { "Debug", "Release" }
    startproject "Engine"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Engine"
    location "Engine"
    kind "ConsoleApp"
    language "C++"
    staticruntime "on"
    cppdialect "C++17"

    targetdir ("Binaries/" .. outputdir .. "/%{prj.name}")
    objdir ("Intermediate/" .. outputdir .. "/%{prj.name}")

    files {
        "%{prj.name}/Source/**.h",
        "%{prj.name}/Source/**.cpp",
        "%{prj.name}/Assets/**",
        "%{prj.name}/Shaders/**",
    }

    includedirs {
        "%{prj.name}/Assets/",
        "%{prj.name}/Shaders/",
        "%{prj.name}/Source/",
        "$(VULKAN_SDK)/include/",
        "%{prj.name}/ThirdParty/",
        "%{prj.name}/ThirdParty/*/include",
    }

    libdirs {
        "$(VULKAN_SDK)/Lib/",
        "%{prj.name}/ThirdParty/SDL/lib",
    }

    links {
        "vulkan-1.lib",
    }

    filter "system:windows"
        systemversion "latest"
        postbuildcommands {
            ("{COPY} Assets/ ../Binaries/" .. outputdir .. "/Afterlife/Assets/"),
        }

    filter "configurations:Debug"
        defines { "_DEBUG" }
        runtime "Debug"
        symbols "On"

        links {
            "SDL2d.lib",
        }

    filter "configurations:Release"
        defines { "_RELEASE" }
        runtime "Release"
        optimize "On"

        links {
            "SDL2.lib",
        }