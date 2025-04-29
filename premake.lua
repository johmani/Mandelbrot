-------------------------------------------------------------------------------------
-- Args
-------------------------------------------------------------------------------------
newoption {
    trigger = "enginePath",
    description = "engine Path",
}

newoption {
    trigger = "includSourceCode",
    description = "includ Source Code",
}

HE = _OPTIONS["enginePath"]
includSourceCode = _OPTIONS["includSourceCode"]  == "true"
projectLocation = "%{wks.location}/Build/IDE"

include (HE .. "/build.lua")
print("[Engine] : " .. HE)

-------------------------------------------------------------------------------------
-- workspace
-------------------------------------------------------------------------------------
workspace "Mandelbrot"
    architecture "x86_64"
    configurations { "Debug", "Release", "Profile", "Dist" }
    startproject "Mandelbrot"
    flags
    {
      "MultiProcessorCompile",
    }

    IncludeHydraProject(includSourceCode)
    AddPlugins()

    group "Mandelbrot"
        project "Mandelbrot"
            kind "ConsoleApp"
            language "C++"
            cppdialect "C++latest"
            staticruntime "off"
            targetdir (binOutputDir)
            objdir (IntermediatesOutputDir)

            LinkHydraApp(includSourceCode)
            SetHydraFilters()

            files
            {
                "Source/**.h",
                "Source/**.cpp",
                "Source/**.cppm",
                "Source/**.hlsl",
                "*.lua",
            }

            includedirs
            {
                "Source",
            }

            links {
            
                "ImGui",
            }

            buildoptions {
            
                AddCppm("imgui"),
            }

            SetupShaders(
                { D3D12 = true, VULKAN = true },             -- api
                "%{prj.location}/Source/Shaders" ,           -- sourceDir
                "%{prj.location}/Source/Mandelbrot/Embeded", -- cacheDir
                "--header"                                   -- args
            )

    group ""
